#include "D3D12Surface.h"
#include "Lights/D3D12LightCulling.h"

namespace mofu::graphics::d3d12 {
namespace {

constexpr DXGI_FORMAT
ToNonSRGB(DXGI_FORMAT format)
{
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) return DXGI_FORMAT_R8G8B8A8_UNORM;
    return format;
}

} // anonymous namespace

void
D3D12Surface::CreateSwapChain(DXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_FORMAT format)
{
    assert(factory && cmdQueue);
    Release();
    constexpr bool VSYNC_ENABLED{ false };
    
    if (SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &_allowTearing, sizeof(BOOL))))
        _presentFlags = DXGI_PRESENT_ALLOW_TEARING;

    _format = format;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.BufferCount = BUFFER_COUNT;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Flags = _allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;
    desc.Format = ToNonSRGB(format);
    desc.Height = _window.Height();
    desc.Width = _window.Width();
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Stereo = false;

    IDXGISwapChain1* swapChain;
    HWND hwnd{ (HWND)_window.Handle() };
    DXCall(factory->CreateSwapChainForHwnd(cmdQueue, hwnd, &desc, nullptr, nullptr, &swapChain));
    DXCall(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    DXCall(swapChain->QueryInterface(IID_PPV_ARGS(&_swapChain)));
    core::Release(swapChain);

    _currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();
    for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
    {
        _renderTargetData[i].rtv = core::RtvHeap().AllocateDescriptor();
    }

    Finalize();
    CreateDepthBuffer();
    CreateMSAAResources();

    assert(!id::IsValid(_lightCullingID));
    _lightCullingID = light::AddLightCuller();
}

void 
D3D12Surface::Present() const
{
    assert(_swapChain);
    HRESULT hr{ _swapChain->Present(0, _presentFlags) };
    if (FAILED(hr) && hr == DXGI_ERROR_DEVICE_REMOVED)
    {
        core::HandleDeviceRemoval();
    }
    _currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();
}

void 
D3D12Surface::Resize(u32 width, u32 height)
{
    assert(_swapChain);
    for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
    {
        core::Release(_renderTargetData[i].backBuffer);
    }

    const u32 flags{ _allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u };
    DXCall(_swapChain->ResizeBuffers(BUFFER_COUNT, width, height, DXGI_FORMAT_UNKNOWN, flags));
    _currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();
    Finalize();
    CreateDepthBuffer();
    CreateMSAAResources();
    DEBUG_LOG("D3D12Surface resized\n");
}

void 
D3D12Surface::SetMSAA(u32 sampleCount, u32 sampleQuality)
{
    if (sampleCount == _sampleCount && sampleQuality == _sampleQuality) return;
    
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels{};
    qualityLevels.Format = _format;
    qualityLevels.SampleCount = sampleCount;
    qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

    if (FAILED(core::Device()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, 
        &qualityLevels, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS))) || qualityLevels.NumQualityLevels == 0)
    {
        DEBUG_LOG("[ERROR]: D3D12Surface: Given MSAA sample count is not supported");
        sampleCount = 1;
        sampleQuality = 0;
    }

    _sampleCount = sampleCount;
    _sampleQuality = sampleQuality;

    if (_swapChain)
    {
        for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
        {
            MSAAResource& msaaResource{ _msaaResources[i] };
            core::Release(msaaResource.Texture);
            core::RtvHeap().FreeDescriptor(msaaResource.Rtv);
        }
        CreateMSAAResources();
    }
}

void 
D3D12Surface::ResolveMSAA(DXGraphicsCommandList* const cmdList) const
{
    if (_sampleCount <= 1) return;

    DXResource* msaaTarget{ _msaaResources[_currentBackBufferIndex].Texture };
    DXResource* backBuffer{ _renderTargetData[_currentBackBufferIndex].backBuffer };

    d3dx::D3D12ResourceBarrierList barriers{};
    barriers.AddTransitionBarrier(msaaTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    barriers.AddTransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    barriers.ApplyBarriers(cmdList);

    cmdList->ResolveSubresource(backBuffer, 0, msaaTarget, 0, _format);

    barriers.AddTransitionBarrier(msaaTarget, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers.AddTransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT);
    barriers.ApplyBarriers(cmdList);
}

void 
D3D12Surface::CreateDepthBuffer()
{
    //auto device = core::Device();
    //const u32 width = Width();
    //const u32 height = Height();

    //// Create depth buffer
    //D3D12_HEAP_PROPERTIES heapProps = {};
    //heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    //heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    //heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    //D3D12_RESOURCE_DESC depthDesc = {};
    //depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    //depthDesc.Alignment = 0;
    //depthDesc.Width = width;
    //depthDesc.Height = height;
    //depthDesc.DepthOrArraySize = 1;
    //depthDesc.MipLevels = 1;
    //depthDesc.Format = DEFAULT_DEPTH_FORMAT;
    //depthDesc.SampleDesc.Count = _sampleCount;
    //depthDesc.SampleDesc.Quality = _sampleQuality;
    //depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    //depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    //D3D12_CLEAR_VALUE depthClearValue = {};
    //depthClearValue.Format = DEFAULT_DEPTH_FORMAT;
    //depthClearValue.DepthStencil.Depth = 1.0f;
    //depthClearValue.DepthStencil.Stencil = 0;

    //DXCall(device->CreateCommittedResource(
    //    &heapProps,
    //    D3D12_HEAP_FLAG_NONE,
    //    &depthDesc,
    //    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    //    &depthClearValue,
    //    IID_PPV_ARGS(&_depthBuffer.texture)
    //));

    //// Create DSV
    //_depthBuffer.dsv = core::DsvHeap().AllocateDescriptor();

    //D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    //dsvDesc.Format = DEFAULT_DEPTH_FORMAT;
    //dsvDesc.ViewDimension = (_sampleCount > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
    //dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    //device->CreateDepthStencilView(_depthBuffer.texture, &dsvDesc, _depthBuffer.dsv.cpu);
}

void 
D3D12Surface::CreateMSAAResources()
{
    if (_sampleCount <= 1) return;

    DXDevice* const device{ core::Device() };
    const u32 width{ Width()};
    const u32 height{ Height() };

    for (u32 i = 0; i < BUFFER_COUNT; ++i)
    {
        MSAAResource& msaaResource{ _msaaResources[i] };

        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = _format;
        desc.SampleDesc.Count = _sampleCount;
        desc.SampleDesc.Quality = _sampleQuality;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = _format;
        clearValue.Color[0] = 0.f; clearValue.Color[1] = 0.f; clearValue.Color[2] = 0.f; clearValue.Color[3] = 1.f;

        DXCall(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, 
            D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&msaaResource.Texture)));
        msaaResource.Rtv = core::RtvHeap().AllocateDescriptor();
        NAME_D3D12_OBJECT_INDEXED(msaaResource.Texture, i, "MSAA Texture: frame ");

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = _format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
        device->CreateRenderTargetView(msaaResource.Texture, &rtvDesc, msaaResource.Rtv.cpu);
    }
}

void 
D3D12Surface::Release()
{
    if (id::IsValid(_lightCullingID)) light::RemoveLightCuller(_lightCullingID);

    for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
    {
        RenderTargetData& data{ _renderTargetData[i] };
        core::Release(data.backBuffer);
        core::RtvHeap().FreeDescriptor(data.rtv);

        MSAAResource& msaaResource{ _msaaResources[i] };
        core::Release(msaaResource.Texture);
        core::RtvHeap().FreeDescriptor(msaaResource.Rtv);
    }
    core::Release(_depthBuffer.Texture);
    core::DsvHeap().FreeDescriptor(_depthBuffer.Rtv);

    core::Release(_swapChain);
}

void 
D3D12Surface::Finalize()
{
    for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
    {
        RenderTargetData& data{ _renderTargetData[i] };
        assert(!data.backBuffer);
        DXCall(_swapChain->GetBuffer(i, IID_PPV_ARGS(&data.backBuffer)));
        D3D12_RENDER_TARGET_VIEW_DESC desc{};
        desc.Format = DEFAULT_BACK_BUFFER_FORMAT;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.PlaneSlice = 0;
        desc.Texture2D.MipSlice = 0;
        core::Device()->CreateRenderTargetView(data.backBuffer, &desc, data.rtv.cpu);
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    DXCall(_swapChain->GetDesc(&desc));
    const u32 width{ desc.BufferDesc.Width };
    const u32 height{ desc.BufferDesc.Height };
    assert(_window.Width() == width && _window.Height() == height);

    _viewport.TopLeftX = 0.f;
    _viewport.TopLeftY = 0.f;
    _viewport.Width = (float)width;
    _viewport.Height = (float)height;
    _viewport.MinDepth = 0.f;
    _viewport.MaxDepth = 1.f;

    _scissorRect = { 0, 0, (s32)width, (s32)height };
}

}

