#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12::d3dx {

constexpr struct
{
    const D3D12_HEAP_PROPERTIES DEFAULT_HEAP
    {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    const D3D12_HEAP_PROPERTIES UPLOAD_HEAP
    {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };
} HeapProperties;

constexpr struct
{
    const D3D12_RASTERIZER_DESC NO_CULLING
    {
        D3D12_FILL_MODE_SOLID,                      // FillMode
        D3D12_CULL_MODE_NONE,                       // CullMode
        1,                                          // FrontCounterClockwise
        0,                                          // DepthBias
        0,                                          // DepthBiasClamp
        0,                                          // SlopeScaledDepthBias
        1,                                          // DepthClipEnable
        0,                                          // MultisampleEnable
        0,                                          // AntialiasedLineEnable
        0,                                          // ForcesSampleCount
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF   // ConservativeRasterization
    };

    const D3D12_RASTERIZER_DESC BACKFACE_CULLING
    {
        D3D12_FILL_MODE_SOLID,                      // FillMode
        D3D12_CULL_MODE_BACK,                       // CullMode
        1,                                          // FrontCounterClockwise
        0,                                          // DepthBias
        0,                                          // DepthBiasClamp
        0,                                          // SlopeScaledDepthBias
        1,                                          // DepthClipEnable
        0,                                          // MultisampleEnable
        0,                                          // AntialiasedLineEnable
        0,                                          // ForcesSampleCount
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF   // ConservativeRasterization
    };

    const D3D12_RASTERIZER_DESC FRONTFACE_CULLING
    {
        D3D12_FILL_MODE_SOLID,                      // FillMode
        D3D12_CULL_MODE_FRONT,                      // CullMode
        1,                                          // FrontCounterClockwise
        0,                                          // DepthBias
        0,                                          // DepthBiasClamp
        0,                                          // SlopeScaledDepthBias
        1,                                          // DepthClipEnable
        0,                                          // MultisampleEnable
        0,                                          // AntialiasedLineEnable
        0,                                          // ForcesSampleCount
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF   // ConservativeRasterization
    };

    const D3D12_RASTERIZER_DESC WIREFRAME
    {
        D3D12_FILL_MODE_WIREFRAME,                  // FillMode
        D3D12_CULL_MODE_NONE,                       // CullMode
        1,                                          // FrontCounterClockwise
        0,                                          // DepthBias
        0,                                          // DepthBiasClamp
        0,                                          // SlopeScaledDepthBias
        1,                                          // DepthClipEnable
        0,                                          // MultisampleEnable
        0,                                          // AntialiasedLineEnable
        0,                                          // ForcesSampleCount
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF   // ConservativeRasterization
    };
} RasterizerState;

constexpr struct
{
    const D3D12_DEPTH_STENCIL_DESC1 DISABLED
    {
        0,                                          // DepthEnable
        D3D12_DEPTH_WRITE_MASK_ZERO,                // DepthWriteMask
        D3D12_COMPARISON_FUNC_LESS_EQUAL,           // DepthFunc
        0,                                          // StencilEnable
        0,                                          // StencilReadMask
        0,                                          // StencilWriteMask
        {},                                         // FrontFace
        {},                                         // BackFace
        0                                           // DepthBoundsTestEnable
    };

    const D3D12_DEPTH_STENCIL_DESC1 ENABLED
    {
        1,                                          // DepthEnable
        D3D12_DEPTH_WRITE_MASK_ALL,                 // DepthWriteMask
        D3D12_COMPARISON_FUNC_LESS_EQUAL,           // DepthFunc
        0,                                          // StencilEnable
        0,                                          // StencilReadMask
        0,                                          // StencilWriteMask
        {},                                         // FrontFace
        {},                                         // BackFace
        0                                           // DepthBoundsTestEnable
    };

    const D3D12_DEPTH_STENCIL_DESC1 ENABLED_READONLY
    {
        1,                                          // DepthEnable
        D3D12_DEPTH_WRITE_MASK_ZERO,                // DepthWriteMask
        D3D12_COMPARISON_FUNC_LESS_EQUAL,           // DepthFunc
        0,                                          // StencilEnable
        0,                                          // StencilReadMask
        0,                                          // StencilWriteMask
        {},                                         // FrontFace
        {},                                         // BackFace
        0                                           // DepthBoundsTestEnable
    };

    const D3D12_DEPTH_STENCIL_DESC1 REVERSED
    {
        1,                                          // DepthEnable
        D3D12_DEPTH_WRITE_MASK_ALL,                 // DepthWriteMask
        D3D12_COMPARISON_FUNC_GREATER_EQUAL,        // DepthFunc
        0,                                          // StencilEnable
        0,                                          // StencilReadMask
        0,                                          // StencilWriteMask
        {},                                         // FrontFace
        {},                                         // BackFace
        0                                           // DepthBoundsTestEnable
    };

    const D3D12_DEPTH_STENCIL_DESC1 REVERSED_READONLY
    {
        1,                                          // DepthEnable
        D3D12_DEPTH_WRITE_MASK_ZERO,                // DepthWriteMask
        D3D12_COMPARISON_FUNC_GREATER_EQUAL,        // DepthFunc
        0,                                          // StencilEnable
        0,                                          // StencilReadMask
        0,                                          // StencilWriteMask
        {},                                         // FrontFace
        {},                                         // BackFace
        0                                           // DepthBoundsTestEnable
    };
} DepthState;

constexpr struct
{
    const D3D12_BLEND_DESC DISABLED
    {
        0,                                          // AlphaToCoverageEnable
        0,                                          // IndependentBlendState
        {                                           // D3D12_RENDER_TARGET_BLEND_DESC
            {
                0,                                  // BlendEnable;
                0,                                  // LogicOpEnable;
                D3D12_BLEND_SRC_ALPHA,              // SrcBlend;
                D3D12_BLEND_INV_SRC_ALPHA,          // DestBlend;
                D3D12_BLEND_OP_ADD,                 // BlendOp;
                D3D12_BLEND_ONE,                    // SrcBlendAlpha;
                D3D12_BLEND_ONE,                    // DestBlendAlpha;
                D3D12_BLEND_OP_ADD,                 // BlendOpAlpha;
                D3D12_LOGIC_OP_NOOP,                // LogicOp
                D3D12_COLOR_WRITE_ENABLE_ALL        // RenderTargetWriteMask;
            },
            {},{},{},{},{},{},{}
        }
    };

    const D3D12_BLEND_DESC ALPHA_BLEND
    {
        0,                                          // AlphaToCoverageEnable
        0,                                          // IndependentBlendState
        {                                           // D3D12_RENDER_TARGET_BLEND_DESC
            {
                1,                                  // BlendEnable;
                0,                                  // LogicOpEnable;
                D3D12_BLEND_SRC_ALPHA,              // SrcBlend;
                D3D12_BLEND_INV_SRC_ALPHA,          // DestBlend;
                D3D12_BLEND_OP_ADD,                 // BlendOp;
                D3D12_BLEND_ONE,                    // SrcBlendAlpha;
                D3D12_BLEND_ONE,                    // DestBlendAlpha;
                D3D12_BLEND_OP_ADD,                 // BlendOpAlpha;
                D3D12_LOGIC_OP_NOOP,                // LogicOp
                D3D12_COLOR_WRITE_ENABLE_ALL        // RenderTargetWriteMask;
            },
            {},{},{},{},{},{},{}
        }
    };

    const D3D12_BLEND_DESC ADDITIVE
    {
        0,                                          // AlphaToCoverageEnable
        0,                                          // IndependentBlendState
        {                                           // D3D12_RENDER_TARGET_BLEND_DESC
            {
                1,                                  // BlendEnable;
                0,                                  // LogicOpEnable;
                D3D12_BLEND_ONE,                    // SrcBlend;
                D3D12_BLEND_ONE,                    // DestBlend;
                D3D12_BLEND_OP_ADD,                 // BlendOp;
                D3D12_BLEND_ONE,                    // SrcBlendAlpha;
                D3D12_BLEND_ONE,                    // DestBlendAlpha;
                D3D12_BLEND_OP_ADD,                 // BlendOpAlpha;
                D3D12_LOGIC_OP_NOOP,                // LogicOp
                D3D12_COLOR_WRITE_ENABLE_ALL        // RenderTargetWriteMask;
            },
            {},{},{},{},{},{},{}
        }
    };

    const D3D12_BLEND_DESC PREMULTIPLIED
    {
        0,                                          // AlphaToCoverageEnable
        0,                                          // IndependentBlendState
        {                                           // D3D12_RENDER_TARGET_BLEND_DESC
            {
                0,                                  // BlendEnable;
                0,                                  // LogicOpEnable;
                D3D12_BLEND_ONE,                    // SrcBlend;
                D3D12_BLEND_INV_SRC_ALPHA,          // DestBlend;
                D3D12_BLEND_OP_ADD,                 // BlendOp;
                D3D12_BLEND_ONE,                    // SrcBlendAlpha;
                D3D12_BLEND_ONE,                    // DestBlendAlpha;
                D3D12_BLEND_OP_ADD,                 // BlendOpAlpha;
                D3D12_LOGIC_OP_NOOP,                // LogicOp
                D3D12_COLOR_WRITE_ENABLE_ALL        // RenderTargetWriteMask;
            },
            {},{},{},{},{},{},{}
        }
    };
} BlendState;

constexpr struct
{
    const D3D12_STATIC_SAMPLER_DESC STATIC_POINT
    {
        D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT, // Filter;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressU;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressV;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressW;
        0.f,                                    // MipLODBias;
        1,                                      // MaxAnisotropy;
        D3D12_COMPARISON_FUNC_NONE,           // ComparisonFunc;
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, // BorderColor;
        0.f,                                    // MinLOD;
        D3D12_FLOAT32_MAX,                      // MaxLOD;
        0,                                      // ShaderRegister;
        0,                                      // RegisterSpace;
        D3D12_SHADER_VISIBILITY_PIXEL           // ShaderVisibility;
    };

    const D3D12_STATIC_SAMPLER_DESC STATIC_LINEAR
    {
        D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR, // Filter;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressU;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressV;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressW;
        0.f,                                    // MipLODBias;
        1,                                      // MaxAnisotropy;
        D3D12_COMPARISON_FUNC_NONE,           // ComparisonFunc;
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, // BorderColor;
        0.f,                                    // MinLOD;
        D3D12_FLOAT32_MAX,                      // MaxLOD;
        0,                                      // ShaderRegister;
        0,                                      // RegisterSpace;
        D3D12_SHADER_VISIBILITY_PIXEL           // ShaderVisibility;
    };

    const D3D12_STATIC_SAMPLER_DESC STATIC_ANISOTROPIC
    {
        D3D12_FILTER_ANISOTROPIC, // Filter;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressU;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressV;
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,        // AddressW;
        0.f,                                    // MipLODBias;
        16,                                      // MaxAnisotropy;
        D3D12_COMPARISON_FUNC_NONE,           // ComparisonFunc;
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, // BorderColor;
        0.f,                                    // MinLOD;
        D3D12_FLOAT32_MAX,                      // MaxLOD;
        0,                                      // ShaderRegister;
        0,                                      // RegisterSpace;
        D3D12_SHADER_VISIBILITY_PIXEL           // ShaderVisibility;
    };
} SamplerState;

constexpr D3D12_STATIC_SAMPLER_DESC
StaticSampler(const D3D12_STATIC_SAMPLER_DESC staticSampler, u32 shaderRegister, u32 registerSpace, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_STATIC_SAMPLER_DESC sampler{ staticSampler };
    sampler.ShaderRegister = shaderRegister;
    sampler.RegisterSpace = registerSpace;
    sampler.ShaderVisibility = visibility;
    return sampler;
}

// if the initial data is going to be changed later, we need it to be cpu-accessible,
// if we only want to upload some data to be used by the GPU, isCpuAccessible should be set to false
DXResource* CreateResourceBuffer(const void* data, u32 bufferSize,
    bool isCpuAccessible = false,
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
    DXHeap* heap = nullptr, u64 heapOffset = 0);

struct D3D12DescriptorRange : public D3D12_DESCRIPTOR_RANGE1
{
    constexpr explicit D3D12DescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType, u32 descriptorCount, u32 shaderRegister,
        u32 registerSpace = 0,
        D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
        u32 offsetFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
    ) : D3D12_DESCRIPTOR_RANGE1{ rangeType, descriptorCount, shaderRegister, registerSpace, flags, offsetFromTableStart } {
    }
};

struct D3D12RootParameter : public D3D12_ROOT_PARAMETER1
{
    constexpr void AsConstants(u32 numConstants, D3D12_SHADER_VISIBILITY visibility, u32 shaderRegister,
        u32 registerSpace = 0)
    {
        ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        ShaderVisibility = visibility;
        Constants.ShaderRegister = shaderRegister;
        Constants.RegisterSpace = registerSpace;
        Constants.Num32BitValues = numConstants;
    }

    constexpr void AsDescriptorTable(D3D12_SHADER_VISIBILITY visibility, const D3D12DescriptorRange* ranges, u32 rangeCount)
    {
        ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        ShaderVisibility = visibility;
        DescriptorTable.NumDescriptorRanges = rangeCount;
        DescriptorTable.pDescriptorRanges = ranges;
    }

    constexpr void AsCBV(D3D12_SHADER_VISIBILITY visibility, u32 shaderRegister,
        u32 registerSpace = 0, D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
    {
        AsDescriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, visibility, shaderRegister, registerSpace, flags);
    }

    constexpr void AsSRV(D3D12_SHADER_VISIBILITY visibility, u32 shaderRegister,
        u32 registerSpace = 0, D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
    {
        AsDescriptor(D3D12_ROOT_PARAMETER_TYPE_SRV, visibility, shaderRegister, registerSpace, flags);
    }

    constexpr void AsUAV(D3D12_SHADER_VISIBILITY visibility, u32 shaderRegister,
        u32 registerSpace = 0, D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
    {
        AsDescriptor(D3D12_ROOT_PARAMETER_TYPE_UAV, visibility, shaderRegister, registerSpace, flags);
    }

private:
    constexpr void AsDescriptor(D3D12_ROOT_PARAMETER_TYPE type, D3D12_SHADER_VISIBILITY visibility,
        u32 shaderRegister, u32 registerSpace, D3D12_ROOT_DESCRIPTOR_FLAGS flags)
    {
        ParameterType = type;
        ShaderVisibility = visibility;
        Descriptor.ShaderRegister = shaderRegister;
        Descriptor.RegisterSpace = registerSpace;
        Descriptor.Flags = flags;
    }
};

// constant buffer data has to be aligned to a multiple of 256 bytes
constexpr u64 
AlignSizeForConstantBuffer(u64 size)
{
	return math::AlignUp<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(size);
}

//NOTE: try not to apply singular barriers whenever possible
void TransitionResource(DXResource* resource, DXGraphicsCommandList* cmdList,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
    D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
    u32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

class D3D12ResourceBarrierList
{
public:
    constexpr static u32 MAX_RESOURCE_BARRIERS{ 64 };

    constexpr void AddTransitionBarrier(DXResource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, 
        D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE, u32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        assert(resource);
        assert(_barrierCount < MAX_RESOURCE_BARRIERS);

        D3D12_RESOURCE_BARRIER& barrier{ _barriers[_barrierCount] };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = flags;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = subresource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;

        ++_barrierCount;
    }

    constexpr void AddUavBarrier(DXResource* resource, 
        D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
    {
        assert(resource);
        assert(_barrierCount < MAX_RESOURCE_BARRIERS);

        D3D12_RESOURCE_BARRIER& barrier{ _barriers[_barrierCount] };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = flags;
        barrier.UAV.pResource = resource;

        ++_barrierCount;
    }

    constexpr void AddAliasingBarrier(DXResource* resourceBefore, DXResource* resourceAfter,
        D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
    {
        assert(resourceBefore && resourceAfter);
        assert(_barrierCount < MAX_RESOURCE_BARRIERS);

        D3D12_RESOURCE_BARRIER& barrier{ _barriers[_barrierCount] };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = flags;
        barrier.Aliasing.pResourceBefore = resourceBefore;
        barrier.Aliasing.pResourceAfter = resourceAfter;

        ++_barrierCount;
    }

    void ApplyBarriers(DXGraphicsCommandList* cmdList)
    {
        assert(_barrierCount != 0);
        cmdList->ResourceBarrier(_barrierCount, _barriers);
        _barrierCount = 0;
    }

private:
    D3D12_RESOURCE_BARRIER _barriers[MAX_RESOURCE_BARRIERS]{};
    u32 _barrierCount{ 0 };
};

ID3D12RootSignature* CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& desc);

// max 64 x dword divided among the root parameters
// root constants - 1 dw per 32-bit constant
// root descriptor (cbv, srv, uav) - 2 dw each
// descriptor table pointer - 1 dw
// static samplers - 0 dw (compiled into shader)
struct D3D12RootSignatureDesc : public D3D12_ROOT_SIGNATURE_DESC1
{
    constexpr static D3D12_ROOT_SIGNATURE_FLAGS DEFAULT_FLAGS{
        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
        // | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
    };

    constexpr explicit D3D12RootSignatureDesc(
        const D3D12RootParameter* parameters, u32 parameter_count,
        D3D12_ROOT_SIGNATURE_FLAGS flags = DEFAULT_FLAGS,
        const D3D12_STATIC_SAMPLER_DESC* static_samplers = nullptr, u32 sampler_count = 0
    ) : D3D12_ROOT_SIGNATURE_DESC1{ parameter_count, parameters, sampler_count, static_samplers, flags } {}

    ID3D12RootSignature* Create() const
    {
        return CreateRootSignature(*this);
    }
};

#pragma warning(push)
#pragma warning(disable: 4324)
template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type, typename T>
class alignas(void*) D3D12PipelineStateSubobject
{
public:
    D3D12PipelineStateSubobject() = default;
    constexpr explicit D3D12PipelineStateSubobject(T subobject) : _type{ type }, _subobject{ subobject } {}
    D3D12PipelineStateSubobject& operator=(const T& subobject) { _subobject = subobject; return *this; }

private:
    const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _type{ type };
    T _subobject{};
};
#pragma warning(pop)

// Pipeline State Subobjects
#define PSS(name, ...) using D3D12PipelineStateSubobject##name = D3D12PipelineStateSubobject<__VA_ARGS__>;
PSS(RootSignature, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature*)
PSS(VS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE)
PSS(PS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE)
PSS(DS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS, D3D12_SHADER_BYTECODE)
PSS(HS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS, D3D12_SHADER_BYTECODE)
PSS(GS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS, D3D12_SHADER_BYTECODE)
PSS(CS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, D3D12_SHADER_BYTECODE)
PSS(AS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, D3D12_SHADER_BYTECODE)
PSS(MS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, D3D12_SHADER_BYTECODE)
PSS(StreamOutput, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT, D3D12_STREAM_OUTPUT_DESC)
PSS(Blend, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, D3D12_BLEND_DESC)
PSS(SampleMask, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, u32)
PSS(Rasterizer, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC)
PSS(DepthStencil, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1, D3D12_DEPTH_STENCIL_DESC1)
PSS(InputLayout, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT, D3D12_INPUT_LAYOUT_DESC)
PSS(IbStripCutValue, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE)
PSS(PrimitiveTopology, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE)
PSS(RenderTargetFormats, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY)
PSS(DepthStencilFormat, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT)
PSS(SampleDesc, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DXGI_SAMPLE_DESC)
PSS(NodeMask, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK, u32)
PSS(CachedPSO, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO, D3D12_CACHED_PIPELINE_STATE)
PSS(Flags, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS, D3D12_PIPELINE_STATE_FLAGS)
PSS(ViewInstancing, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING, D3D12_VIEW_INSTANCING_DESC)
#undef PSS

struct D3D12PipelineStateSubobjectStream
{
    D3D12PipelineStateSubobjectRootSignature rootSignature{ nullptr };
    D3D12PipelineStateSubobjectVS vs{};
    D3D12PipelineStateSubobjectPS ps{};
    D3D12PipelineStateSubobjectDS ds{};
    D3D12PipelineStateSubobjectHS hs{};
    D3D12PipelineStateSubobjectGS gs{};
    D3D12PipelineStateSubobjectCS cs{};
    D3D12PipelineStateSubobjectAS as{};
    D3D12PipelineStateSubobjectMS ms{};
    D3D12PipelineStateSubobjectStreamOutput streamOutput{};
    D3D12PipelineStateSubobjectBlend blend{ BlendState.DISABLED };
    D3D12PipelineStateSubobjectSampleMask sampleMask{ UINT_MAX };
    D3D12PipelineStateSubobjectRasterizer rasterizer{ RasterizerState.NO_CULLING };
    D3D12PipelineStateSubobjectDepthStencil depthStencil{ DepthState.DISABLED };
    D3D12PipelineStateSubobjectInputLayout inputLayout{};
    D3D12PipelineStateSubobjectIbStripCutValue ibStripCutValue{};
    D3D12PipelineStateSubobjectPrimitiveTopology primitiveTopology{};
    D3D12PipelineStateSubobjectRenderTargetFormats renderTargetFormats{};
    D3D12PipelineStateSubobjectDepthStencilFormat depthStencilFormat{};
    D3D12PipelineStateSubobjectSampleDesc sampleDesc{ {1, 0} };
    D3D12PipelineStateSubobjectNodeMask nodeMask{};
    D3D12PipelineStateSubobjectCachedPSO cachedPSO{};
    D3D12PipelineStateSubobjectFlags flags{};
    D3D12PipelineStateSubobjectViewInstancing viewInstancing{};
};

ID3D12PipelineState* CreatePipelineState(D3D12_PIPELINE_STATE_STREAM_DESC desc);
ID3D12PipelineState* CreatePipelineState(void* stream, u64 streamSize);

}