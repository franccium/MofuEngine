#pragma once
#include "D3D12CommonHeaders.h"

namespace mofu::graphics::d3d12 {
class D3D12Surface
{
public:
	constexpr static u32 BUFFER_COUNT{ 3 };
	constexpr static DXGI_FORMAT DEFAULT_BACK_BUFFER_FORMAT{ DXGI_FORMAT_R16G16B16A16_FLOAT };

	explicit D3D12Surface(platform::Window window) : _window(window) { assert(_window.Handle()); }
	~D3D12Surface() { Release(); }

	void CreateSwapChain(DXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_FORMAT format = DEFAULT_BACK_BUFFER_FORMAT);
	void Present() const;
	void Resize(u32 width, u32 height);

	[[nodiscard]] constexpr u32 Width() const { return (u32)_viewport.Width; }
	[[nodiscard]] constexpr u32 Height() const { return (u32)_viewport.Height; }
	[[nodiscard]] constexpr DXResource* const BackBuffer() const { return _renderTargetData[_currentBackBufferIndex].back_buffer; }
	[[nodiscard]] constexpr D3D12_CPU_DESCRIPTOR_HANDLE Rtv() const { return _renderTargetData[_currentBackBufferIndex].rtv.cpu; }
	[[nodiscard]] constexpr const D3D12_VIEWPORT* Viewport() const { return &_viewport; }
	[[nodiscard]] constexpr const D3D12_RECT* ScissorRect() const { return &_scissorRect; }

private:
	void Release();
	void Finalize();

	struct RenderTargetData
	{
		DXResource* back_buffer{ nullptr };
		DescriptorHandle rtv{};
	};

	platform::Window _window{};
	D3D12_VIEWPORT _viewport;
	D3D12_RECT _scissorRect{};
	DXSwapChain* _swapChain{ nullptr };
	RenderTargetData _renderTargetData[BUFFER_COUNT];
	DXGI_FORMAT _format{ DEFAULT_BACK_BUFFER_FORMAT };
	mutable u32 _currentBackBufferIndex{ 0 };
	u32 _allowTearing{ 0 };
	u32 _presentFlags{ 0 };
};

}