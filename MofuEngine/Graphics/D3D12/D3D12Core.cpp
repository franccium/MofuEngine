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
#include "Graphics/Lights/Light.h"
#include "Lights/D3D12Light.h"
#include "Lights/D3D12LightCulling.h"

#include "tracy/TracyD3D12.hpp"
#include "tracy/Tracy.hpp"

#define ENABLE_DEBUG_LAYER 1
#define ENABLE_GPU_BASED_VALIDATION 1
#define RENDER_SCENE_ONTO_GUI_IMAGE 1

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 615; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

using namespace Microsoft::WRL;

namespace mofu::graphics::d3d12::core {
namespace {

//TODO: a list of new items/sth
bool renderItemsUpdated{ true };

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
            for (u32 j{ 0 }; j < COMMAND_LIST_WORKERS; ++j)
            {
                DXCall(hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&frame.cmdAllocators[j])));
                if (FAILED(hr)) goto _error;
                NAME_D3D12_OBJECT_INDEXED(frame.cmdAllocators[j], i * COMMAND_LIST_WORKERS + j, type == D3D12_COMMAND_LIST_TYPE_DIRECT ? L"GFX Command Allocator"
                    : type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? L"Compute Command Allocator" : L"Command Allocator");
            }

            for (u32 j{ 0 }; j < BUNDLE_COUNT; ++j)
            {
                DXCall(hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&frame.cmdAllocatorsBundle[j])));
                if (FAILED(hr)) goto _error;
                NAME_D3D12_OBJECT_INDEXED(frame.cmdAllocatorsBundle[j], i * COMMAND_LIST_WORKERS + j, L"Bundle Command Allocator");
            }
        }

        for (u32 i{ 0 }; i < COMMAND_LIST_WORKERS; ++i)
        {
            DXCall(hr = device->CreateCommandList(0, type, _cmdFrames[0].cmdAllocators[i], nullptr, IID_PPV_ARGS(&_cmdLists[i])));
            if (FAILED(hr)) goto _error;
            DXCall(_cmdLists[i]->Close());
            NAME_D3D12_OBJECT_INDEXED(_cmdLists[i], i, type == D3D12_COMMAND_LIST_TYPE_DIRECT ? L"GFX Command List"
                : type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? L"Compute Command List" : L"Command List");
        }
        for (u32 i{ 0 }; i < BUNDLE_COUNT; ++i)
        {
            DXCall(hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, _cmdFrames[0].cmdAllocatorsBundle[i], nullptr, IID_PPV_ARGS(&_cmdListsBundle[i])));
            if (FAILED(hr)) goto _error;
            DXCall(_cmdListsBundle[i]->Close());
            NAME_D3D12_OBJECT_INDEXED(_cmdListsBundle[i], i, L"Bundle Command List");
        }

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

    ~D3D12Command() { assert(!_cmdQueue && !_fence && !_cmdLists); }

    // wait for the current frame to be signalised and reset the command list and allocator
    void BeginFrame()
    {
        ZoneScopedN("BeginFrame CPU");

        CommandFrame& frame{ _cmdFrames[_frameIndex] };
        frame.Wait(_fenceEvent, _fence);

        // free the memory used by previously recorded and reopen the list for recording new commnads
        for (u32 i = 0; i < COMMAND_LIST_WORKERS; ++i)
        {
            DXCall(frame.cmdAllocators[i]->Reset());
            DXCall(_cmdLists[i]->Reset(frame.cmdAllocators[i], nullptr));
        }

        if (renderItemsUpdated)
        {
            for (u32 i = 0; i < BUNDLE_COUNT; ++i)
            {
                DXCall(frame.cmdAllocatorsBundle[i]->Reset());
                DXCall(_cmdListsBundle[i]->Reset(frame.cmdAllocatorsBundle[i], nullptr));
            }
        }
    }

    // signal the fence with the new fence value
    void EndFrame(const D3D12Surface& surface)
    {
        ZoneScopedN("EndFrame CPU");

        for (u32 i{ FIRST_DEPTH_WORKER }; i < COMMAND_LIST_WORKERS; ++i)
        {
            DXCall(_cmdLists[i]->Close());
        }
        //_cmdQueue->ExecuteCommandLists(COMMAND_LIST_WORKERS, (ID3D12CommandList* const*)&_cmdLists[0]);

        // only depth mt
        /*_cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&_cmdLists[0]);
        _cmdQueue->ExecuteCommandLists(COMMAND_LIST_WORKERS - 1, (ID3D12CommandList* const*)&_cmdLists[1]);*/

        // all mt
#if MT
        _cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&_cmdLists[DEPTH_SETUP_LIST]);
        if constexpr (DEPTH_WORKERS > 1)
            _cmdQueue->ExecuteCommandLists(DEPTH_WORKERS, (ID3D12CommandList* const*)&_cmdLists[FIRST_DEPTH_WORKER]);
        if constexpr (COMMAND_LIST_WORKERS > 1)
            _cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&_cmdLists[MAIN_SETUP_LIST]);
        if constexpr (GPASS_WORKERS > 1)
            _cmdQueue->ExecuteCommandLists(GPASS_WORKERS, (ID3D12CommandList* const*)&_cmdLists[FIRST_MAIN_GPASS_WORKER]);
        if constexpr (COMMAND_LIST_WORKERS > 1)
            _cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&_cmdLists[CLOSING_LIST_INDEX]);
#else
        _cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&_cmdLists[DEPTH_SETUP_LIST]);
#endif

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
        for (u32 i{ 0 }; i < COMMAND_LIST_WORKERS; ++i)
            core::Release(_cmdLists[i]);
        for (u32 i{ 0 }; i < BUNDLE_COUNT; ++i)
            core::Release(_cmdListsBundle[i]);
        for (u32 i{ 0 }; i < FRAME_BUFFER_COUNT; ++i)
            _cmdFrames[i].Release();
    }

    [[nodiscard]] constexpr ID3D12CommandQueue* const CommandQueue() const { return _cmdQueue; }
    [[nodiscard]] constexpr DXGraphicsCommandList* const CommandList(u32 workerIdx) const { return _cmdLists[workerIdx]; }
    [[nodiscard]] constexpr DXGraphicsCommandList* const* CommandLists() const { return _cmdLists; } 
    [[nodiscard]] constexpr DXGraphicsCommandList* const CommandListBundle(u32 workerIdx) const { return _cmdListsBundle[workerIdx]; }
    [[nodiscard]] constexpr DXGraphicsCommandList* const* CommandListsBundle() const { return _cmdListsBundle; }
    [[nodiscard]] constexpr u32 FrameIndex() const { return _frameIndex; }

private:
    struct CommandFrame
    {
        ID3D12CommandAllocator* cmdAllocators[COMMAND_LIST_WORKERS]{};
        ID3D12CommandAllocator* cmdAllocatorsBundle[BUNDLE_COUNT]{};
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
            for (u32 i{ 0 }; i < COMMAND_LIST_WORKERS; ++i)
            {
                core::Release(cmdAllocators[i]);
            }
            for (u32 i{ 0 }; i < BUNDLE_COUNT; ++i)
            {
                core::Release(cmdAllocatorsBundle[i]);
            }
            fenceValue = 0;
        }
    };

    CommandFrame _cmdFrames[FRAME_BUFFER_COUNT]{};
    DXGraphicsCommandList* _cmdLists[COMMAND_LIST_WORKERS];
    DXGraphicsCommandList* _cmdListsBundle[BUNDLE_COUNT];
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

TracyD3D12Ctx tracyQueueContext{ nullptr };

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
    data.DirectionalLightsCount = graphics::light::GetDirectionalLightsCount(info.LightSetIdx);
    //data.AmbientLight = ;
    
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
        surface.LightCullingID(),
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
#if ENABLE_DEBUG_LAYER
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

#if ENABLE_DEBUG_LAYER
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

    tracyQueueContext = TracyD3D12Context(mainDevice, gfxCommand.CommandQueue());

    if (!ui::Initialize(gfxCommand.CommandQueue())) return InitializeFailed();

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
#if ENABLE_DEBUG_LAYER
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

void 
RenderItemsUpdated()
{
    renderItemsUpdated = true;
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
    TracyD3D12NewFrame(tracyQueueContext);

    DXGraphicsCommandList* const cmdListDepthSetup{ gfxCommand.CommandList(DEPTH_SETUP_LIST) };
    DXGraphicsCommandList* const cmdListGPassSetup{ gfxCommand.CommandList(MAIN_SETUP_LIST) };
    DXGraphicsCommandList* const cmdListFXSetup{ gfxCommand.CommandList(CLOSING_LIST_INDEX) };
    DXGraphicsCommandList* const* cmdLists{ gfxCommand.CommandLists() };

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

    d3dx::D3D12ResourceBarrierList& barriers{ resourceBarriers };

    gpass::StartNewFrame(d3d12FrameInfo);

    gpass::SetBufferSize({ d3d12FrameInfo.SurfaceWidth, d3d12FrameInfo.SurfaceHeight });

    ID3D12DescriptorHeap* const heaps[]{ srvDescHeap.Heap() };

    {
        TracyD3D12ZoneC(tracyQueueContext, cmdListDepthSetup, "Render Surface Frame Start", tracy::Color::Violet);

        for (u32 i{ 0 }; i < COMMAND_LIST_WORKERS; ++i)
        {
            DXGraphicsCommandList* const cmdList{ cmdLists[i] };

            cmdList->SetDescriptorHeaps(1, &heaps[0]);

            cmdList->RSSetViewports(1, surface.Viewport());
            cmdList->RSSetScissorRects(1, surface.ScissorRect());
        }

        ui::SetupGUIFrame();

        //TODO: for now just do that here, would need to rewrite everything
        ecs::UpdateRenderSystems(ecs::system::SystemUpdateData{}, d3d12FrameInfo);
    }

    DXGraphicsCommandList* const* depthBundles{ gfxCommand.CommandListsBundle() };
    DXGraphicsCommandList* const* mainBundles{ &gfxCommand.CommandListsBundle()[MAIN_BUNDLE_INDEX] };
    {
        // Depth Prepass
        constexpr u32 LAST_DEPTH_WORKER{ DEPTH_WORKERS + FIRST_DEPTH_WORKER };
        {
            ZoneScopedNC("Depth Prepass Preparation", tracy::Color::DarkSeaGreen2);
            // the first command list clears the depth buffer and prepares it to be written to
            gpass::AddTransitionsForDepthPrepass(barriers);
            barriers.ApplyBarriers(cmdListDepthSetup);
            gpass::ClearDepthStencilView(cmdListDepthSetup);
            
            if constexpr (DEPTH_WORKERS > 1)
                gfxCommand.CommandList(0)->Close();

            for (u32 i{ FIRST_DEPTH_WORKER }; i < LAST_DEPTH_WORKER; ++i)
            {
                gpass::SetRenderTargetsForDepthPrepass(gfxCommand.CommandList(i));
            }
        }

        {
            ZoneScopedNC("Depth Prepass Execution", tracy::Color::DarkSeaGreen1);
            if (renderItemsUpdated)
            {
                for (u32 i{ 0 }; i < DEPTH_WORKERS; ++i)
                {
                    depthBundles[i]->SetDescriptorHeaps(1, &heaps[0]);
                }
                gpass::DoDepthPrepass(&depthBundles[0], d3d12FrameInfo, 0);
                for (u32 i{ 0 }; i < DEPTH_WORKERS; ++i)
                {
                    depthBundles[i]->Close();
                }
            }
            else
            {
                for (u32 i{ FIRST_DEPTH_WORKER }; i < LAST_DEPTH_WORKER; ++i)
                {
                    TracyD3D12ZoneC(tracyQueueContext, gfxCommand.CommandList(i), "Depth Prepass Worker", tracy::Color::DarkSeaGreen3);
                    gfxCommand.CommandList(i)->ExecuteBundle(depthBundles[i - FIRST_DEPTH_WORKER]);
                }
            }
        }
    }

    {
        // Lighting Pass
        u32 lightSetIdx{ frameInfo.LightSetIdx };
        const graphics::light::LightSet& set{ graphics::light::GetLightSet(lightSetIdx) };
        light::UpdateLightBuffers(set, lightSetIdx, frameIndex);
        light::CullLights(cmdListGPassSetup, d3d12FrameInfo, barriers);
    }

    // Main GPass
    {
        //TracyD3D12ZoneC(tracyQueueContext, cmdListSetup, "Main GPass", tracy::Color::PaleVioletRed3);

        constexpr u32 LAST_GPASS_WORKER{ FIRST_MAIN_GPASS_WORKER + GPASS_WORKERS };

        {
            ZoneScopedNC("Main GPass Preparation", tracy::Color::PaleVioletRed2);

            // the first command list has to setup the depth buffer to be read from, but can do it only after the others finished their work on depth;
            // transitions and clears the main buffer
            
            gpass::AddTransitionsForGPass(barriers);
            barriers.ApplyBarriers(cmdListGPassSetup);
            gpass::ClearMainBufferView(cmdListGPassSetup);

            //if (FIRST_MAIN_GPASS_WORKER > 0)
                //cmdListGPassSetup->Close();
            for (u32 i{ FIRST_MAIN_GPASS_WORKER }; i < LAST_GPASS_WORKER; ++i)
            {
                gpass::SetRenderTargetsForGPass(gfxCommand.CommandList(i));
            }
        }

        {
            ZoneScopedNC("Main GPass Execution", tracy::Color::PaleVioletRed1);
            if (renderItemsUpdated)
            {
                ZoneScopedNC("Main GPass Bundles Update", tracy::Color::PaleVioletRed1);
                for (u32 i{ 0 }; i < GPASS_WORKERS; ++i)
                {
                    mainBundles[i]->SetDescriptorHeaps(1, &heaps[0]);
                }
                gpass::RenderMT(&mainBundles[0], d3d12FrameInfo);

                for (u32 i{ 0 }; i < GPASS_WORKERS; ++i)
                {
                    mainBundles[i]->Close();
                }
            }
            else
            {
                ZoneScopedNC("Main GPass Bundles Start", tracy::Color::PaleVioletRed1);
                for (u32 i{ FIRST_MAIN_GPASS_WORKER }; i < LAST_GPASS_WORKER; ++i)
                {
                    TracyD3D12ZoneC(tracyQueueContext, gfxCommand.CommandList(i), "Main GPass Worker", tracy::Color::Red3);

                    gfxCommand.CommandList(i)->ExecuteBundle(mainBundles[i - FIRST_MAIN_GPASS_WORKER]);
                }
            }
        }

        renderItemsUpdated = false;
    }

    // Post Processing
    {
        TracyD3D12ZoneC(tracyQueueContext, cmdListFXSetup, "Post Processing", tracy::Color::Blue2);
        {
            ZoneScopedNC("Post Processing CPU", tracy::Color::Blue1);
            gpass::AddTransitionsForPostProcess(barriers);
            barriers.AddTransitionBarrier(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers.ApplyBarriers(cmdListFXSetup);

            fx::DoPostProcessing(cmdListFXSetup, d3d12FrameInfo, surface.Rtv());
        }
    }

    // Editor UI
    {
        TracyD3D12ZoneC(tracyQueueContext, cmdListFXSetup, "Editor UI", tracy::Color::LightSkyBlue3);

        {
            ZoneScopedNC("Editor UI CPU", tracy::Color::LightSkyBlue2);
            ui::RenderGUI();
            ui::RenderSceneIntoImage(gpass::MainBuffer().Srv().gpu, d3d12FrameInfo);
#if RENDER_2D_TEST
#if RENDER_SCENE_ONTO_GUI_IMAGE
            //TODO: make it work with post processing
            ui::RenderSceneIntoImage(cmdListFXSetup, gpass::MainBuffer().Srv().gpu, d3d12FrameInfo);
#else
            // TODO:
#endif // RENDER_SCENE_ONTO_GUI_IMAGE
#else
            // render 3d scene
#endif // RENDER_2D_TEST

            ui::EndGUIFrame(cmdListFXSetup);
        }
    }

    d3dx::TransitionResource(currentBackBuffer, cmdListFXSetup, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    gfxCommand.EndFrame(surface);

    TracyD3D12Collect(tracyQueueContext);

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