#include "DebugRenderer.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include "Content/ContentManagement.h"
#include "Content/ContentUtils.h"
#include "Graphics/D3D12/D3D12Surface.h"
#include "Graphics/D3D12/D3D12Camera.h"
#include "Graphics/D3D12/D3D12GPass.h" // TODO: store the formats in some separate header
#include "Graphics/D3D12/D3D12Primitives.h"
#include "Graphics/RenderingDebug.h"

//FIXME: TESTING
#include "ECS/Scene.h"
#include "ECS/Transform.h"
#include "ECS/ComponentRegistry.h"
#include "ECS/Entity.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"
#include <Jolt/Geometry/OrientedBox.h>
#include "Physics/PhysicsCore.h"
#include "Physics/BodyInterface.h"
#include "Graphics/D3D12/D3D12Upload.h"

namespace mofu::graphics::d3d12::debug {
namespace {
DebugRenderer* _renderer;

struct RootParameterIndices
{
	enum : u8
	{
		VSConstants,
		Count
	};
};

mofu::shaders::CompiledShaderPtr _shaders[shaders::physics::DebugShaders::Count]{};
std::unique_ptr<u8[]> _shadersBlob;

struct VSConstants
{
	m4x4a View;
	m4x4a Projection;
};

void 
LoadShaders()
{
	assert(!_shadersBlob);

	u64 totalSize{ 0 };
	bool result{ mofu::content::ReadFileToByteBuffer(shaders::physics::SHADERS_BIN_PATHS[0], _shadersBlob, totalSize)};
	assert(result);
	assert(_shadersBlob && totalSize != 0);

	u64 offset{ 0 };
	u32 index{ 0 };
	while (offset < totalSize)
	{
		mofu::shaders::CompiledShaderPtr& shader{ _shaders[index] };
		assert(!shader);
		result &= (index < shaders::physics::DebugShaders::Count && !shader);
		if (!result) break;

		shader = reinterpret_cast<const mofu::shaders::CompiledShaderPtr>(&_shadersBlob[offset]);
		offset += shader->GetBufferSize();
		++index;
	}
	assert(index == shaders::physics::DebugShaders::Count && offset == totalSize);

	/*u64 totalSize{ 0 };
	bool result{ content::LoadPhysicsDebugShaders(_shadersBlobs, totalSize) };
	assert(result);
	assert(_shadersBlobs.size() == shaders::physics::DebugShaders::Count && totalSize != 0);

	for (u32 i{ 0 }; i < shaders::physics::DebugShaders::Count; ++i)
	{
		mofu::shaders::CompiledShaderPtr& shader{ _shaders[i] };
		assert(!shader);

		shader = reinterpret_cast<const mofu::shaders::CompiledShaderPtr>(_shadersBlobs[i].get());
	}*/
}

D3D12_SHADER_BYTECODE
GetShader(shaders::physics::DebugShaders::ID id)
{
	assert(id < shaders::physics::DebugShaders::Count);
	const mofu::shaders::CompiledShaderPtr shader{ _shaders[id] };
	assert(shader && shader->BytecodeSize());
	return { shader->Bytecode(), shader->BytecodeSize() };
}

} // anonymous namespace

DebugRenderer::DebugRenderer()
{
	LoadShaders();

	CreatePSOs();
}

void 
DebugRenderer::CreatePSOs()
{
	// Line
	d3dx::D3D12DescriptorRange range
	{
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND, 0, 0,
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
	};

	D3D12_INPUT_ELEMENT_DESC inputElements[]{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA }
	};
	D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements, _countof(inputElements) };

	d3dx::D3D12RootParameter parameters[RootParameterIndices::Count]{};
	parameters[RootParameterIndices::VSConstants].AsCBV(D3D12_SHADER_VISIBILITY_VERTEX, 0);

	d3dx::D3D12RootSignatureDesc rootSigDesc{ &parameters[0], _countof(parameters), d3dx::D3D12RootSignatureDesc::DEFAULT_FLAGS, nullptr, 0 };

	rootSigDesc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	_lineRootSig = rootSigDesc.Create();
	assert(_lineRootSig);
	NAME_D3D12_OBJECT(_lineRootSig, L"Physics Debug: Line Root Signature");

	D3D12_RT_FORMAT_ARRAY rtfArray{};
	rtfArray.NumRenderTargets = 1;
	rtfArray.RTFormats[0] = D3D12Surface::DEFAULT_BACK_BUFFER_FORMAT;

	constexpr u64 alignedStreamSize{ math::AlignUp<sizeof(u64)>(sizeof(d3dx::D3D12PipelineStateSubobjectStream)) };
	u8* const streamPtr{ (u8*)_alloca(alignedStreamSize) };
	ZeroMemory(streamPtr, alignedStreamSize);
	new (streamPtr) d3dx::D3D12PipelineStateSubobjectStream{};
	d3dx::D3D12PipelineStateSubobjectStream& stream{ *(d3dx::D3D12PipelineStateSubobjectStream* const)streamPtr };
	stream.rootSignature = _lineRootSig;
	stream.vs = GetShader(shaders::physics::DebugShaders::LineVS);
	stream.ps = GetShader(shaders::physics::DebugShaders::LinePS);
	stream.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	stream.renderTargetFormats = rtfArray;
	stream.rasterizer = d3dx::RasterizerState.BACKFACE_CULLING;
	stream.blend = d3dx::BlendState.ALPHA_BLEND;
	stream.inputLayout = inputLayout;
	stream.depthStencil = d3dx::DepthState.REVERSED_READONLY;
	stream.depthStencilFormat = gpass::DEPTH_BUFFER_FORMAT;

	_linePSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(_linePSO, L"Physics Debug: Line PSO");

	_triangleRootSig = rootSigDesc.Create();
	assert(_triangleRootSig);
	NAME_D3D12_OBJECT(_triangleRootSig, L"Physics Debug: Triangle Root Signature");
	stream.rootSignature = _triangleRootSig;
	stream.vs = GetShader(shaders::physics::DebugShaders::TriangleVS);
	stream.ps = GetShader(shaders::physics::DebugShaders::TrianglePS);
	stream.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stream.renderTargetFormats = rtfArray;
	stream.rasterizer = d3dx::RasterizerState.BACKFACE_CULLING;
	stream.blend = d3dx::BlendState.ALPHA_BLEND;
	stream.inputLayout = inputLayout;
	stream.depthStencil = d3dx::DepthState.REVERSED_READONLY;
	stream.depthStencilFormat = gpass::DEPTH_BUFFER_FORMAT;
	_trianglePSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(_linePSO, L"Physics Debug: Triangle PSO");

	_triangleWireframeRootSig = rootSigDesc.Create();
	assert(_triangleWireframeRootSig);
	NAME_D3D12_OBJECT(_triangleWireframeRootSig, L"Physics Debug: Triangle Wireframe Root Signature");
	stream.rootSignature = _triangleWireframeRootSig;
	stream.vs = GetShader(shaders::physics::DebugShaders::TriangleVS);
	stream.ps = GetShader(shaders::physics::DebugShaders::TrianglePS);
	stream.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stream.renderTargetFormats = rtfArray;
	stream.rasterizer = d3dx::RasterizerState.WIREFRAME;
	stream.blend = d3dx::BlendState.ALPHA_BLEND;
	stream.inputLayout = inputLayout;
	stream.depthStencil = d3dx::DepthState.REVERSED_READONLY;
	stream.depthStencilFormat = gpass::DEPTH_BUFFER_FORMAT;
	_triangleWireframePSO = d3dx::CreatePipelineState(&stream, sizeof(stream));
	NAME_D3D12_OBJECT(_linePSO, L"Physics Debug: Triangle Wireframe PSO");

	JPH::DebugRenderer::Initialize();
}

void
DebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
	Line line;
	JPH::Vec3(inFrom).StoreFloat3(&line.From);
	line.FromColor = inColor;
	JPH::Vec3(inTo).StoreFloat3(&line.To);
	line.ToColor = inColor;

	std::lock_guard lock{ _linesMutex };
	_lines.emplace_back(line);
}

void 
DebugRenderer::DrawTriangle(JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color, ECastShadow castShadow)
{
	
}

void 
DebugRenderer::DrawText3D(JPH::RVec3Arg position, const std::string_view& string, JPH::ColorArg color, f32 height)
{
}

void 
DebugRenderer::DrawGeometry(JPH::RMat44Arg modelMatrix, const JPH::AABox& worldSpaceBounds, f32 LODScaleSq, JPH::ColorArg modelColor, 
	const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode)
{
	JPH::Mat44 modelMtx{ modelMatrix.ToMat44() };

	if (drawMode == EDrawMode::Wireframe)
	{
		_wireframePrimitives[geometry].Instances.push_back({ modelMtx, modelMtx.GetDirectionPreservingMatrix(), 
			modelColor, worldSpaceBounds, LODScaleSq });
		++_instanceCount;
	}
	else
	{
		assert(false);
	}
}

DebugRenderer::Batch
DebugRenderer::CreateTriangleBatch(const Triangle* triangles, s32 triangleCount)
{
	if (triangles == nullptr || triangleCount == 0)
		return _emptyBatch;

	const primitives::Primitive::Topology topology{ primitives::Primitive::Topology::Triangle };
	u32 primitiveIdx{ primitives::AddPrimitive(topology) };
	primitives::CreateVertexBuffer(topology, primitiveIdx, sizeof(Vertex), 3 * triangleCount, (void*)triangles);

	return primitives::GetPrimitive(topology, primitiveIdx);
}

DebugRenderer::Batch
DebugRenderer::CreateTriangleBatch(const Vertex* vertices, s32 vertexCount, const u32* indices, s32 indexCount)
{
	if (vertices == nullptr || vertexCount == 0)
		return _emptyBatch;

	const primitives::Primitive::Topology topology{ primitives::Primitive::Topology::Triangle };
	u32 primitiveIdx{ primitives::AddPrimitive(topology) };
	primitives::CreateVertexBuffer(topology, primitiveIdx, sizeof(Vertex), vertexCount, (void*)vertices);
	primitives::CreateIndexBuffer(topology, primitiveIdx, indexCount, (void*)indices);

	DebugRenderer::Batch batch{ primitives::GetPrimitive(topology, primitiveIdx) };

	return primitives::GetPrimitive(topology, primitiveIdx);
}

void
DebugRenderer::Draw(const D3D12FrameInfo& frameInfo)
{
	DrawLines(frameInfo);
	DrawTriangles(frameInfo);
	DrawTextBuffer(frameInfo);
}

void
DebugRenderer::Clear()
{
	ClearLines();
	ClearTriangles();
	ClearTextBuffer();
	NextFrame();
}

void 
DebugRenderer::DrawLines(const D3D12FrameInfo& frameInfo)
{
	std::lock_guard lock{ _linesMutex };
	if (!_lines.empty())
	{
		u32 idx{ primitives::AddPrimitive(primitives::Primitive::Topology::Line) };
		primitives::CreateVertexBuffer(primitives::Primitive::Topology::Line, idx, sizeof(Line) / 2, _lines.size() * 2, &_lines[0]);
		_linesPrimitivesIndex = idx;
		DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };

		VSConstants* constants = core::CBuffer().AllocateSpace<VSConstants>();
		DirectX::XMStoreFloat4x4A(&constants->View, frameInfo.Camera->View());
		DirectX::XMStoreFloat4x4A(&constants->Projection, frameInfo.Camera->Projection());
		
		cmdList->SetGraphicsRootSignature(_lineRootSig);
		cmdList->SetPipelineState(_linePSO);
		cmdList->SetGraphicsRootConstantBufferView(RootParameterIndices::VSConstants, core::CBuffer().GpuAddress(constants));
		primitives::DrawPrimitives(primitives::Primitive::Topology::Line);
	}
}

void
DebugRenderer::DrawInstances(const Geometry* geometry, const InstanceBatch& instanceBatch, 
	const D3D12FrameInfo& frameInfo, DXGraphicsCommandList* const cmdList)
{
	primitives::D3D12Instance* const instanceBuffer{ primitives::GetInstanceBuffer() };

	if (!instanceBatch.Instances.empty())
	{
		const JPH::Array<LOD>& geometryLODs{ geometry->mLODs };

		u32 nextStartIdx{ instanceBatch.GeometryStartIndex.front() };
		for (u32 lod{ 0 }; lod < geometryLODs.size(); ++lod)
		{
			const u32 startIdx{ nextStartIdx } ;
			nextStartIdx = instanceBatch.GeometryStartIndex[lod + 1];
			u32 instanceCount{ nextStartIdx - startIdx };
			if (instanceCount == 0) continue;

			primitives::Primitive* primitives{ static_cast<primitives::Primitive*>(geometryLODs[lod].mTriangleBatch.GetPtr()) };
			cmdList->IASetPrimitiveTopology(primitives->PrimitiveTopology == primitives::Primitive::Topology::Line 
				? D3D_PRIMITIVE_TOPOLOGY_LINELIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			D3D12_VERTEX_BUFFER_VIEW vb[2]{};
			vb[0].BufferLocation = primitives->VertexBuffer->GetGPUVirtualAddress();
			vb[0].SizeInBytes = primitives->VertexSize * primitives->VertexCount;
			vb[0].StrideInBytes = primitives->VertexSize;

			vb[1].BufferLocation = instanceBuffer->InstanceBuffer->GetGPUVirtualAddress();
			vb[1].SizeInBytes = instanceBuffer->InstanceBufferSize;
			vb[1].StrideInBytes = instanceBuffer->InstanceSize;

			cmdList->IASetVertexBuffers(0, 2, &vb[0]);
			if (!primitives->IndexBuffer)
			{
				cmdList->DrawInstanced(primitives->NumVertexToDraw, instanceCount, 0, startIdx);
			}
			else
			{
				D3D12_INDEX_BUFFER_VIEW ib_view;
				ib_view.BufferLocation = primitives->IndexBuffer->GetGPUVirtualAddress();
				ib_view.SizeInBytes = primitives->NumIndexToDraw * sizeof(u32);
				ib_view.Format = DXGI_FORMAT_R32_UINT;
				cmdList->IASetIndexBuffer(&ib_view);

				cmdList->DrawIndexedInstanced(primitives->NumVertexToDraw, instanceCount, 0, 0, startIdx);
			}
		}
	}
}

void
DebugRenderer::DrawTriangles(const D3D12FrameInfo& frameInfo)
{
	DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };
	const ecs::component::LocalTransform& camLT{ ecs::scene::GetComponent<ecs::component::LocalTransform>(ecs::scene::GetSingletonEntity(ecs::component::ID<ecs::component::Camera>)) };
	JPH::Vec3 camPos{ camLT.Position.Vec3() };

	if (_instanceCount > 0)
	{
		primitives::D3D12Instance* const instanceBuffer{ primitives::GetInstanceBuffer() };
		primitives::CreateInstanceBuffer(2 * _instanceCount, sizeof(Instance), nullptr);
		u32 size{ (2 * _instanceCount) * sizeof(Instance) };
		upload::D3D12UploadContext context{ size };
		Instance* dstInstance{ reinterpret_cast<Instance*>(context.CpuAddress()) };

		for (InstanceMap::value_type& v : _wireframePrimitives)
		{
			const JPH::Array<LOD>& geometryLODs{ v.first->mLODs };
			const u32 lodCount{ (u32)geometryLODs.size() };

			Vec<Vec<u32>> lodIndices{ lodCount };

			const Vec<Instance>& instances{ v.second.Instances };
			for (u32 i{ 0 }; i < instances.size(); ++i)
			{
				const Instance& srcInstance{ instances[i] };

				// TODO: do culling
				const LOD& lod{ v.first->GetLOD(camPos, srcInstance.WorldSpaceBounds, srcInstance.LODScaleSq) };
				u32 lodIdx{ (u32)(&lod - geometryLODs.data()) };

				lodIndices[lodIdx].emplace_back(i);
			}

			Vec<u32>& geometryStartIndices{ v.second.GeometryStartIndex };
			geometryStartIndices.resize(lodCount + 1);
			u32 dstIdx{ 0 };
			for (u32 lod{ 0 }; lod < lodCount; ++lod)
			{
				geometryStartIndices[lod] = dstIdx;
				Vec<u32>& thisLodIndices{ lodIndices[lod] };
				for (u32 idx : thisLodIndices)
				{
					const Instance& srcInstance{ instances[idx] };
					dstInstance[dstIdx++] = srcInstance;
				}
				thisLodIndices.clear();
			}
			// write out the end of last LOD
			geometryStartIndices.back() = dstIdx;
		}

		context.CommandList()->CopyResource(instanceBuffer->InstanceBuffer, context.UploadBuffer());
		context.EndUpload();
	}

	if (_instanceCount > 0 && !_solidPrimitives.empty() || !_tempPrimitives.empty())
	{
		cmdList->SetGraphicsRootSignature(_triangleRootSig);
		cmdList->SetPipelineState(_trianglePSO);

		assert(false);
	}

	if (!_backfacingPrimitives.empty())
	{

	}

	if (!_wireframePrimitives.empty())
	{
		DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };
		VSConstants* constants = core::CBuffer().AllocateSpace<VSConstants>();
		DirectX::XMStoreFloat4x4A(&constants->View, frameInfo.Camera->View());
		DirectX::XMStoreFloat4x4A(&constants->Projection, frameInfo.Camera->Projection());

		cmdList->SetGraphicsRootSignature(_triangleWireframeRootSig);
		cmdList->SetPipelineState(_triangleWireframePSO);
		cmdList->SetGraphicsRootConstantBufferView(RootParameterIndices::VSConstants, core::CBuffer().GpuAddress(constants));

		for (InstanceMap::value_type& p : _wireframePrimitives)
			DrawInstances(p.first, p.second, frameInfo, cmdList);
	}
}

void 
DebugRenderer::DrawTextBuffer(const D3D12FrameInfo& frameInfo)
{
	std::lock_guard lock{ _textMutex };
	_textArray.clear();
}

void
DebugRenderer::ClearLines()
{
	std::lock_guard lock{ _linesMutex };
	if(_linesPrimitivesIndex != U32_INVALID_ID) primitives::RemovePrimitive(primitives::Primitive::Topology::Line, _linesPrimitivesIndex);
	_lines.clear();
}

void
DebugRenderer::ClearTriangles()
{
	std::lock_guard lock{ _primitivesMutex };

	Vec<GeometryRef> geoToDelete{};
	for (InstanceMap::value_type& v : _wireframePrimitives)
	{
		if (v.second.Instances.empty()) geoToDelete.emplace_back(v.first);
		else v.second.Instances.clear();
	}
	for (GeometryRef& g : geoToDelete) _wireframePrimitives.erase(g);

	_tempPrimitives.clear();
	_instanceCount = 0;
}

void 
DebugRenderer::ClearTextBuffer()
{
	std::lock_guard lock(_textMutex);
	_textArray.clear();
}

void
Initialize()
{
	_renderer = new DebugRenderer{};
}

void 
Shutdown()
{
	delete _renderer;
}

void 
Render(const D3D12FrameInfo& frameInfo)
{
	//for (auto [entity, s, lt, wt, col] : ecs::scene::GetRW<ecs::component::StaticObject, ecs::component::LocalTransform, ecs::component::WorldTransform, ecs::component::Collider>())
	//{
	//	/*JPH::OrientedBox box{};
	//	box.mHalfExtents = JPH::Vec3(0.5f, 0.5f, 0.5f);
	//	box.mOrientation;*/
	//	auto shape{ physics::core::PhysicsSystem().GetBodyInterface().GetShape(col.BodyID).GetPtr() };
	//	JPH::AABox box{ shape->GetLocalBounds() };
	//	JPH::Color color{ JPH::Color{ JPH::Color::sRed } };
	//	_renderer->DrawWireBox(JPH::RMat44::sRotationTranslation(lt.Rotation, lt.Position.AsJPVec3()) * JPH::Mat44::sScale(JPH::Vec3(1.f, 1.f, 1.f)),
	//		shape->GetLocalBounds(), color);
	//}

	for (auto [entity, s, lt, wt, col] : ecs::scene::GetRW<ecs::component::StaticObject, ecs::component::LocalTransform, 
		ecs::component::WorldTransform, ecs::component::Collider>())
	{
		JPH::BodyLockRead lock{ physics::core::PhysicsSystem().GetBodyLockInterface(), col.BodyID };

		//graphics::d3d12::debug::DrawBodyShape(lock.GetBody().GetShape(), physics::GetCenterOfMassTransform(lt.Position.Vec3(), lt.Rotation, lock.GetBody().GetShape()), lt.Scale.Vec3());
		//JPH::StaticCast<JPH::MeshShape>(lock.GetBody().GetShape())->Draw(_renderer, physics::GetCenterOfMassTransform(lt.Position.Vec3(), lt.Rotation, lock.GetBody().GetShape()), lt.Scale.Vec3(), JPH::Color::sCyan, false, true);
		lock.GetBody().GetShape()->Draw(_renderer, physics::GetCenterOfMassTransform(lt.Position.Vec3(), lt.Rotation, lock.GetBody().GetShape()), lt.Scale.Vec3(), JPH::Color::sCyan, false, true);
	}
	

	JPH::PhysicsSystem& physicsSystem{ mofu::physics::core::PhysicsSystem() };
	JPH::BodyManager::DrawSettings settings{};
	if(graphics::debug::RenderingSettings.DrawPhysicsWorldBounds) _renderer->DrawWireBox(physicsSystem.GetBounds(), JPH::Color::sGreen);
	/*physicsSystem.DrawBodies(settings, _renderer, nullptr);
	physicsSystem.DrawConstraints(_renderer);
	physicsSystem.DrawConstraintLimits(_renderer);
	physicsSystem.DrawConstraintReferenceFrame(_renderer);*/
	_renderer->Draw(frameInfo);
}

void
Clear()
{
	_renderer->Clear();
}

void DrawBodyShape(const JPH::Shape* shape, JPH::RMat44Arg centerOfMassTransform, JPH::Vec3 scale, bool wireframe)
{
	shape->Draw(_renderer, centerOfMassTransform, scale, JPH::Color::sCyan, false, wireframe);
}

DebugRenderer* const GetDebugRenderer() { return _renderer; }

}