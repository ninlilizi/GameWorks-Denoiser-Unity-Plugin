/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma region [  CoreInterface  ]

static void NRI_CALL SetCommandQueueDebugName(CommandQueue& commandQueue, const char* name)
{
    ((CommandQueueD3D11&)commandQueue).SetDebugName(name);
}

static void NRI_CALL SubmitQueueWork(CommandQueue& commandQueue, const WorkSubmissionDesc& workSubmissionDesc, DeviceSemaphore* deviceSemaphore)
{
    ((CommandQueueD3D11&)commandQueue).Submit(workSubmissionDesc, deviceSemaphore);
}

static void NRI_CALL WaitForSemaphore(CommandQueue& commandQueue, DeviceSemaphore& deviceSemaphore)
{
    ((CommandQueueD3D11&)commandQueue).Wait(deviceSemaphore);
}

void FillFunctionTableCommandQueueD3D11(CoreInterface& coreInterface)
{
    coreInterface.SetCommandQueueDebugName = ::SetCommandQueueDebugName;
    coreInterface.SubmitQueueWork = ::SubmitQueueWork;
    coreInterface.WaitForSemaphore = ::WaitForSemaphore;
}

#pragma endregion

#pragma region [  HelperInterface  ]

static Result NRI_CALL ChangeResourceStatesD3D11(CommandQueue& commandQueue, const TransitionBarrierDesc& transitionBarriers)
{
    return ((CommandQueueD3D11&)commandQueue).ChangeResourceStates(transitionBarriers);
}

static nri::Result NRI_CALL UploadDataD3D11(CommandQueue& commandQueue, const TextureUploadDesc* textureUploadDescs, uint32_t textureUploadDescNum,
    const BufferUploadDesc* bufferUploadDescs, uint32_t bufferUploadDescNum)
{
    return ((CommandQueueD3D11&)commandQueue).UploadData(textureUploadDescs, textureUploadDescNum, bufferUploadDescs, bufferUploadDescNum);
}

static nri::Result NRI_CALL WaitForIdleD3D11(CommandQueue& commandQueue)
{
    return ((CommandQueueD3D11&)commandQueue).WaitForIdle();
}

void FillFunctionTableCommandQueueD3D11(HelperInterface& helperInterface)
{
    helperInterface.ChangeResourceStates = ::ChangeResourceStatesD3D11;
    helperInterface.UploadData = ::UploadDataD3D11;
    helperInterface.WaitForIdle = ::WaitForIdleD3D11;
}

#pragma endregion
