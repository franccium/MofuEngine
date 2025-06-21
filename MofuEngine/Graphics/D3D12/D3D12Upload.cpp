#include "D3D12Upload.h"

namespace mofu::graphics::d3d12::upload {
namespace {
ID3D12Fence1* uploadFence{ nullptr };
u64 uploadFenceValue{ 0 };
HANDLE fenceEvent{};

struct UploadFrame
{
	ID3D12CommandAllocator* CmdAllocator{ nullptr };
	DXGraphicsCommandList* CmdList{ nullptr };
	DXResource* UploadBuffer{ nullptr };
	void* CpuAddress{ nullptr };
	u64 FenceValue{ 0 };

	void WaitAndReset();

	void Release()
	{
		WaitAndReset();
		core::Release(CmdAllocator);
		core::Release(CmdList);
	}

	constexpr bool IsReady() const { return UploadBuffer == nullptr; }
};

void
UploadFrame::WaitAndReset()	
{
	assert(uploadFence && fenceEvent);
	assert(UploadBuffer);
	assert(CpuAddress);
	assert(CmdList);
	assert(CmdAllocator);

	if (uploadFence->GetCompletedValue() < FenceValue)
	{
		HRESULT hr{ S_OK };
		// the command list has not been executed yet
		DXCall(hr = uploadFence->SetEventOnCompletion(FenceValue, fenceEvent));
		assert(SUCCEEDED(hr));
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	assert(UploadBuffer);
	assert(CpuAddress);
	assert(CmdList);
	assert(CmdAllocator);

	// the command list has been executed, the frame is ready for upload
	if (UploadBuffer)
	{
		core::Release(UploadBuffer);
	}
	CpuAddress = nullptr;
}

constexpr u32 UPLOAD_FRAME_COUNT{ 4 }; // FIXME: something is very wrong here
UploadFrame uploadFrames[UPLOAD_FRAME_COUNT];
ID3D12CommandQueue* uploadCmdQueue{ nullptr };
std::mutex frameMutex{};
std::mutex queueMutex{};


// NOTE: frames should be locked before this function is called
u32
GetAvailableUploadFrame()
{
	u32 index{ U32_INVALID_ID };
	UploadFrame* const frames{ &uploadFrames[0] };
	for (u32 i{ 0 }; i < UPLOAD_FRAME_COUNT; ++i)
	{
		if (frames[i].IsReady())
		{
			index = i;
			break;
		}
	}

	// if none of the frames are done uploading, we're the only thread here, 
	// so we can just do a busy wait loop until one becomes available as the congenstion is low
	if (index == U32_INVALID_ID)
	{
		index = 0;
		while (!frames[index].IsReady())
		{
			index = (index + 1) % UPLOAD_FRAME_COUNT;
			std::this_thread::yield();
		}
	}

	// mark the frame as busy to prevent other threads from picking it
	uploadFrames[index].UploadBuffer = (DXResource*)1;

	return index;
}

bool
InitializeFailed()
{
	Shutdown();
	return false;
}

} // anonymous namespace

D3D12UploadContext::D3D12UploadContext(u32 alignedSize)
{
	assert(uploadCmdQueue);
	{
		// find an available upload frame
		std::lock_guard lock{ frameMutex };
		_frameIndex = GetAvailableUploadFrame();
		assert(_frameIndex != U32_INVALID_ID);
	}
	UploadFrame& frame{ uploadFrames[_frameIndex] };

	frame.UploadBuffer = d3dx::CreateResourceBuffer(nullptr, alignedSize, true);
	NAME_D3D12_OBJECT_INDEXED(frame.UploadBuffer, alignedSize, L"Upload Buffer - size:");

	// map the upload buffer to system memory
	const D3D12_RANGE range{};
	DXCall(frame.UploadBuffer->Map(0, &range, reinterpret_cast<void**>(&frame.CpuAddress)));
	assert(frame.CpuAddress);

	_cmdList = frame.CmdList;
	_uploadBuffer = frame.UploadBuffer;
	_cpuAddress = frame.CpuAddress;
	assert(_cmdList && _uploadBuffer && _cpuAddress);
	
	DXCall(frame.CmdAllocator->Reset());
	DXCall(_cmdList->Reset(frame.CmdAllocator, nullptr));
}

void 
D3D12UploadContext::EndUpload()
{
	assert(_frameIndex != U32_INVALID_ID);
	assert(_cmdList);
	UploadFrame& frame{ uploadFrames[_frameIndex] };
	DXCall(_cmdList->Close());

	std::lock_guard lock{ queueMutex };
	ID3D12CommandList* const cmdLists[]{ _cmdList };
	ID3D12CommandQueue* const cmdQueue{ uploadCmdQueue };

	assert(cmdQueue);
	assert(cmdLists);

	cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	++uploadFenceValue;
	frame.FenceValue = uploadFenceValue;
	DXCall(cmdQueue->Signal(uploadFence, frame.FenceValue));

	assert(uploadFence);
	assert(_cmdList);
	assert(_frameIndex != U32_INVALID_ID);
	assert(cmdQueue);
	assert(cmdLists);

	// wait for the copy queue to finish, then release the upload buffer
	frame.WaitAndReset();

	// mark the instance of this upload context as expired
	DEBUG_OP(new (this) D3D12UploadContext{});
}

bool 
Initialize()
{
	DXDevice* const device{ core::Device() };
	assert(device && !uploadCmdQueue);

	HRESULT hr{ S_OK };
	for (u32 i{ 0 }; i < UPLOAD_FRAME_COUNT; ++i)
	{
		UploadFrame& frame{ uploadFrames[i] };
		DXCall(hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&frame.CmdAllocator)));
		if (FAILED(hr)) return InitializeFailed();

		DXCall(hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, frame.CmdAllocator, nullptr, IID_PPV_ARGS(&frame.CmdList)));
		if (FAILED(hr)) return InitializeFailed();

		DXCall(frame.CmdList->Close());
		NAME_D3D12_OBJECT_INDEXED(frame.CmdAllocator, i, L"Upload Command Allocator");
		NAME_D3D12_OBJECT_INDEXED(frame.CmdList, i, L"Upload Command List");
	}

	D3D12_COMMAND_QUEUE_DESC desc{};
	desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;		
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	DXCall(hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&uploadCmdQueue)));
	if (FAILED(hr)) return InitializeFailed();
	NAME_D3D12_OBJECT(uploadCmdQueue, L"Upload Command Queue");

	DXCall(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence)));	
	if (FAILED(hr)) return InitializeFailed();
	NAME_D3D12_OBJECT(uploadFence, L"Upload Fence");

	fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	assert(fenceEvent);

	return true;
}

void 
Shutdown()
{
	for (u32 i{ 0 }; i < UPLOAD_FRAME_COUNT; ++i)
	{
		uploadFrames[i].Release();
	}

	if (fenceEvent)
	{
		CloseHandle(fenceEvent);
		fenceEvent = nullptr;
	}

	core::Release(uploadFence);
	core::Release(uploadCmdQueue);
	uploadFenceValue = 0;
}

}
