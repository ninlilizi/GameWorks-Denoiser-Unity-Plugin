#include <d3d12.h>
#include <mutex>
#include <cassert>


class Direct3DQueue
{
public:
	Direct3DQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType);
	~Direct3DQueue();

	bool IsFenceComplete(UINT64 fenceValue);
	void InsertWait(UINT64 fenceValue);
	void InsertWaitForQueueFence(Direct3DQueue* otherQueue, UINT64 fenceValue);
	void InsertWaitForQueue(Direct3DQueue* otherQueue);

	void WaitForFenceCPUBlocking(UINT64 fenceValue);
	void WaitForIdle() { WaitForFenceCPUBlocking(mNextFenceValue - 1); }

	ID3D12CommandQueue* GetCommandQueue() { return mCommandQueue; }

	UINT64 PollCurrentFenceValue();
	UINT64 GetLastCompletedFence() { return mLastCompletedFenceValue; }
	UINT64 GetNextFenceValue() { return mNextFenceValue; }
	ID3D12Fence* GetFence() { return mFence; }

	UINT64 ExecuteCommandList(ID3D12CommandList* List);


	//private:
	ID3D12CommandQueue* mCommandQueue;
	D3D12_COMMAND_LIST_TYPE mQueueType;

	std::mutex mFenceMutex;
	std::mutex mEventMutex;

	ID3D12Fence* mFence;
	UINT64 mNextFenceValue;
	UINT64 mLastCompletedFenceValue;
	HANDLE mFenceEventHandle;
};

Direct3DQueue::Direct3DQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType)
{
	mQueueType = commandType;
	mCommandQueue = NULL;
	mFence = NULL;
	mNextFenceValue = ((uint64_t)mQueueType << 56) + 1;
	mLastCompletedFenceValue = ((uint64_t)mQueueType << 56);

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = mQueueType;
	queueDesc.NodeMask = 0;
	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));

	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));

	mFence->Signal(mLastCompletedFenceValue);

	mFenceEventHandle = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	assert(mFenceEventHandle != INVALID_HANDLE_VALUE);
}

Direct3DQueue::~Direct3DQueue()
{
	CloseHandle(mFenceEventHandle);

	mFence->Release();
	mFence = NULL;

	mCommandQueue->Release();
	mCommandQueue = NULL;
}

UINT64 Direct3DQueue::PollCurrentFenceValue()
{
	mLastCompletedFenceValue = max(mLastCompletedFenceValue, mFence->GetCompletedValue());
	return mLastCompletedFenceValue;
}

bool Direct3DQueue::IsFenceComplete(UINT64 fenceValue)
{
	if (fenceValue > mLastCompletedFenceValue)
	{
		PollCurrentFenceValue();
	}

	return fenceValue <= mLastCompletedFenceValue;
}

void Direct3DQueue::InsertWait(UINT64 fenceValue)
{
	mCommandQueue->Wait(mFence, fenceValue);
}

void Direct3DQueue::InsertWaitForQueueFence(Direct3DQueue* otherQueue, UINT64 fenceValue)
{
	mCommandQueue->Wait(otherQueue->GetFence(), fenceValue);
}

void Direct3DQueue::InsertWaitForQueue(Direct3DQueue* otherQueue)
{
	mCommandQueue->Wait(otherQueue->GetFence(), otherQueue->GetNextFenceValue() - 1);
}

void Direct3DQueue::WaitForFenceCPUBlocking(UINT64 fenceValue)
{
	if (IsFenceComplete(fenceValue))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lockGuard(mEventMutex);

		mFence->SetEventOnCompletion(fenceValue, mFenceEventHandle);
		WaitForSingleObjectEx(mFenceEventHandle, INFINITE, false);
		mLastCompletedFenceValue = fenceValue;
	}
}

UINT64 Direct3DQueue::ExecuteCommandList(ID3D12CommandList* commandList)
{
	((ID3D12GraphicsCommandList*)commandList)->Close();
	mCommandQueue->ExecuteCommandLists(1, &commandList);

	std::lock_guard<std::mutex> lockGuard(mFenceMutex);

	mCommandQueue->Signal(mFence, mNextFenceValue);

	return mNextFenceValue++;
}


class Direct3DQueueManager
{
public:
	Direct3DQueueManager(ID3D12Device* device);
	~Direct3DQueueManager();

	Direct3DQueue* GetGraphicsQueue() { return mGraphicsQueue; }
	Direct3DQueue* GetComputeQueue() { return mComputeQueue; }
	Direct3DQueue* GetCopyQueue() { return mCopyQueue; }

	Direct3DQueue* GetQueue(D3D12_COMMAND_LIST_TYPE commandType);

	bool IsFenceComplete(UINT64 fenceValue);
	void WaitForFenceCPUBlocking(UINT64 fenceValue);
	void WaitForAllIdle();


private:
	Direct3DQueue* mGraphicsQueue;
	Direct3DQueue* mComputeQueue;
	Direct3DQueue* mCopyQueue;
};

Direct3DQueueManager::Direct3DQueueManager(ID3D12Device* device)
{
	mGraphicsQueue = new Direct3DQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	mComputeQueue = new Direct3DQueue(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	mCopyQueue = new Direct3DQueue(device, D3D12_COMMAND_LIST_TYPE_COPY);
}

Direct3DQueueManager::~Direct3DQueueManager()
{
	delete mGraphicsQueue;
	delete mComputeQueue;
	delete mCopyQueue;
}

Direct3DQueue* Direct3DQueueManager::GetQueue(D3D12_COMMAND_LIST_TYPE commandType)
{
	switch (commandType)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		return mGraphicsQueue;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		return mComputeQueue;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		return mCopyQueue;
		//default:
			//Direct3DUtils::ThrowRuntimeError("Bad command type lookup in queue manager.");
	}

	return NULL;
}

bool Direct3DQueueManager::IsFenceComplete(UINT64 fenceValue)
{
	return GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56))->IsFenceComplete(fenceValue);
}

void Direct3DQueueManager::WaitForFenceCPUBlocking(UINT64 fenceValue)
{
	Direct3DQueue* commandQueue = GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56));
	commandQueue->WaitForFenceCPUBlocking(fenceValue);
}

void Direct3DQueueManager::WaitForAllIdle()
{
	mGraphicsQueue->WaitForIdle();
	mComputeQueue->WaitForIdle();
	mCopyQueue->WaitForIdle();
}

