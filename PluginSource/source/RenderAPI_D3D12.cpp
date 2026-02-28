#define NOMINMAX

#pragma comment(lib, "d3d12")

#include "RenderAPI.h"
#include "PlatformBase.h"
#include "NRDDenoiserConfig.h"

#include <cmath>

// Direct3D 12 implementation of RenderAPI.


#if SUPPORT_D3D12

#include <cassert>
#include <d3d12.h>
#include "Unity/IUnityGraphicsD3D12.h"

#include "D3DCommandQueue.h"

// Gameworks - NRI headers (must be included before NRD integration
// so that NRI_WRAPPER_D3D12_H is defined when NRDIntegration.hpp is parsed)
// Use the NRI fetched by NRD's CMake (v178) — must match the built NRI.dll
#include "../NRD/_Build/_deps/nri-src/Include/NRI.h"
#include "../NRD/_Build/_deps/nri-src/Include/NRIDescs.h"
#include "../NRD/_Build/_deps/nri-src/Include/Extensions/NRIWrapperD3D12.h"
#include "../NRD/_Build/_deps/nri-src/Include/Extensions/NRIHelper.h"

// Gameworks - NRD headers
#include "../NRD/Include/NRD.h"
#include "../NRD/Include/NRDSettings.h"

#include "../NRD/Integration/NRDIntegration.hpp"

#include <source/DX12/d3dx12_core.h>
#include <source/DX12/d3dx12_barriers.h>


// Helper: resolve DXGI typeless formats to compatible typed formats.
// Unity creates render textures as TYPELESS; NRI requires a typed format for SRV/UAV creation.
static DXGI_FORMAT ResolveTypelessFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case DXGI_FORMAT_R32G32B32_TYPELESS:    return DXGI_FORMAT_R32G32B32_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case DXGI_FORMAT_R32G32_TYPELESS:       return DXGI_FORMAT_R32G32_FLOAT;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:  return DXGI_FORMAT_R10G10B10A2_UNORM;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:     return DXGI_FORMAT_R8G8B8A8_UNORM;
	case DXGI_FORMAT_R16G16_TYPELESS:       return DXGI_FORMAT_R16G16_FLOAT;
	case DXGI_FORMAT_R32_TYPELESS:          return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R8G8_TYPELESS:         return DXGI_FORMAT_R8G8_UNORM;
	case DXGI_FORMAT_R16_TYPELESS:          return DXGI_FORMAT_R16_FLOAT;
	case DXGI_FORMAT_R8_TYPELESS:           return DXGI_FORMAT_R8_UNORM;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:     return DXGI_FORMAT_B8G8R8A8_UNORM;
	default:                                return format;
	}
}

// Helper: create an nrd::Resource from a D3D12 resource pointer
static nrd::Resource MakeD3D12Resource(void* ptr)
{
	nrd::Resource res = {};
	ID3D12Resource* d3dRes = (ID3D12Resource*)ptr;
	res.d3d12.resource = d3dRes;

	// Unity creates render textures with typeless DXGI formats;
	// NRI requires a compatible typed format for SRV/UAV creation.
	D3D12_RESOURCE_DESC desc = d3dRes->GetDesc();
	res.d3d12.format = (DXGIFormat)ResolveTypelessFormat(desc.Format);

	res.state.access = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	res.state.layout = nri::Layout::GENERAL;
	res.state.stages = nri::StageBits::ALL;
	return res;
}


// Per-denoiser runtime state — lazily initialized
struct DenoiserSlot
{
	nrd::Integration integration;
	ID3D12CommandAllocator* cmdAlloc = nullptr;
	ID3D12GraphicsCommandList* cmdList = nullptr;
	void* resources[MAX_DENOISER_RESOURCES] = {};
	UnityGraphicsD3D12ResourceState outputStates[MAX_OUTPUT_RESOURCES] = {};
	int outputStateCount = 0;
	int width = 0;
	int height = 0;
	bool cmdObjectsCreated = false;
	UINT64 lastFenceValue = 0; // Fence value from last ExecuteCommandList — used to wait for GPU completion before release
};


const UINT kNodeMask = 0;


class RenderAPI_D3D12 : public RenderAPI
{
public:
	RenderAPI_D3D12();
	virtual ~RenderAPI_D3D12() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

private:
	void ReleaseResources();
	bool NRDInitialize(int denoiserType, int renderWidth, int renderHeight, void** resources, int resourceCount);
	void NRDDenoise(int denoiserType);
	void NRDRelease(int denoiserType);
	void NRDReleaseAllSlots();
	void SetMatrix(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16]);

	bool CreateCommandObjects(DenoiserSlot& slot);
	void ApplyDenoiserSettings(int type, DenoiserSlot& slot);
	void SetCommonSettings();
	int GetLastInitError() override { return m_lastInitError; }

private:
	int m_lastInitError = 0;
	IUnityGraphicsD3D12v4* s_D3D12;
	nrd::CommonSettings commonSettings;
	DenoiserSlot m_slots[NRD_DENOISER_COUNT];

	float m_viewToClipMatrixPrev[16];
	float m_worldToViewMatrixPrev[16];
};


RenderAPI* CreateRenderAPI_D3D12()
{
	return new RenderAPI_D3D12();
}


RenderAPI_D3D12::RenderAPI_D3D12()
	: s_D3D12(NULL)
	, commonSettings()
	, m_slots()
	, m_viewToClipMatrixPrev()
	, m_worldToViewMatrixPrev()
{
}


bool RenderAPI_D3D12::CreateCommandObjects(DenoiserSlot& slot)
{
	ID3D12Device* device = s_D3D12->GetDevice();
	HRESULT hr;

	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slot.cmdAlloc));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to CreateCommandAllocator.\n");
		return false;
	}

	hr = device->CreateCommandList(kNodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT, slot.cmdAlloc, nullptr, IID_PPV_ARGS(&slot.cmdList));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to CreateCommandList.\n");
		SAFE_RELEASE(slot.cmdAlloc);
		return false;
	}

	slot.cmdList->Close();
	slot.cmdObjectsCreated = true;
	return true;
}


void RenderAPI_D3D12::SetCommonSettings()
{
	// Don't zero commonSettings — it may contain matrices from SetMatrix()
	commonSettings.isBaseColorMetalnessAvailable = true;
	commonSettings.isMotionVectorInWorldSpace = false;
	commonSettings.motionVectorScale[0] = 1.0f;
	commonSettings.motionVectorScale[1] = 1.0f;
	commonSettings.motionVectorScale[2] = 0.0f;
}


void RenderAPI_D3D12::ApplyDenoiserSettings(int type, DenoiserSlot& slot)
{
	const DenoiserTypeDesc& desc = g_DenoiserTypeDescs[type];

	switch (desc.settingsFamily)
	{
	case SettingsFamily::REBLUR:
	{
		nrd::ReblurSettings settings = {};
		settings.enableAntiFirefly = false;
		settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::AREA_3X3;
		slot.integration.SetDenoiserSettings(0, &settings);
		break;
	}
	case SettingsFamily::RELAX:
	{
		nrd::RelaxSettings settings = {};
		settings.enableAntiFirefly = true;
		settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::AREA_3X3;
		slot.integration.SetDenoiserSettings(0, &settings);
		break;
	}
	case SettingsFamily::SIGMA:
	{
		nrd::SigmaSettings settings = {};
		slot.integration.SetDenoiserSettings(0, &settings);
		break;
	}
	case SettingsFamily::REFERENCE:
	{
		nrd::ReferenceSettings settings = {};
		slot.integration.SetDenoiserSettings(0, &settings);
		break;
	}
	}
}


bool RenderAPI_D3D12::NRDInitialize(int denoiserType, int renderWidth, int renderHeight, void** resources, int resourceCount)
{
	if (denoiserType < 0 || denoiserType >= NRD_DENOISER_COUNT)
	{
		m_lastInitError = 1;
		return false;
	}

	const DenoiserTypeDesc& desc = g_DenoiserTypeDescs[denoiserType];
	if (resourceCount != desc.resourceCount)
	{
		m_lastInitError = 3;
		return false;
	}

	DenoiserSlot& slot = m_slots[denoiserType];

	// Lazy-create command objects on first use
	if (!slot.cmdObjectsCreated)
	{
		if (!CreateCommandObjects(slot))
		{
			m_lastInitError = 4;
			return false;
		}
	}

	// Store dimensions and resource pointers
	slot.width = renderWidth;
	slot.height = renderHeight;
	for (int i = 0; i < resourceCount; i++)
		slot.resources[i] = resources[i];

	// Configure denoiser
	nrd::DenoiserDesc denoiserDesc = {};
	denoiserDesc.identifier = 0;
	denoiserDesc.denoiser = desc.denoiser;

	nrd::InstanceCreationDesc instanceDesc = {};
	instanceDesc.denoisers = &denoiserDesc;
	instanceDesc.denoisersNum = 1;

	nrd::IntegrationCreationDesc integrationDesc = {};
	integrationDesc.resourceWidth = (uint16_t)renderWidth;
	integrationDesc.resourceHeight = (uint16_t)renderHeight;

	// Set up D3D12 device with queue family
	ID3D12CommandQueue* queue = s_D3D12->GetCommandQueue();
	nri::QueueFamilyD3D12Desc queueFamily = {};
	queueFamily.d3d12Queues = &queue;
	queueFamily.queueNum = 1;
	queueFamily.queueType = nri::QueueType::GRAPHICS;

	nri::DeviceCreationD3D12Desc deviceDesc = {};
	deviceDesc.d3d12Device = s_D3D12->GetDevice();
	deviceDesc.queueFamilies = &queueFamily;
	deviceDesc.queueFamilyNum = 1;
	deviceDesc.enableNRIValidation = false;
	deviceDesc.disableD3D12EnhancedBarriers = true; // Unity uses legacy barriers

	nrd::Result result = slot.integration.RecreateD3D12(integrationDesc, instanceDesc, deviceDesc);
	if (result != nrd::Result::SUCCESS)
	{
		m_lastInitError = 5;
		return false;
	}
	m_lastInitError = 0;

	SetCommonSettings();
	ApplyDenoiserSettings(denoiserType, slot);

	// Build output resource states for Unity
	slot.outputStateCount = 0;
	for (int i = 0; i < desc.resourceCount; i++)
	{
		if (desc.resources[i].isOutput)
		{
			UnityGraphicsD3D12ResourceState& state = slot.outputStates[slot.outputStateCount++];
			state = {};
			state.resource = (ID3D12Resource*)resources[i];
			state.expected = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			state.current = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
	}

	return true;
}


void RenderAPI_D3D12::NRDDenoise(int denoiserType)
{
	if (denoiserType < 0 || denoiserType >= NRD_DENOISER_COUNT)
		return;

	DenoiserSlot& slot = m_slots[denoiserType];

	// Ensure the GPU has finished executing the previous command list for
	// this slot before resetting the allocator. D3D12 forbids resetting a
	// command allocator while the GPU is still reading from it — doing so
	// corrupts in-flight commands and causes a device hang (TDR).
	if (slot.lastFenceValue > 0 && s_D3D12 != nullptr)
	{
		ID3D12Fence* frameFence = s_D3D12->GetFrameFence();
		if (frameFence && frameFence->GetCompletedValue() < slot.lastFenceValue)
			return; // GPU still busy with previous denoise — skip this frame
	}

	// Reset and begin recording
	slot.cmdAlloc->Reset();
	slot.cmdList->Reset(slot.cmdAlloc, NULL);

	// Update NRD per frame
	slot.integration.NewFrame();

	// Make local copy of common settings with per-slot dimensions
	nrd::CommonSettings localSettings = commonSettings;
	localSettings.resourceSize[0] = (uint16_t)slot.width;
	localSettings.resourceSize[1] = (uint16_t)slot.height;
	localSettings.resourceSizePrev[0] = (uint16_t)slot.width;
	localSettings.resourceSizePrev[1] = (uint16_t)slot.height;
	localSettings.rectSize[0] = (uint16_t)slot.width;
	localSettings.rectSize[1] = (uint16_t)slot.height;
	localSettings.rectSizePrev[0] = (uint16_t)slot.width;
	localSettings.rectSizePrev[1] = (uint16_t)slot.height;

	slot.integration.SetCommonSettings(localSettings);

	// Build resource snapshot from table
	const DenoiserTypeDesc& desc = g_DenoiserTypeDescs[denoiserType];
	nrd::ResourceSnapshot snapshot;
	snapshot.restoreInitialState = true;

	for (int i = 0; i < desc.resourceCount; i++)
		snapshot.SetResource(desc.resources[i].type, MakeD3D12Resource(slot.resources[i]));

	// Build command buffer desc
	nri::CommandBufferD3D12Desc cmdBufferDesc = {};
	cmdBufferDesc.d3d12CommandList = slot.cmdList;
	cmdBufferDesc.d3d12CommandAllocator = slot.cmdAlloc;

	// Denoise
	nrd::Identifier id = 0;
	slot.integration.DenoiseD3D12(&id, 1, cmdBufferDesc, snapshot);

	slot.cmdList->Close();
	slot.lastFenceValue = s_D3D12->ExecuteCommandList(slot.cmdList, slot.outputStateCount, slot.outputStates);
}


void RenderAPI_D3D12::SetMatrix(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16])
{
	commonSettings.frameIndex = frameIndex;

	// Set matrices
	for (int i = 0; i < 16; ++i)
	{
		commonSettings.viewToClipMatrixPrev[i] = m_viewToClipMatrixPrev[i];
		commonSettings.viewToClipMatrix[i] = _viewToClipMatrix[i];
		m_viewToClipMatrixPrev[i] = _viewToClipMatrix[i];

		commonSettings.worldToViewMatrixPrev[i] = m_worldToViewMatrixPrev[i];
		commonSettings.worldToViewMatrix[i] = _worldToViewMatrix[i];
		m_worldToViewMatrixPrev[i] = _worldToViewMatrix[i];
	}
}


void RenderAPI_D3D12::NRDRelease(int denoiserType)
{
	if (denoiserType < 0 || denoiserType >= NRD_DENOISER_COUNT)
		return;

	DenoiserSlot& slot = m_slots[denoiserType];

	// Wait for the GPU to finish executing the last NRD command list
	// before destroying NRI/NRD resources. Without this, the GPU may
	// still be reading from D3D12 resources that are about to be freed,
	// causing a DEVICE_REMOVED error.
	if (slot.lastFenceValue > 0 && s_D3D12 != nullptr)
	{
		ID3D12Fence* frameFence = s_D3D12->GetFrameFence();
		if (frameFence && frameFence->GetCompletedValue() < slot.lastFenceValue)
		{
			HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
			if (event)
			{
				frameFence->SetEventOnCompletion(slot.lastFenceValue, event);
				WaitForSingleObject(event, 5000); // 5s timeout to avoid infinite hang
				CloseHandle(event);
			}
		}
		slot.lastFenceValue = 0;
	}

	slot.integration.Destroy();
}


void RenderAPI_D3D12::NRDReleaseAllSlots()
{
	if (s_D3D12 == nullptr)
		return;

	// Phase 1: Find max fence value across ALL slots, wait once
	UINT64 maxFenceValue = 0;
	for (int i = 0; i < NRD_DENOISER_COUNT; i++)
		if (m_slots[i].lastFenceValue > maxFenceValue)
			maxFenceValue = m_slots[i].lastFenceValue;

	if (maxFenceValue > 0)
	{
		ID3D12Fence* frameFence = s_D3D12->GetFrameFence();
		if (frameFence && frameFence->GetCompletedValue() < maxFenceValue)
		{
			HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
			if (event)
			{
				frameFence->SetEventOnCompletion(maxFenceValue, event);
				WaitForSingleObject(event, 5000);
				CloseHandle(event);
			}
		}
	}

	// Phase 2: GPU is idle — destroy all integrations
	for (int i = 0; i < NRD_DENOISER_COUNT; i++)
	{
		m_slots[i].lastFenceValue = 0;
		m_slots[i].integration.Destroy();
	}
}


void RenderAPI_D3D12::ReleaseResources()
{
	NRDReleaseAllSlots();
	for (int i = 0; i < NRD_DENOISER_COUNT; i++)
	{
		SAFE_RELEASE(m_slots[i].cmdList);
		SAFE_RELEASE(m_slots[i].cmdAlloc);
		m_slots[i].cmdObjectsCreated = false;
	}
}


void RenderAPI_D3D12::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
		s_D3D12 = interfaces->Get<IUnityGraphicsD3D12v4>();
		break;
	case kUnityGfxDeviceEventShutdown:
		ReleaseResources();
		break;
	}
}

#endif // #if SUPPORT_D3D12
