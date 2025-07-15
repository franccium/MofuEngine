#include "D3D12GUI.h"
#include "D3D12Core.h"
#include "D3D12Surface.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "Utilities/Logger.h"
#include "Editor/EditorInterface.h"
#include "D3D12Content/D3D12Texture.h"

namespace mofu::graphics::d3d12::ui {
namespace {
        
static void 
WindowConstrainAspectRatio(ImGuiSizeCallbackData* data)
{
    float aspect_ratio = *(float*)data->UserData;
    data->DesiredSize.y = (float)(int)(data->DesiredSize.x / aspect_ratio);
}

static void 
WindowConstrainSizeInSteps(ImGuiSizeCallbackData* data)
{
    float step = *(float*)data->UserData;
    data->DesiredSize = ImVec2((int)(data->DesiredSize.x / step + 0.5f) * step, (int)(data->DesiredSize.y / step + 0.5f) * step);
}

ImVec2 
ContentSizeAspectRatioToAvailableRegion(float aspectRatio)
{
    ImVec2 availableSpace = ImGui::GetContentRegionAvail();
    float availableAspectRatio = availableSpace.x / availableSpace.y;

    return (availableAspectRatio > aspectRatio)
        ? ImVec2{ availableSpace.y * aspectRatio, availableSpace.y }
        : ImVec2{ availableSpace.x, availableSpace.x / aspectRatio };
}

void
RenderConfigWindow()
{
    //TODO: config window
    ImGui::ShowDemoWindow();
}


void
RenderLoggerWindow()
{
    bool open = true;
    ImGui::Begin("Logs", &open);
    if (ImGui::SmallButton("[Debug] Add 5 entries"))
    {
        log::AddTestLogs();
    }
    ImGui::End();

    log::Draw("Logs", &open);
}
}

bool 
Initialize(ID3D12CommandQueue* queue)
{
    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = core::Device();
    initInfo.CommandQueue = queue;
    initInfo.NumFramesInFlight = FRAME_BUFFER_COUNT;
    initInfo.RTVFormat = D3D12Surface::DEFAULT_BACK_BUFFER_FORMAT;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = core::SrvHeap().Heap();
    initInfo.UserData = &core::SrvHeap();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
            DescriptorHeap* heap{ static_cast<DescriptorHeap*>(info->UserData) };
            DescriptorHandle handle{ heap->AllocateDescriptor() };
            *outCpuHandle = handle.cpu;
            *outGpuHandle = handle.gpu;
        };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
            DescriptorHeap* heap{ static_cast<DescriptorHeap*>(info->UserData) };
            DescriptorHandle handle{ cpu_handle, gpu_handle };
            return heap->FreeDescriptor(handle);
        };

    if(!ImGui_ImplDX12_Init(&initInfo)) return false;

    if (!editor::InitializeEditorGUI()) return false;
    return true;
}

void
Shutdown()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void
StartNewFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void 
SetupGUIFrame()
{
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

void 
RenderGUI()
{
    RenderConfigWindow();
    RenderLoggerWindow();
	editor::RenderEditorGUI();
}

void
RenderSceneIntoImage(D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle, D3D12FrameInfo frameInfo)
{
    float aspectRatio = (float)frameInfo.SurfaceWidth / frameInfo.SurfaceHeight;
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, FLT_MAX), WindowConstrainAspectRatio, (void*)&aspectRatio);

    ImGui::Begin("Scene View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::Image((ImTextureID)srvGpuHandle.ptr, ImGui::GetContentRegionAvail(), { 0, 0 }, { 1, 1 });
    ImGui::End();
}

void 
ViewTextureAsImage(id_t textureID)
{
    const DescriptorHandle& handle{ content::texture::GetDescriptorHandle(textureID, 0) };

    ImGui::Begin("Texture View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::Image((ImTextureID)handle.gpu.ptr, ImGui::GetContentRegionAvail(), { 0, 0 }, { 1, 1 });
    ImGui::End();
}

u64
GetImTextureID(id_t textureID, u32 mipLevel, u32 format)
{
    const DescriptorHandle& handle{ content::texture::GetDescriptorHandle(textureID, mipLevel, (DXGI_FORMAT)format) };
    return (u64)handle.gpu.ptr;
}

void 
DestroyViewTexture(id_t textureID)
{
    content::texture::RemoveTexture(textureID);
}

void 
EndGUIFrame(DXGraphicsCommandList* cmdList)
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

}