#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d12")

#include "RenderAPI.h"
#include "PlatformBase.h"

#include <cmath>

// Direct3D 12 implementation of RenderAPI.


#if SUPPORT_D3D12

#include <cassert>
#include <d3d12.h>
#include "Unity/IUnityGraphicsD3D12.h"

#include "D3DCommandQueue.h"

#include <dxgi.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;


// Gameworks
#include "../NRI/Include/NRI.h"
#include "../NRI/Include/NRIDescs.h"
#include "../NRI/Include/Extensions/NRIWrapperD3D12.h"
#include "../NRI/Include/Extensions/NRIHelper.h"
#include "../NRD/Include/NRD.h"
#include "../NRD/Integration/NRDIntegration.hpp"
#include <source/DX12/d3dx12_core.h>
#include <source/DX12/d3dx12_barriers.h>


const int maxNumberOfFramesInFlight = 1;
NrdIntegration NRD = NrdIntegration(maxNumberOfFramesInFlight);
NrdIntegration NRDSigma = NrdIntegration(maxNumberOfFramesInFlight);

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

private:
	void CreateResources();
	void ReleaseResources();
	void ReleaseResourcesSigma();
	void ReleaseTextures();
	void ReleaseTexturesSigma();
	void Initialize(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST);
	void InitializeSigma(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_SHADOWDATA, void* IN_SHADOW_TRANSLUCENCY, void* OUT_SHADOW_TRANSLUCENCY);
	void Denoise();
	void DenoiseSigma();
	void SetMatrix(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16]);
	void ReleaseNRD();
	void ReleaseNRDSigma();


private:
	IUnityGraphicsD3D12v4* s_D3D12;
	ID3D12Resource* s_D3D12Upload;
	ID3D12CommandAllocator* s_D3D12CmdAlloc;
	ID3D12CommandAllocator* s_D3D12CmdAllocSigma;
	ID3D12GraphicsCommandList* s_D3D12CmdList;
	ID3D12GraphicsCommandList* s_D3D12CmdListSigma;
	IDXGIAdapter1* s_IDXGIAdapter;
	UINT64 s_D3D12FenceValue = 0;
	HANDLE s_D3D12Event = NULL;

	nri::Device* s_nriDevice;
	nri::CommandBuffer* s_nriCommandBuffer;
	nri::CommandBuffer* s_nriCommandBufferSigma;
	nri::TextureTransitionBarrierDesc textureDescs[5];
	nri::TextureTransitionBarrierDesc textureDescsSigma[5];

	NrdUserPool userPool;
	NrdUserPool userPoolSigma;
	nrd::CommonSettings commonSettings;
	nrd::RelaxDiffuseSettings relaxSettings;
	nrd::SigmaSettings sigmaSettings;
	UnityGraphicsD3D12ResourceState resourceState;
	UnityGraphicsD3D12ResourceState resourceStateSigma;

	nri::TextureD3D12Desc textureDesc_IN_MV;
	nri::TextureD3D12Desc textureDesc_IN_NORMAL_ROUGHNESS;
	nri::TextureD3D12Desc textureDesc_IN_VIEWZ;
	nri::TextureD3D12Desc textureDesc_IN_DIFF_RADIANCE_HITDIST;
	nri::TextureD3D12Desc textureDesc_OUT_DIFF_RADIANCE_HITDIST;

	nri::TextureD3D12Desc textureDesc_Sigma_IN_MV;
	nri::TextureD3D12Desc textureDesc_Sigma_IN_NORMAL_ROUGHNESS;
	nri::TextureD3D12Desc textureDesc_Sigma_IN_SHADOWDATA;
	nri::TextureD3D12Desc textureDesc_Sigma_IN_SHADOW_TRANSLUCENCY;
	nri::TextureD3D12Desc textureDesc_Sigma_OUT_SHADOW_TRANSLUCENCY;

	// Integration textures
	NrdIntegrationTexture integrationTexture_IN_MV;
	NrdIntegrationTexture integrationTexture_IN_NORMAL_ROUGHNESS;
	NrdIntegrationTexture integrationTexture_IN_VIEWZ;
	NrdIntegrationTexture integrationTexture_IN_DIFF_RADIANCE_HITDIST;
	NrdIntegrationTexture integrationTexture_OUT_DIFF_RADIANCE_HITDIST;

	NrdIntegrationTexture integrationTexture_Sigma_IN_MV;
	NrdIntegrationTexture integrationTexture_Sigma_IN_NORMAL_ROUGHNESS;
	NrdIntegrationTexture integrationTexture_Sigma_IN_SHADOWDATA;
	NrdIntegrationTexture integrationTexture_Sigma_IN_SHADOW_TRANSLUCENCY;
	NrdIntegrationTexture integrationTexture_Sigma_OUT_SHADOW_TRANSLUCENCY;

	void* _OUT_DIFF_RADIANCE_HITDIST;
	void* _OUT_SHADOW_TRANSLUCENCY;


	bool nrdInitalized = false;
	bool nrdInitalizedSigma = false;

	bool nrdCmdListPopulated = false;
	bool nrdCmdListPopulatedSigma = false;

	float m_viewToClipMatrixPrev[16];
	float m_worldToViewMatrixPrev[16];

	int _renderWidth;
	int _renderHeight;
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
	, s_D3D12CmdAllocSigma(NULL)
	, s_D3D12CmdList(NULL)
	, s_D3D12CmdListSigma(NULL)
	, s_IDXGIAdapter(NULL)
	, s_D3D12FenceValue(0)
	, s_D3D12Event(NULL)
	, s_nriDevice(NULL)
	, s_nriCommandBuffer(NULL)
	, s_nriCommandBufferSigma(NULL)
	, textureDescs()
	, textureDescsSigma()
	, userPool()
	, userPoolSigma()
	, commonSettings()
	, relaxSettings()
	, sigmaSettings()
	, resourceState()
	, resourceStateSigma()
	, textureDesc_IN_MV()
	, textureDesc_IN_NORMAL_ROUGHNESS()
	, textureDesc_IN_VIEWZ()
	, textureDesc_IN_DIFF_RADIANCE_HITDIST()
	, textureDesc_OUT_DIFF_RADIANCE_HITDIST()
	, textureDesc_Sigma_IN_MV()
	, textureDesc_Sigma_IN_NORMAL_ROUGHNESS()
	, textureDesc_Sigma_IN_SHADOWDATA()
	, textureDesc_Sigma_IN_SHADOW_TRANSLUCENCY()
	, textureDesc_Sigma_OUT_SHADOW_TRANSLUCENCY()
	, integrationTexture_IN_MV()
	, integrationTexture_IN_NORMAL_ROUGHNESS()
	, integrationTexture_IN_VIEWZ()
	, integrationTexture_IN_DIFF_RADIANCE_HITDIST()
	, integrationTexture_OUT_DIFF_RADIANCE_HITDIST()
	, integrationTexture_Sigma_IN_MV()
	, integrationTexture_Sigma_IN_NORMAL_ROUGHNESS()
	, integrationTexture_Sigma_IN_SHADOWDATA()
	, integrationTexture_Sigma_IN_SHADOW_TRANSLUCENCY()
	, integrationTexture_Sigma_OUT_SHADOW_TRANSLUCENCY()
	, _OUT_DIFF_RADIANCE_HITDIST(NULL)
	, _OUT_SHADOW_TRANSLUCENCY(NULL)
	, m_viewToClipMatrixPrev()
	, m_worldToViewMatrixPrev()
	, _renderWidth(0)
	, _renderHeight(0)
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

	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&s_D3D12CmdAllocSigma));
	if (FAILED(hr)) OutputDebugStringA("Failed to CreateCommandAllocator.\n");
	hr = device->CreateCommandList(kNodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT, s_D3D12CmdAllocSigma, nullptr, IID_PPV_ARGS(&s_D3D12CmdListSigma));
	if (FAILED(hr)) OutputDebugStringA("Failed to CreateCommandList.\n");
	s_D3D12CmdListSigma->Close();

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
}


// INPUTS - IN_MV, IN_NORMAL_ROUGHNESS, IN_VIEWZ, IN_DIFF_RADIANCE_HITDIST,
void RenderAPI_D3D12::Initialize(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	// Release resources before recreating
	if (nrdInitalized)
	{
		ReleaseNRD();
	}

	// Initialize NRD
	const nrd::MethodDesc methodDescs[] =
	{
		// Put neeeded methods here, like:
		{ nrd::Method::RELAX_DIFFUSE, renderWidth, renderHeight }

	};

	nrd::DenoiserCreationDesc denoiserCreationDesc = {};
	denoiserCreationDesc.requestedMethods = methodDescs;
	denoiserCreationDesc.requestedMethodsNum = 1;

	bool result = NRD.Initialize(denoiserCreationDesc, *s_nriDevice, NRI, NRI);


	///

	_renderWidth = renderWidth;
	_renderHeight = renderHeight;


	// Wrap pointers

	// Wrap the command buffer
	nri::CommandBufferD3D12Desc commandBufferDesc = {};
	commandBufferDesc.d3d12CommandList = (ID3D12GraphicsCommandList*)s_D3D12CmdList;

	// Not needed for NRD integration layer, but needed for NRI validation layer
	commandBufferDesc.d3d12CommandAllocator = (ID3D12CommandAllocator*)s_D3D12CmdAlloc;

	//nri::CommandBuffer* nriCommandBuffer = nullptr;
	NRI.CreateCommandBufferD3D12(*s_nriDevice, commandBufferDesc, s_nriCommandBuffer);

	// Wrap required textures (better do it only once on initialization)
	textureDesc_IN_MV = {};
	textureDesc_IN_MV.d3d12Resource = (ID3D12Resource*)IN_MV;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_IN_MV, (nri::Texture*&)textureDescs[0].texture);
	textureDescs[0].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescs[0].nextLayout = nri::TextureLayout::GENERAL;
	
	textureDesc_IN_NORMAL_ROUGHNESS = {};
	textureDesc_IN_NORMAL_ROUGHNESS.d3d12Resource = (ID3D12Resource*)IN_NORMAL_ROUGHNESS;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_IN_NORMAL_ROUGHNESS, (nri::Texture*&)textureDescs[1].texture);
	textureDescs[1].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescs[1].nextLayout = nri::TextureLayout::GENERAL;

	textureDesc_IN_VIEWZ = {};
	textureDesc_IN_VIEWZ.d3d12Resource = (ID3D12Resource*)IN_VIEWZ;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_IN_VIEWZ, (nri::Texture*&)textureDescs[2].texture);
	textureDescs[2].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescs[2].nextLayout = nri::TextureLayout::GENERAL;

	textureDesc_IN_DIFF_RADIANCE_HITDIST = {};
	textureDesc_IN_DIFF_RADIANCE_HITDIST.d3d12Resource = (ID3D12Resource*)IN_DIFF_RADIANCE_HITDIST;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_IN_DIFF_RADIANCE_HITDIST, (nri::Texture*&)textureDescs[3].texture);
	textureDescs[3].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescs[3].nextLayout = nri::TextureLayout::GENERAL;

	textureDesc_OUT_DIFF_RADIANCE_HITDIST = {};
	textureDesc_OUT_DIFF_RADIANCE_HITDIST.d3d12Resource = (ID3D12Resource*)OUT_DIFF_RADIANCE_HITDIST;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_OUT_DIFF_RADIANCE_HITDIST, (nri::Texture*&)textureDescs[4].texture);
	textureDescs[4].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescs[4].nextLayout = nri::TextureLayout::GENERAL;


	integrationTexture_IN_MV.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_IN_MV.subresourceStates = &textureDescs[0];
	
	integrationTexture_IN_NORMAL_ROUGHNESS.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_IN_NORMAL_ROUGHNESS.subresourceStates = &textureDescs[1];
	
	integrationTexture_IN_VIEWZ.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_IN_VIEWZ.subresourceStates = &textureDescs[2];
	
	integrationTexture_IN_DIFF_RADIANCE_HITDIST.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_IN_DIFF_RADIANCE_HITDIST.subresourceStates = &textureDescs[3];
	
	integrationTexture_OUT_DIFF_RADIANCE_HITDIST.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_OUT_DIFF_RADIANCE_HITDIST.subresourceStates = &textureDescs[4];

	_OUT_DIFF_RADIANCE_HITDIST = OUT_DIFF_RADIANCE_HITDIST;



	// Fill up the user pool
	userPool = {};
	{
		// Fill only required "in-use" inputs and outputs in appropriate slots using entryDescs & entryFormat,
		// applying remapping if necessary. Unused slots will be {nullptr, nri::Format::UNKNOWN}
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_MV, integrationTexture_IN_MV);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_NORMAL_ROUGHNESS, integrationTexture_IN_NORMAL_ROUGHNESS);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_VIEWZ, integrationTexture_IN_VIEWZ);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, integrationTexture_IN_DIFF_RADIANCE_HITDIST);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, integrationTexture_OUT_DIFF_RADIANCE_HITDIST);
	};


	// Populate common settings
	//  - for the first time use defaults
	//  - currently NRD supports only the following view space: X - right, Y - top, Z - forward or backward
	commonSettings = {};
	commonSettings.isBaseColorMetalnessAvailable = false;
	commonSettings.isMotionVectorInWorldSpace = true;

	// Set settings for each method in the NRD instance
	relaxSettings = {};
	relaxSettings.enableAntiFirefly = true;
	relaxSettings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::AREA_3X3;


	NRD.SetMethodSettings(nrd::Method::RELAX_DIFFUSE, &relaxSettings);



	// We inform Unity that we expect this resource to be in D3D12_RESOURCE_STATE_COPY_DEST state,
	// and because we do not barrier it ourselves, we tell Unity that no changes are done on our command list.
	resourceState = {};
	resourceState.resource = (ID3D12Resource*)_OUT_DIFF_RADIANCE_HITDIST;
	resourceState.expected = D3D12_RESOURCE_STATE_COPY_DEST;
	resourceState.current = D3D12_RESOURCE_STATE_COPY_DEST;

	
	nrdInitalized = true;
}


// INPUTS - IN_NORMAL_ROUGHNESS, IN_SHADOWDATA, IN_SHADOW_TRANSLUCENCY, OUT_SHADOW_TRANSLUCENCY,
void RenderAPI_D3D12::InitializeSigma(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_SHADOWDATA, void* IN_SHADOW_TRANSLUCENCY, void* OUT_SHADOW_TRANSLUCENCY)
{
	// Release resources before recreating
	if (nrdInitalizedSigma)
	{
		ReleaseNRDSigma();
	}

	// Initialize NRD
	const nrd::MethodDesc methodDescs[] =
	{
		// Put neeeded methods here, like:
		{ nrd::Method::SIGMA_SHADOW_TRANSLUCENCY, renderWidth, renderHeight }
	};

	nrd::DenoiserCreationDesc denoiserCreationDesc = {};
	denoiserCreationDesc.requestedMethods = methodDescs;
	denoiserCreationDesc.requestedMethodsNum = 1;

	bool result = NRDSigma.Initialize(denoiserCreationDesc, *s_nriDevice, NRI, NRI);


	///

	_renderWidth = renderWidth;
	_renderHeight = renderHeight;


	// Wrap pointers

	// Wrap the command buffer
	nri::CommandBufferD3D12Desc commandBufferDesc = {};
	commandBufferDesc.d3d12CommandList = (ID3D12GraphicsCommandList*)s_D3D12CmdListSigma;

	// Not needed for NRD integration layer, but needed for NRI validation layer
	commandBufferDesc.d3d12CommandAllocator = (ID3D12CommandAllocator*)s_D3D12CmdAllocSigma;

	//nri::CommandBuffer* nriCommandBuffer = nullptr;
	NRI.CreateCommandBufferD3D12(*s_nriDevice, commandBufferDesc, s_nriCommandBufferSigma);

	// Wrap required textures (better do it only once on initialization)
	textureDesc_Sigma_IN_MV = {};
	textureDesc_Sigma_IN_MV.d3d12Resource = (ID3D12Resource*)IN_MV;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_Sigma_IN_MV, (nri::Texture*&)textureDescsSigma[0].texture);
	textureDescsSigma[0].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescsSigma[0].nextLayout = nri::TextureLayout::GENERAL;

	textureDesc_Sigma_IN_NORMAL_ROUGHNESS = {};
	textureDesc_Sigma_IN_NORMAL_ROUGHNESS.d3d12Resource = (ID3D12Resource*)IN_NORMAL_ROUGHNESS;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_Sigma_IN_NORMAL_ROUGHNESS, (nri::Texture*&)textureDescsSigma[1].texture);
	textureDescsSigma[1].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescsSigma[1].nextLayout = nri::TextureLayout::GENERAL;

	textureDesc_Sigma_IN_SHADOWDATA = {};
	textureDesc_Sigma_IN_SHADOWDATA.d3d12Resource = (ID3D12Resource*)IN_SHADOWDATA;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_Sigma_IN_SHADOWDATA, (nri::Texture*&)textureDescsSigma[2].texture);
	textureDescsSigma[2].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescsSigma[2].nextLayout = nri::TextureLayout::GENERAL;

	textureDesc_Sigma_IN_SHADOW_TRANSLUCENCY = {};
	textureDesc_Sigma_IN_SHADOW_TRANSLUCENCY.d3d12Resource = (ID3D12Resource*)IN_SHADOW_TRANSLUCENCY;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_Sigma_IN_SHADOW_TRANSLUCENCY, (nri::Texture*&)textureDescsSigma[3].texture);
	textureDescsSigma[3].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescsSigma[3].nextLayout = nri::TextureLayout::GENERAL;

	textureDesc_Sigma_OUT_SHADOW_TRANSLUCENCY = {};
	textureDesc_Sigma_OUT_SHADOW_TRANSLUCENCY.d3d12Resource = (ID3D12Resource*)OUT_SHADOW_TRANSLUCENCY;
	NRI.CreateTextureD3D12(*s_nriDevice, textureDesc_Sigma_OUT_SHADOW_TRANSLUCENCY, (nri::Texture*&)textureDescsSigma[4].texture);
	textureDescsSigma[4].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescsSigma[4].nextLayout = nri::TextureLayout::GENERAL;


	integrationTexture_Sigma_IN_MV.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_Sigma_IN_MV.subresourceStates = &textureDescsSigma[0];

	integrationTexture_Sigma_IN_NORMAL_ROUGHNESS.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_Sigma_IN_NORMAL_ROUGHNESS.subresourceStates = &textureDescsSigma[1];

	integrationTexture_Sigma_IN_SHADOWDATA.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_Sigma_IN_SHADOWDATA.subresourceStates = &textureDescsSigma[2];

	integrationTexture_Sigma_IN_SHADOW_TRANSLUCENCY.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_Sigma_IN_SHADOW_TRANSLUCENCY.subresourceStates = &textureDescsSigma[3];

	integrationTexture_Sigma_OUT_SHADOW_TRANSLUCENCY.format = nri::Format::RGBA16_SFLOAT;
	integrationTexture_Sigma_OUT_SHADOW_TRANSLUCENCY.subresourceStates = &textureDescsSigma[4];

	_OUT_SHADOW_TRANSLUCENCY = OUT_SHADOW_TRANSLUCENCY;



	// Fill up the user pool
	userPoolSigma = {};
	{
		// Fill only required "in-use" inputs and outputs in appropriate slots using entryDescs & entryFormat,
		// applying remapping if necessary. Unused slots will be {nullptr, nri::Format::UNKNOWN}
		NrdIntegration_SetResource(userPoolSigma, nrd::ResourceType::IN_MV, integrationTexture_Sigma_IN_MV);
		NrdIntegration_SetResource(userPoolSigma, nrd::ResourceType::IN_NORMAL_ROUGHNESS, integrationTexture_Sigma_IN_NORMAL_ROUGHNESS);
		NrdIntegration_SetResource(userPoolSigma, nrd::ResourceType::IN_SHADOWDATA, integrationTexture_Sigma_IN_SHADOWDATA);
		NrdIntegration_SetResource(userPoolSigma, nrd::ResourceType::IN_SHADOW_TRANSLUCENCY, integrationTexture_Sigma_IN_SHADOW_TRANSLUCENCY);
		NrdIntegration_SetResource(userPoolSigma, nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY, integrationTexture_Sigma_OUT_SHADOW_TRANSLUCENCY);
	};


	// Populate common settings
	//  - for the first time use defaults
	//  - currently NRD supports only the following view space: X - right, Y - top, Z - forward or backward
	commonSettings = {};
	commonSettings.isBaseColorMetalnessAvailable = false;
	commonSettings.isMotionVectorInWorldSpace = true;

	// Set settings for each method in the NRD instance
	sigmaSettings = {};


	NRDSigma.SetMethodSettings(nrd::Method::SIGMA_SHADOW_TRANSLUCENCY, &sigmaSettings);



	// We inform Unity that we expect this resource to be in D3D12_RESOURCE_STATE_COPY_DEST state,
	// and because we do not barrier it ourselves, we tell Unity that no changes are done on our command list.
	resourceStateSigma = {};
	resourceStateSigma.resource = (ID3D12Resource*)_OUT_SHADOW_TRANSLUCENCY;
	resourceStateSigma.expected = D3D12_RESOURCE_STATE_COPY_DEST;
	resourceStateSigma.current = D3D12_RESOURCE_STATE_COPY_DEST;


	nrdInitalizedSigma = true;
}



void RenderAPI_D3D12::Denoise()
{
	if (nrdInitalized && !nrdCmdListPopulated)
	{
		s_D3D12CmdList->Reset(s_D3D12CmdAlloc, NULL);

		// Better use "true" if resources are not changing between frames (i.e. are not suballocated from a heap)
		NRD.Denoise(commonSettings.frameIndex, *s_nriCommandBuffer, commonSettings, userPool, true);


		s_D3D12CmdList->Close();
		nrdCmdListPopulated = true;
	}

	s_D3D12->ExecuteCommandList(s_D3D12CmdList, 1, &resourceState);
}


void RenderAPI_D3D12::DenoiseSigma()
{
	if (nrdInitalizedSigma && !nrdCmdListPopulatedSigma)
	{
		s_D3D12CmdListSigma->Reset(s_D3D12CmdAllocSigma, NULL);

		// Better use "true" if resources are not changing between frames (i.e. are not suballocated from a heap)
		NRDSigma.Denoise(commonSettings.frameIndex, *s_nriCommandBufferSigma, commonSettings, userPoolSigma, true);

		s_D3D12CmdListSigma->Close();
		nrdCmdListPopulatedSigma = true;
	}

	s_D3D12->ExecuteCommandList(s_D3D12CmdListSigma, 1, &resourceStateSigma);
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


void RenderAPI_D3D12::ReleaseTextures()
{
	// Better do it only once on shutdown
	for (uint32_t i = 0; i < 5; i++)
	{
		if (textureDescs[i].texture != nullptr) NRI.DestroyTexture(*(nri::Texture*&)textureDescs[i].texture);
	}

	_OUT_DIFF_RADIANCE_HITDIST = nullptr;
}


void RenderAPI_D3D12::ReleaseTexturesSigma()
{
	// Better do it only once on shutdown
	for (uint32_t i = 0; i < 5; i++)
	{
		if (textureDescsSigma[i].texture != nullptr) NRI.DestroyTexture(*(nri::Texture*&)textureDescsSigma[i].texture);
	}

	_OUT_SHADOW_TRANSLUCENCY = nullptr;
}


void RenderAPI_D3D12::ReleaseResources()
{
	SAFE_RELEASE(s_D3D12Upload);
	if (s_D3D12Event)
		CloseHandle(s_D3D12Event);
	SAFE_RELEASE(s_D3D12CmdList);
	SAFE_RELEASE(s_D3D12CmdAlloc);
	SAFE_RELEASE(s_IDXGIAdapter);

	//ReleaseTextures();
	ReleaseNRD();

	//NRI.DestroyDevice(*s_nriDevice);
	s_nriDevice = nullptr;

	nrdInitalized = false;
	nrdCmdListPopulated = false;
}


void RenderAPI_D3D12::ReleaseResourcesSigma()
{
	SAFE_RELEASE(s_D3D12CmdListSigma);
	SAFE_RELEASE(s_D3D12CmdAllocSigma);
	
	//ReleaseTextures();
	ReleaseNRDSigma();


	nrdInitalizedSigma = false;
	nrdCmdListPopulatedSigma = false;
}


void RenderAPI_D3D12::ReleaseNRD()
{
	// Also NRD needs to be recreated on "resize"
	if (nrdInitalized)
	{
		if (s_nriCommandBuffer != nullptr) NRI.DestroyCommandBuffer(*s_nriCommandBuffer);

		ReleaseTextures();
		NRD.Destroy();

		nrdInitalized = false;
		nrdCmdListPopulated = false;
	}
}


void RenderAPI_D3D12::ReleaseNRDSigma()
{
	// Also NRD needs to be recreated on "resize"
	if (nrdInitalizedSigma)
	{
		if (s_nriCommandBufferSigma != nullptr) NRI.DestroyCommandBuffer(*s_nriCommandBufferSigma);

		ReleaseTexturesSigma();
		NRDSigma.Destroy();

		nrdInitalizedSigma = false;
		nrdCmdListPopulatedSigma = false;
	}
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