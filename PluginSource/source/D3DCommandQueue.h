#pragma once
#include <basetsd.h>
#include <d3d12.h>
#include <mutex>


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