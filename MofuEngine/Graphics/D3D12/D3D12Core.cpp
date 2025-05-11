#include "D3D12Core.h"
#include "D3D12Surface.h"
#include "D3D12DescriptorHeap.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 615; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

using namespace Microsoft::WRL;

namespace mofu::graphics::d3d12::core {
namespace {

class D3D12Command
{

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


bool 
InitializeModules()
{
    //TODO: return shaders::Initialize && gpass::Initialize();
    return true;
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

    
    if (!InitializeModules) return false;

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

    return true;
}

void
Shutdown()
{
}

Surface 
CreateSurface(platform::Window window)
{
    return Surface();
}

void 
RemoveSurface(surface_id id)
{
}

void 
RenderSurface(surface_id id, FrameInfo frameInfo)
{
}

void 
ResizeSurface(surface_id id, u32 width, u32 height)
{
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
