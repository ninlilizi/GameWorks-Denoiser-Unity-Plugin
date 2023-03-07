#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d12")

#include "RenderAPI.h"
#include "PlatformBase.h"

#include <cmath>

// Direct3D 12 implementation of RenderAPI.


#if SUPPORT_D3D12

#include <assert.h>
#include <d3d12.h>
#include "Unity/IUnityGraphicsD3D12.h"

#include <dxgi.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;


// Gameworkds
#include "../NRI/_NRI_SDK/Include/NRI.h"
#include "../NRI/_NRI_SDK/Include/NRIDescs.h"
#include "../NRI/_NRI_SDK/Include/Extensions/NRIWrapperD3D12.h"
#include "../NRI/_NRI_SDK/Include/Extensions/NRIHelper.h"
#include "../NRD/_NRD_SDK/Include/NRD.h"
#include "../NRD/_NRD_SDK/Integration/NRDIntegration.hpp"


const int maxNumberOfFramesInFlight = 3;
NrdIntegration NRD = NrdIntegration(maxNumberOfFramesInFlight);

struct NriInterface
	: public nri::CoreInterface
	, public nri::HelperInterface
	, public nri::WrapperD3D12Interface
{};
NriInterface NRI;

class RenderAPI_D3D12 : public RenderAPI
{
public:
	RenderAPI_D3D12();
	virtual ~RenderAPI_D3D12() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual bool GetUsesReverseZ() { return true; }

private:
	void CreateResources();
	void ReleaseResources();
	void ReleaseTextures();
	void Initialize(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST);
	void InitializeNRD(int renderWidth, int renderHeight);
	void Denoise(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16]);

private:
	IUnityGraphicsD3D12v4* s_D3D12;
	ID3D12Resource* s_D3D12Upload;
	ID3D12CommandAllocator* s_D3D12CmdAlloc;
	ID3D12GraphicsCommandList* s_D3D12CmdList;
	IDXGIAdapter1* s_IDXGIAdapter;
	UINT64 s_D3D12FenceValue = 0;
	HANDLE s_D3D12Event = NULL;

	nri::Device* s_nriDevice;
	nri::CommandBuffer* s_nriCommandBuffer;
	nri::TextureTransitionBarrierDesc textureDescs[5];

	bool nrdInitalized = false;

	float m_viewToClipMatrixPrev[16];
	float m_worldToViewMatrixPrev[16];

	void* tex_IN_MV;
	void* tex_IN_NORMAL_ROUGHNESS;
	void* tex_IN_VIEWZ;
	void* tex_IN_DIFF_RADIANCE_HITDIST;
	void* tex_OUT_DIFF_RADIANCE_HITDIST;
};


RenderAPI* CreateRenderAPI_D3D12()
{
	return new RenderAPI_D3D12();
}


const UINT kNodeMask = 0;


RenderAPI_D3D12::RenderAPI_D3D12()
	: s_D3D12(NULL)
	, s_D3D12Upload(NULL)
	, s_D3D12CmdAlloc(NULL)
	, s_D3D12CmdList(NULL)
	, s_IDXGIAdapter(NULL)
	, s_D3D12FenceValue(0)
	, s_D3D12Event(NULL)
	, s_nriDevice(NULL)
	, s_nriCommandBuffer(NULL)
	, textureDescs()
{
}


void RenderAPI_D3D12::CreateResources()
{
	ID3D12Device* device = s_D3D12->GetDevice();

	HRESULT hr = E_FAIL;

	// Command list
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&s_D3D12CmdAlloc));
	if (FAILED(hr)) OutputDebugStringA("Failed to CreateCommandAllocator.\n");
	hr = device->CreateCommandList(kNodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT, s_D3D12CmdAlloc, nullptr, IID_PPV_ARGS(&s_D3D12CmdList));
	if (FAILED(hr)) OutputDebugStringA("Failed to CreateCommandList.\n");
	s_D3D12CmdList->Close();

	// Fence
	s_D3D12FenceValue = 0;
	s_D3D12Event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// DXGI
	ComPtr<IDXGIFactory4> dxgiFactory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(adapterIndex, &s_IDXGIAdapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		hr = s_IDXGIAdapter->GetDesc1(&desc);
		if (FAILED(hr))
			continue;

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12,
		// but don't create the actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(s_IDXGIAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}
	if (s_IDXGIAdapter == nullptr) OutputDebugStringA("Failed to find DXGI adapter");

	// Wrap the device
	nri::DeviceCreationD3D12Desc deviceDesc = {};
	deviceDesc.d3d12Device = device;
	deviceDesc.d3d12PhysicalAdapter = s_IDXGIAdapter;
	deviceDesc.d3d12GraphicsQueue = s_D3D12->GetCommandQueue();
	deviceDesc.enableNRIValidation = false;

	nri::Result nriResult = nri::CreateDeviceFromD3D12Device(deviceDesc, s_nriDevice);

	// Get core functionality
	nriResult = nri::GetInterface(*s_nriDevice, NRI_INTERFACE(nri::CoreInterface), (nri::CoreInterface*)&NRI);

	nriResult = nri::GetInterface(*s_nriDevice, NRI_INTERFACE(nri::HelperInterface), (nri::HelperInterface*)&NRI);

	// Get appropriate "wrapper" extension (XXX - can be D3D11, D3D12 or VULKAN)
	nriResult = nri::GetInterface(*s_nriDevice, NRI_INTERFACE(nri::WrapperD3D12Interface), (nri::WrapperD3D12Interface*)&NRI);

	// Initialize NRD
	InitializeNRD(256, 256);
}


void RenderAPI_D3D12::InitializeNRD(int renderWidth, int renderHeight)
{
	// Initialize NRD
	const nrd::MethodDesc methodDescs[] =
	{
		// Put neeeded methods here, like:
		{ nrd::Method::REBLUR_DIFFUSE, renderWidth, renderHeight }
	};

	nrd::DenoiserCreationDesc denoiserCreationDesc = {};
	denoiserCreationDesc.requestedMethods = methodDescs;
	denoiserCreationDesc.requestedMethodsNum = 1;

	bool result = NRD.Initialize(denoiserCreationDesc, *s_nriDevice, NRI, NRI);

	nrdInitalized = true;
}

// INPUTS - IN_MV, IN_NORMAL_ROUGHNESS, IN_VIEWZ, IN_DIFF_RADIANCE_HITDIST,
void RenderAPI_D3D12::Initialize(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	if (nrdInitalized)
	{
		// Release resources before recreating
		ReleaseTextures();
		NRD.Destroy();
		InitializeNRD(renderWidth, renderHeight);

		tex_IN_MV = &IN_MV;
		tex_IN_NORMAL_ROUGHNESS = &IN_NORMAL_ROUGHNESS;
		tex_IN_VIEWZ = &IN_VIEWZ;
		tex_IN_DIFF_RADIANCE_HITDIST = &IN_DIFF_RADIANCE_HITDIST;
		tex_OUT_DIFF_RADIANCE_HITDIST = &OUT_DIFF_RADIANCE_HITDIST;


		// Wrap pointers

		// Wrap the command buffer
		nri::CommandBufferD3D12Desc commandBufferDesc = {};
		commandBufferDesc.d3d12CommandList = (ID3D12GraphicsCommandList*)s_D3D12CmdList;

		// Not needed for NRD integration layer, but needed for NRI validation layer
		commandBufferDesc.d3d12CommandAllocator = (ID3D12CommandAllocator*)s_D3D12CmdAlloc;

		//nri::CommandBuffer* nriCommandBuffer = nullptr;
		NRI.CreateCommandBufferD3D12(*s_nriDevice, commandBufferDesc, s_nriCommandBuffer);

		// Wrap required textures (better do it only once on initialization)
		nri::TextureD3D12Desc textureDescIN_MV = {};
		textureDescIN_MV.d3d12Resource = (ID3D12Resource*&)IN_MV;
		NRI.CreateTextureD3D12(*s_nriDevice, textureDescIN_MV, (nri::Texture*&)textureDescs[0].texture);
		textureDescs[0].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
		textureDescs[0].nextLayout = nri::TextureLayout::GENERAL;

		nri::TextureD3D12Desc textureDescIN_NORMAL_ROUGHNESS = {};
		textureDescIN_NORMAL_ROUGHNESS.d3d12Resource = (ID3D12Resource*&)IN_NORMAL_ROUGHNESS;
		NRI.CreateTextureD3D12(*s_nriDevice, textureDescIN_NORMAL_ROUGHNESS, (nri::Texture*&)textureDescs[1].texture);
		textureDescs[1].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
		textureDescs[1].nextLayout = nri::TextureLayout::GENERAL;

		nri::TextureD3D12Desc textureDescIN_VIEWZ = {};
		textureDescIN_VIEWZ.d3d12Resource = (ID3D12Resource*&)IN_VIEWZ;
		NRI.CreateTextureD3D12(*s_nriDevice, textureDescIN_VIEWZ, (nri::Texture*&)textureDescs[2].texture);
		textureDescs[2].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
		textureDescs[2].nextLayout = nri::TextureLayout::GENERAL;

		nri::TextureD3D12Desc textureDescIN_DIFF_RADIANCE_HITDIST = {};
		textureDescIN_DIFF_RADIANCE_HITDIST.d3d12Resource = (ID3D12Resource*&)IN_DIFF_RADIANCE_HITDIST;
		NRI.CreateTextureD3D12(*s_nriDevice, textureDescIN_DIFF_RADIANCE_HITDIST, (nri::Texture*&)textureDescs[3].texture);
		textureDescs[3].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
		textureDescs[3].nextLayout = nri::TextureLayout::GENERAL;

		nri::TextureD3D12Desc textureDescOUT_DIFF_RADIANCE_HITDIST = {};
		textureDescOUT_DIFF_RADIANCE_HITDIST.d3d12Resource = (ID3D12Resource*&)OUT_DIFF_RADIANCE_HITDIST;
		NRI.CreateTextureD3D12(*s_nriDevice, textureDescOUT_DIFF_RADIANCE_HITDIST, (nri::Texture*&)textureDescs[4].texture);
		textureDescs[4].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
		textureDescs[4].nextLayout = nri::TextureLayout::GENERAL;
	}
}


void RenderAPI_D3D12::Denoise(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16])
{
	// Populate common settings
	//  - for the first time use defaults
	//  - currently NRD supports only the following view space: X - right, Y - top, Z - forward or backward
	nrd::CommonSettings commonSettings = {};
	commonSettings.frameIndex = frameIndex;
	commonSettings.isBaseColorMetalnessAvailable = false;

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


	// Set settings for each method in the NRD instance
	nrd::ReblurSettings settings = {};
	settings.enableAntiFirefly = false;


	NRD.SetMethodSettings(nrd::Method::REBLUR_DIFFUSE, &settings);


	NrdIntegrationTexture integrationTexture_IN_MV;
	integrationTexture_IN_MV.format = nri::Format::RGBA32_SFLOAT;
	integrationTexture_IN_MV.subresourceStates = &textureDescs[0];

	NrdIntegrationTexture integrationTexture_IN_NORMAL_ROUGHNESS;
	integrationTexture_IN_NORMAL_ROUGHNESS.format = nri::Format::RGBA32_SFLOAT;
	integrationTexture_IN_NORMAL_ROUGHNESS.subresourceStates = &textureDescs[1];

	NrdIntegrationTexture integrationTexture_IN_VIEWZ;
	integrationTexture_IN_VIEWZ.format = nri::Format::RGBA32_SFLOAT;
	integrationTexture_IN_VIEWZ.subresourceStates = &textureDescs[2];

	NrdIntegrationTexture integrationTexture_IN_DIFF_RADIANCE_HITDIST;
	integrationTexture_IN_DIFF_RADIANCE_HITDIST.format = nri::Format::RGBA32_SFLOAT;
	integrationTexture_IN_DIFF_RADIANCE_HITDIST.subresourceStates = &textureDescs[3];

	NrdIntegrationTexture integrationTexture_OUT_DIFF_RADIANCE_HITDIST;
	integrationTexture_OUT_DIFF_RADIANCE_HITDIST.format = nri::Format::RGBA32_SFLOAT;
	integrationTexture_OUT_DIFF_RADIANCE_HITDIST.subresourceStates = &textureDescs[4];

	// Fill up the user pool
	NrdUserPool userPool = {};
	{
		// Fill only required "in-use" inputs and outputs in appropriate slots using entryDescs & entryFormat,
		// applying remapping if necessary. Unused slots will be {nullptr, nri::Format::UNKNOWN}
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_MV, integrationTexture_IN_MV);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_NORMAL_ROUGHNESS, integrationTexture_IN_NORMAL_ROUGHNESS);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_VIEWZ, integrationTexture_IN_VIEWZ);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, integrationTexture_IN_DIFF_RADIANCE_HITDIST);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, integrationTexture_OUT_DIFF_RADIANCE_HITDIST);
	};

	// Better use "true" if resources are not changing between frames (i.e. are not suballocated from a heap)
	bool enableDescriptorCaching = true;

	NRD.Denoise(frameIndex, *s_nriCommandBuffer, commonSettings, userPool, enableDescriptorCaching);
}

void RenderAPI_D3D12::ReleaseTextures()
{
	// Better do it only once on shutdown
	for (uint32_t i = 0; i < 5; i++)
	{
		if (textureDescs[i].texture != nullptr) NRI.DestroyTexture(*(nri::Texture*&)textureDescs[i].texture);
	}

	if (s_nriCommandBuffer != nullptr) NRI.DestroyCommandBuffer(*s_nriCommandBuffer);
}


void RenderAPI_D3D12::ReleaseResources()
{
	SAFE_RELEASE(s_D3D12Upload);
	if (s_D3D12Event)
		CloseHandle(s_D3D12Event);
	SAFE_RELEASE(s_D3D12CmdList);
	SAFE_RELEASE(s_D3D12CmdAlloc);
	SAFE_RELEASE(s_IDXGIAdapter);
	s_nriDevice = nullptr;

	ReleaseTextures();

	// Release wrapped device
	//NRI.DestroyDevice(*s_nriDevice);

	// Also NRD needs to be recreated on "resize"
	NRD.Destroy();
}


void RenderAPI_D3D12::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
		s_D3D12 = interfaces->Get<IUnityGraphicsD3D12v4>();
		CreateResources();
		break;
	case kUnityGfxDeviceEventShutdown:
		ReleaseResources();
		break;
	}
}

#endif // #if SUPPORT_D3D12
