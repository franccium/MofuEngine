#include "D3D12Surface.h"
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
}

void 
D3D12Surface::Present() const
{
    assert(_swapChain);
    DXCall(_swapChain->Present(0, _presentFlags));
    _currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();
}

void 
D3D12Surface::Resize(u32 width, u32 height)
{
    assert(_swapChain);
    for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
    {
        core::Release(_renderTargetData[i].back_buffer);
    }

    const u32 flags{ _allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u };
    DXCall(_swapChain->ResizeBuffers(_currentBackBufferIndex, width, height, DXGI_FORMAT_UNKNOWN, flags));
    _currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();
    Finalize();
    DEBUG_LOG(L"D3D12Surface resized\n");
}

void 
D3D12Surface::Release()
{
    for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
    {
        RenderTargetData& data{ _renderTargetData[i] };
        core::Release(data.back_buffer);
        core::RtvHeap().FreeDescriptor(data.rtv);
    }
    core::Release(_swapChain);
}

void 
D3D12Surface::Finalize()
{
    for (u32 i{ 0 }; i < BUFFER_COUNT; ++i)
    {
        RenderTargetData& data{ _renderTargetData[i] };
        assert(!data.back_buffer);
        DXCall(_swapChain->GetBuffer(i, IID_PPV_ARGS(&data.back_buffer)));
        D3D12_RENDER_TARGET_VIEW_DESC desc{};
        desc.Format = DEFAULT_BACK_BUFFER_FORMAT;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.PlaneSlice = 0;
        desc.Texture2D.PlaneSlice = 0;
        core::Device()->CreateRenderTargetView(data.back_buffer, &desc, data.rtv.cpu);
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

