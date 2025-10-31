#include "DebugRenderer.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include "Content/ContentManagement.h"
#include "Content/ContentUtils.h"
#include "Graphics/D3D12/D3D12Surface.h"
#include "Graphics/D3D12/D3D12Camera.h"
#include "Graphics/D3D12/D3D12GPass.h" // TODO: store the formats in some separate header
#include "Graphics/D3D12/D3D12Primitives.h"

//FIXME: TESTING
#include "ECS/Scene.h"
#include "ECS/Transform.h"
#include "ECS/Entity.h"
#include "EngineAPI/ECS/SceneAPI.h"
#include "ECS/QueryView.h"
#include <Jolt/Geometry/OrientedBox.h>
#include "Physics/PhysicsCore.h"
#include "Physics/BodyInterface.h"

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


	stream.rootSignature = _triangleRootSig;

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
DebugRenderer::DrawGeometry(JPH::RMat44Arg modelMatrix, const JPH::AABox& worldSpaceBounds, f32 LODScaleSq, JPH::ColorArg modelColor, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode)
{
}

DebugRenderer::Batch
DebugRenderer::CreateTriangleBatch(const Triangle* triangles, s32 triangleCount)
{
	return Batch();
}

DebugRenderer::Batch
DebugRenderer::CreateTriangleBatch(const Vertex* vertices, s32 vertexCount, const u32* indices, s32 indexCount)
{
	return Batch();
}

void
DebugRenderer::Draw(const D3D12FrameInfo& frameInfo)
{
	DrawLines(frameInfo);
}

void
DebugRenderer::Clear()
{
	ClearLines();
}

void 
DebugRenderer::DrawLines(const D3D12FrameInfo& frameInfo)
{
	std::lock_guard lock{ _linesMutex };
	if (!_lines.empty())
	{
		u32 idx(primitives::AddPrimitive(primitives::Primitive{ primitives::Primitive::Topology::Line }));
		primitives::CreateVertexBuffer(idx, sizeof(Line) / 2, _lines.size() * 2, &_lines[0]);
		_linesPrimitivesIndices.emplace_back(idx);
		DXGraphicsCommandList* const cmdList{ core::GraphicsCommandList() };

		VSConstants* constants = core::CBuffer().AllocateSpace<VSConstants>();
		DirectX::XMStoreFloat4x4A(&constants->View, frameInfo.Camera->View());
		DirectX::XMStoreFloat4x4A(&constants->Projection, frameInfo.Camera->Projection());
		
		cmdList->SetGraphicsRootSignature(_lineRootSig);
		cmdList->SetPipelineState(_linePSO);
		cmdList->SetGraphicsRootConstantBufferView(RootParameterIndices::VSConstants, core::CBuffer().GpuAddress(constants));
		primitives::DrawPrimitives();
	}
}

void
DebugRenderer::ClearLines()
{
	std::lock_guard lock{ _linesMutex };
	if (!_lines.empty())
	{
		primitives::ReleaseVertexBuffer(_linesPrimitivesIndices[0]);
	}
	_lines.clear();
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
	for (auto [entity, s, lt, wt, col] : ecs::scene::GetRW<ecs::component::StaticObject, ecs::component::LocalTransform, ecs::component::WorldTransform, ecs::component::Collider>())
	{
		/*JPH::OrientedBox box{};
		box.mHalfExtents = JPH::Vec3(0.5f, 0.5f, 0.5f);
		box.mOrientation;*/
		auto shape{ physics::core::PhysicsSystem().GetBodyInterface().GetShape(col.BodyID).GetPtr() };
		JPH::AABox box{ shape->GetLocalBounds() };
		JPH::Color color{ JPH::Color{ JPH::Color::sRed } };
		_renderer->DrawWireBox(JPH::RMat44::sRotationTranslation(lt.Rotation, lt.Position.AsJPVec3()) * JPH::Mat44::sScale(JPH::Vec3(1.f, 1.f, 1.f)),
			shape->GetLocalBounds(), color);
	}
	//JPH::PhysicsSystem& physicsSystem{mofu::physics::core::}
	JPH::BodyManager::DrawSettings settings{};
	_renderer->Draw(frameInfo);
	/*physicsSystem.DrawBodies(settings, _renderer, nullptr);
	physicsSystem.DrawConstraints(_renderer);
	physicsSystem.DrawConstraintLimits(_renderer);
	physicsSystem.DrawConstraintReferenceFrame(_renderer);*/
}

void
Clear()
{
	_renderer->Clear();
}

}