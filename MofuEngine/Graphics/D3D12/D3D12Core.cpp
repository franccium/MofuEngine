#include "D3D12Core.h"
#include "Graphics/GraphicsTypes.h"
#include "D3D12Surface.h"
#include "D3D12DescriptorHeap.h"
#include "D3D12Shaders.h"
#include "D3D12GPass.h"
#include "D3D12PostProcess.h"
#include "D3D12GUI.h"
#include "D3D12Upload.h"
#include "D3D12Camera.h"
#include "ECS/ECSCore.h"
#include "EngineAPI/ECS/SystemAPI.h"

#define ENABLE_GPU_BASED_VALIDATION 1
#define RENDER_SCENE_ONTO_GUI_IMAGE 1

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 615; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

using namespace Microsoft::WRL;

namespace mofu::graphics::d3d12::core {
namespace {

class D3D12Command
{
public:
    D3D12Command() = default;
    DISABLE_COPY_AND_MOVE(D3D12Command);
    explicit D3D12Command(DXDevice* const device, D3D12_COMMAND_LIST_TYPE type)
    {
        HRESULT hr{ S_OK };

        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Type = type;
        DXCall(hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&_cmdQueue)));
        if (FAILED(hr)) goto _error;
        NAME_D3D12_OBJECT(_cmdQueue, type == D3D12_COMMAND_LIST_TYPE_DIRECT ? L"GFX Command Queue"
            : type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? L"Compute Command Queue" : L"Command Queue");

        for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
        {
            CommandFrame& frame{ _cmdFrames[i] };
            DXCall(hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&frame.cmdAllocator)));
            if (FAILED(hr)) goto _error;
            NAME_D3D12_OBJECT_INDEXED(frame.cmdAllocator, i, type == D3D12_COMMAND_LIST_TYPE_DIRECT ? L"GFX Command Allocator"
                : type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? L"Compute Command Allocator" : L"Command Allocator");
        }

        DXCall(hr = device->CreateCommandList(0, type, _cmdFrames[0].cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList)));
        if (FAILED(hr)) goto _error;
        DXCall(_cmdList->Close());
        NAME_D3D12_OBJECT(_cmdList, type == D3D12_COMMAND_LIST_TYPE_DIRECT ? L"GFX Command List"
            : type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? L"Compute Command List" : L"Command List");

        DXCall(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
        if (FAILED(hr)) goto _error;
        NAME_D3D12_OBJECT(_fence, L"D3D12 Fence");

#ifdef _WIN64
        _fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
#endif
        assert(_fenceEvent);
        return;

    _error:
        Release();
    }

    ~D3D12Command() { assert(!_cmdQueue && !_fence && !_cmdList); }

    // wait for the current frame to be signalised and reset the command list and allocator
    void BeginFrame()
    {
        CommandFrame& frame{ _cmdFrames[_frameIndex] };
        frame.Wait(_fenceEvent, _fence);
        // free the memory used by previously recorded commands
        DXCall(frame.cmdAllocator->Reset());
        // reopen the list for recording new commnads
        DXCall(_cmdList->Reset(frame.cmdAllocator, nullptr));
    }

    // signal the fence with the new fence value
    void EndFrame(const D3D12Surface& surface)
    {
        DXCall(_cmdList->Close());
        ID3D12CommandList* const cmdLists[]{ _cmdList };
        _cmdQueue->ExecuteCommandLists(_countof(cmdLists), &cmdLists[0]);

        surface.Present();

        ++_fenceValue;
        CommandFrame& frame{ _cmdFrames[_frameIndex] };
        frame.fenceValue = _fenceValue;
        // if frame.fenceValue < signaled fence value, the GPU is done executing 
        // the commands for this frame and we can reuse the frame for new commands
        DXCall(_cmdQueue->Signal(_fence, _fenceValue));
        _frameIndex = (_frameIndex + 1) % FRAME_BUFFER_COUNT;
    }

    void Flush()
    {
        for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
            _cmdFrames[i].Wait(_fenceEvent, _fence);
        _frameIndex = 0;
    }

    void Release()
    {
        Flush();
        core::Release(_fence);
        _fenceValue = 0;
        CloseHandle(_fenceEvent);
        _fenceEvent = nullptr;

        core::Release(_cmdQueue);
        core::Release(_cmdList);
        for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
            _cmdFrames[i].Release();
    }

    [[nodiscard]] constexpr ID3D12CommandQueue* const CommandQueue() const { return _cmdQueue; }
    [[nodiscard]] constexpr DXGraphicsCommandList* const CommandList() const { return _cmdList; }
    [[nodiscard]] constexpr u32 FrameIndex() const { return _frameIndex; }

private:
    struct CommandFrame
    {
        ID3D12CommandAllocator* cmdAllocator{ nullptr };
        u64 fenceValue{ 0 };

        void Wait(HANDLE fenceEvent, ID3D12Fence1* fence) const
        {
            assert(fenceEvent && fence);
            // if current fence value is less than fenceValue, the GPU has not finished 
            // executing the command lists, and hasn't reached the _cmdQueue->Singal() command
            if (fence->GetCompletedValue() < fenceValue)
            {
                // wait for an event signaled once the fence's current value is equal fo fenceValue
                DXCall(fence->SetEventOnCompletion(fenceValue, fenceEvent));
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }

        void Release()
        {
            core::Release(cmdAllocator);
            fenceValue = 0;
        }
    };

    CommandFrame _cmdFrames[FRAME_BUFFER_COUNT]{};
    DXGraphicsCommandList* _cmdList{ nullptr };
    ID3D12CommandQueue* _cmdQueue{ nullptr };
    ID3D12Fence1* _fence{ nullptr };
    u32 _frameIndex{ 0 };
    u64 _fenceValue{ 0 };
#ifdef _WIN64
    HANDLE _fenceEvent{};
#endif
};

constexpr D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL{ D3D_FEATURE_LEVEL_11_0 };
DXDevice* mainDevice{ nullptr };
DXGIFactory* dxgiFactory{ nullptr };
D3D12Command gfxCommand;

util::FreeList<D3D12Surface> surfaces{};

DescriptorHeap rtvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
DescriptorHeap dsvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
DescriptorHeap srvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
DescriptorHeap uavDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
constexpr u32 DESC_HEAP_CAPACITY{ 512 };
constexpr u32 SRV_DESC_HEAP_CAPACITY{ 4096 };

ConstantBuffer constantBuffers[FRAME_BUFFER_COUNT];
constexpr u32 CONSTANT_BUFFER_SIZE{ 1024 * 1024 };

u32 deferredReleasesFlag[FRAME_BUFFER_COUNT]{};
std::mutex deferredReleasesMutex{};
Vec<IUnknown*> deferredReleases[FRAME_BUFFER_COUNT]{};

d3dx::D3D12ResourceBarrierList resourceBarriers{};

bool 
InitializeModules()
{
    return upload::Initialize() && shaders::Initialize() && gpass::Initialize() && fx::Initialize();
}

bool 
InitializeFailed()
{
    Shutdown();
    return false;
}

D3D_FEATURE_LEVEL 
GetMaxSupportedFeatureLevel(DXAdapter* adapter)
{
    constexpr D3D_FEATURE_LEVEL featureLevels[]
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_2
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelData{};
    featureLevelData.NumFeatureLevels = _countof(featureLevels);
    featureLevelData.pFeatureLevelsRequested = featureLevels;
    ComPtr<DXDevice> device;
    DXCall(D3D12CreateDevice(adapter, MINIMUM_FEATURE_LEVEL, IID_PPV_ARGS(&device)));
    DXCall(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevelData, sizeof(featureLevelData)));
    assert(featureLevelData.MaxSupportedFeatureLevel >= MINIMUM_FEATURE_LEVEL);
    return featureLevelData.MaxSupportedFeatureLevel;
}

bool
InitializeDescriptorHeaps()
{
    bool result{ true };
    result &= rtvDescHeap.Initialize(DESC_HEAP_CAPACITY, false);
    result &= dsvDescHeap.Initialize(DESC_HEAP_CAPACITY, false);
    result &= srvDescHeap.Initialize(SRV_DESC_HEAP_CAPACITY, true);
    result &= uavDescHeap.Initialize(DESC_HEAP_CAPACITY, false);
    if (!result) return false;
    NAME_D3D12_OBJECT(rtvDescHeap.Heap(), L"RTV Descriptor Heap");
    NAME_D3D12_OBJECT(dsvDescHeap.Heap(), L"DSV Descriptor Heap");
    NAME_D3D12_OBJECT(srvDescHeap.Heap(), L"SRV Descriptor Heap");
    NAME_D3D12_OBJECT(uavDescHeap.Heap(), L"UAV Descriptor Heap");
    return true;
}

void __declspec(noinline)
ProcessDeferredReleases(u32 frameId)
{
    std::lock_guard lock{ deferredReleasesMutex };
    deferredReleasesFlag[frameId] = 0;

    rtvDescHeap.ProcessDeferredFree(frameId);
    dsvDescHeap.ProcessDeferredFree(frameId);
    srvDescHeap.ProcessDeferredFree(frameId);
    uavDescHeap.ProcessDeferredFree(frameId);

    Vec<IUnknown*>& resourcesToFree{ deferredReleases[frameId] };
    if (!resourcesToFree.empty())
    {
        for (auto& resource : resourcesToFree)
        {
            Release(resource);
        }
        resourcesToFree.clear();
    }
}

D3D12FrameInfo  
GetD3D12FrameInfo(const FrameInfo& info, ConstantBuffer& cbuffer, const D3D12Surface& surface, u32 frameIndex, f32 deltaTime)
{
    camera::D3D12Camera& camera{ camera::GetCamera(info.CameraID) };
    camera.Update();
    
    // fill out the global shader data
    using namespace DirectX;
    hlsl::GlobalShaderData data{};
    XMStoreFloat4x4A(&data.View, camera.View());
	XMStoreFloat4x4A(&data.Projection, camera.Projection());
	XMStoreFloat4x4A(&data.InvProjection, camera.InverseProjection());
	XMStoreFloat4x4A(&data.ViewProjection, camera.ViewProjection());    
	XMStoreFloat4x4A(&data.InvViewProjection, camera.InverseViewProjection());    
    XMStoreFloat3(&data.CameraPosition, camera.Position());
	XMStoreFloat3(&data.CameraDirection, camera.Direction());
    data.ViewWidth = surface.Viewport()->Width;
    data.ViewHeight = surface.Viewport()->Height;
    data.DeltaTime = deltaTime;
    
    hlsl::GlobalShaderData* const shaderData{ cbuffer.AllocateSpace<hlsl::GlobalShaderData>() };
    memcpy(shaderData, &data, sizeof(hlsl::GlobalShaderData));

    D3D12FrameInfo frameInfo
    {
        &info,
        &camera,
        cbuffer.GpuAddress(shaderData),
        surface.Width(),
        surface.Height(),
        frameIndex,
        deltaTime
    };

    return frameInfo;
}

} // anonymous namespace

bool
Initialize()
{
    /*
    * initialize: D3D12
    * Descriptor Heaps
    * Global Constant Buffer
    * Graphics Command Manager
    * D3D12 Modules
    */

    if (mainDevice) Shutdown();

    HRESULT hr{ S_OK };
    u32 dxgiFactoryFlags{ 0 };
#ifdef _DEBUG
    {
        ComPtr<DXDebug> debugInterface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
            debugInterface->EnableDebugLayer();
        else
            OutputDebugStringA("\nWARNING: D3D12 Debug Interface is not available\n");

        debugInterface->SetEnableGPUBasedValidation(ENABLE_GPU_BASED_VALIDATION);

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    DXCall(hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
    if (FAILED(hr)) return InitializeFailed();

    ComPtr<DXAdapter> mainAdapter;
    DXAdapter* adapter{ nullptr };
    for (u32 i{ 0 }; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        if (SUCCEEDED(D3D12CreateDevice(adapter, MINIMUM_FEATURE_LEVEL, __uuidof(DXDevice), nullptr)))
            break;
        Release(adapter);
    }
    mainAdapter.Attach(adapter);
    if (!mainAdapter) return InitializeFailed();

    D3D_FEATURE_LEVEL maxSupportedFeatureLevel{ GetMaxSupportedFeatureLevel(mainAdapter.Get()) };
    DXCall(hr = D3D12CreateDevice(mainAdapter.Get(), maxSupportedFeatureLevel, IID_PPV_ARGS(&mainDevice)));
    if (FAILED(hr)) return InitializeFailed();
    NAME_D3D12_OBJECT(mainDevice, L"Main D3D12 Device");

    if (!InitializeDescriptorHeaps()) return InitializeFailed();

    new (&gfxCommand) D3D12Command(mainDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (!gfxCommand.CommandQueue()) return InitializeFailed();

    for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
    {
        new (&constantBuffers[i]) ConstantBuffer(ConstantBuffer::DefaultInitInfo(CONSTANT_BUFFER_SIZE));
        NAME_D3D12_OBJECT_INDEXED(constantBuffers[i].Buffer(), i, L"Global Constant Buffer");
        if (!constantBuffers[i].Buffer()) return InitializeFailed();
    }

    if (!InitializeModules()) return InitializeFailed();

#ifdef _DEBUG
    {
        ComPtr<ID3D12InfoQueue> infoQueue;
        DXCall(mainDevice->QueryInterface(IID_PPV_ARGS(&infoQueue)));

        //D3D12_MESSAGE_ID disabled_messages[]
        //{
        //};
        //D3D12_INFO_QUEUE_FILTER filter{};
        //filter.DenyList.NumIDs = _countof(disabled_messages);
        //filter.DenyList.pIDList = &disabled_messages[0];
        //info_queue->AddStorageFilterEntries(&filter);

        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    }
#endif

    if (!ui::Initialize(gfxCommand.CommandList(), gfxCommand.CommandQueue())) return InitializeFailed();

    return true;
}

void
Shutdown()
{
    gfxCommand.Release();

    for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
        ProcessDeferredReleases(i);

    gpass::Shutdown();
    fx::Shutdown();
    shaders::Shutdown();
    upload::Shutdown();

    for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
        constantBuffers[i].Release();

    rtvDescHeap.ProcessDeferredFree(0);
    dsvDescHeap.ProcessDeferredFree(0);
    srvDescHeap.ProcessDeferredFree(0);
    uavDescHeap.ProcessDeferredFree(0);
    rtvDescHeap.Release();
    dsvDescHeap.Release();
    srvDescHeap.Release();
    uavDescHeap.Release();
    ProcessDeferredReleases(0);

    Release(dxgiFactory);
#ifdef _DEBUG
    if (mainDevice)
    {
        {
            ComPtr<ID3D12InfoQueue> info_queue;
            DXCall(mainDevice->QueryInterface(IID_PPV_ARGS(&info_queue)));
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
        }
        ComPtr<ID3D12DebugDevice2> debugDeivce;
        DXCall(mainDevice->QueryInterface(IID_PPV_ARGS(&debugDeivce)));
        Release(mainDevice);
        DXCall(debugDeivce->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));
    }
#endif
    Release(mainDevice);
}

DXDevice* const Device() { return mainDevice; }

DescriptorHeap& RtvHeap() { return rtvDescHeap; }
DescriptorHeap& DsvHeap() { return dsvDescHeap; }
DescriptorHeap& SrvHeap() { return srvDescHeap; }
DescriptorHeap& UavHeap() { return uavDescHeap; }

ConstantBuffer& CBuffer() { return constantBuffers[CurrentFrameIndex()]; }

u32 CurrentFrameIndex() { return gfxCommand.FrameIndex(); }

void SetHasDeferredReleases()
{
    deferredReleasesFlag[CurrentFrameIndex()] = 1;
}

namespace detail {
void 
DeferredRelease(IUnknown* resource)
{
    std::lock_guard lock{ deferredReleasesMutex };
    deferredReleases[CurrentFrameIndex()].push_back(resource);
    SetHasDeferredReleases();
}
} // namespace detail

void
RenderSurface(surface_id id, FrameInfo frameInfo)
{
    gfxCommand.BeginFrame();

    DXGraphicsCommandList* const cmdList{ gfxCommand.CommandList() };

    const u32 frameIndex{ CurrentFrameIndex() };

    ConstantBuffer& cbuffer{ constantBuffers[frameIndex] };
    cbuffer.Clear();

    if (deferredReleasesFlag[frameIndex])
    {
        ProcessDeferredReleases(frameIndex);
    }

    const D3D12Surface& surface{ surfaces[id] };
    DXResource* const currentBackBuffer{ surface.BackBuffer() };

    f32 deltaTime{ 16.7f };
    const D3D12FrameInfo d3d12FrameInfo{ GetD3D12FrameInfo(frameInfo, cbuffer, surface, frameIndex, deltaTime) };

    gpass::StartNewFrame(d3d12FrameInfo);

    gpass::SetBufferSize({ d3d12FrameInfo.SurfaceWidth, d3d12FrameInfo.SurfaceHeight });
    d3dx::D3D12ResourceBarrierList& barriers{ resourceBarriers };

    ID3D12DescriptorHeap* const heaps[]{ srvDescHeap.Heap() };
    cmdList->SetDescriptorHeaps(1, &heaps[0]);

    cmdList->RSSetViewports(1, surface.Viewport());
    cmdList->RSSetScissorRects(1, surface.ScissorRect());

    ui::SetupGUIFrame();

    barriers.AddTransitionBarrier(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);
    // Depth Prepass
    gpass::AddTransitionsForDepthPrepass(barriers);
    barriers.ApplyBarriers(cmdList);
    gpass::SetRenderTargetsForDepthPrepass(cmdList);

    //TODO: for now just do that here, would need to rewrite everything
    ecs::UpdateRenderSystems(ecs::system::SystemUpdateData{}, d3d12FrameInfo);

    gpass::DoDepthPrepass(cmdList, d3d12FrameInfo);

    // Main GPass
    gpass::AddTransitionsForGPass(barriers);
    barriers.ApplyBarriers(cmdList);
    gpass::SetRenderTargetsForGPass(cmdList);
    gpass::Render(cmdList, d3d12FrameInfo);

    // Post Processing
    gpass::AddTransitionsForPostProcess(barriers);
    barriers.AddTransitionBarrier(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);
    barriers.ApplyBarriers(cmdList);

    fx::DoPostProcessing(cmdList, d3d12FrameInfo, surface.Rtv());

    ui::RenderGUI(cmdList);
    ui::RenderSceneIntoImage(cmdList, gpass::MainBuffer().Srv().gpu, d3d12FrameInfo);
#if RENDER_2D_TEST
#if RENDER_SCENE_ONTO_GUI_IMAGE
    //TODO: make it work with post processing
    ui::RenderSceneIntoImage(cmdList, gpass::MainBuffer().Srv().gpu, d3d12FrameInfo);
#else
    // TODO:
#endif // RENDER_SCENE_ONTO_GUI_IMAGE
#else
    // render 3d scene
#endif // RENDER_2D_TEST

    ui::EndGUIFrame(cmdList);

    d3dx::TransitionResource(currentBackBuffer, cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    gfxCommand.EndFrame(surface);
}

Surface
CreateSurface(platform::Window window)
{
    surface_id id{ surfaces.add(window) };
    surfaces[id].CreateSwapChain(dxgiFactory, gfxCommand.CommandQueue());
    return Surface{ id };
}

void 
RemoveSurface(surface_id id)
{
    gfxCommand.Flush();
    surfaces.remove(id);
}

void 
ResizeSurface(surface_id id, u32 width, u32 height)
{
    gfxCommand.Flush();
    surfaces[id].Resize(width, height);
}

u32 
SurfaceWidth(surface_id id)
{
    return surfaces[id].Width();
}

u32 
SurfaceHeight(surface_id id)
{
    return surfaces[id].Height();
}

}