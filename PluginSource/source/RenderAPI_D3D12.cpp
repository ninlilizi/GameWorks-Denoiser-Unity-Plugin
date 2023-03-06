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

	virtual void DrawSimpleTriangles(const float worldMatrix[16], int triangleCount, const void* verticesFloat3Byte4);

	virtual void* BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch);
	virtual void EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr);

	virtual void* BeginModifyVertexBuffer(void* bufferHandle, size_t* outBufferSize);
	virtual void EndModifyVertexBuffer(void* bufferHandle);

private:
	UINT64 AlignPow2(UINT64 value);
	UINT64 GetAlignedSize(int width, int height, int pixelSize, int rowPitch);
	ID3D12Resource* GetUploadResource(UINT64 size);
	void CreateResources();
	void ReleaseResources();
	void Initialize(int renderWidth, int renderHeight, void* diffusePtr, void* specularHandle);
	void InitializeNRD(int renderWidth, int renderHeight);
	void Denoise(int frameIndex);

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
	nri::TextureTransitionBarrierDesc textureDescs[2];
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

UINT64 RenderAPI_D3D12::AlignPow2(UINT64 value)
{
	UINT64 aligned = pow(2, (int)log2(value));
	return aligned >= value ? aligned : aligned * 2;
}

UINT64 RenderAPI_D3D12::GetAlignedSize( int width, int height, int pixelSize, int rowPitch)
{
	UINT64 size = width * height * pixelSize;

	size = AlignPow2(size);

	if (size < D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
	{
		return D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;
	}
	else if (width * pixelSize < rowPitch)
	{
		return rowPitch * height;
	}
	else
	{
		return size;
	}
}

ID3D12Resource* RenderAPI_D3D12::GetUploadResource(UINT64 size)
{
	if (s_D3D12Upload)
	{
		D3D12_RESOURCE_DESC desc = s_D3D12Upload->GetDesc();
		if (desc.Width == size)
			return s_D3D12Upload;
		else
			s_D3D12Upload->Release();
	}

	// Texture upload buffer
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = kNodeMask;
	heapProps.VisibleNodeMask = kNodeMask;

	D3D12_RESOURCE_DESC heapDesc = {};
	heapDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	heapDesc.Alignment = 0;
	heapDesc.Width = size;
	heapDesc.Height = 1;
	heapDesc.DepthOrArraySize = 1;
	heapDesc.MipLevels = 1;
	heapDesc.Format = DXGI_FORMAT_UNKNOWN;
	heapDesc.SampleDesc.Count = 1;
	heapDesc.SampleDesc.Quality = 0;
	heapDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	heapDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Device* device = s_D3D12->GetDevice();
	HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&heapDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&s_D3D12Upload));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to CreateCommittedResource.\n");
	}
	
	return s_D3D12Upload;
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
		{ nrd::Method::REBLUR_DIFFUSE, 256, 256 },
	};

	nrd::DenoiserCreationDesc denoiserCreationDesc = {};
	denoiserCreationDesc.requestedMethods = methodDescs;
	denoiserCreationDesc.requestedMethodsNum = 1;

	bool result = NRD.Initialize(denoiserCreationDesc, *s_nriDevice, NRI, NRI);
}


void RenderAPI_D3D12::Initialize(int renderWidth, int renderHeight, void* diffuseHandle, void* specularHandle)
{
	// Initialize NRD
	NRD.Destroy();
	InitializeNRD(renderWidth, renderHeight);


	// Wrap pointers

	// Wrap the command buffer
	nri::CommandBufferD3D12Desc commandBufferDesc = {};
	commandBufferDesc.d3d12CommandList = (ID3D12GraphicsCommandList*)s_D3D12CmdList;

	// Not needed for NRD integration layer, but needed for NRI validation layer
	commandBufferDesc.d3d12CommandAllocator = (ID3D12CommandAllocator*)s_D3D12CmdAlloc;

	nri::CommandBuffer* nriCommandBuffer = nullptr;
	NRI.CreateCommandBufferD3D12(*s_nriDevice, commandBufferDesc, s_nriCommandBuffer);

	// Wrap required textures (better do it only once on initialization)
	nri::TextureD3D12Desc textureDescDiffuse = {};
	NRI.CreateTextureD3D12(*s_nriDevice, textureDescDiffuse, (nri::Texture*&)diffuseHandle);
	textureDescs[0].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescs[0].nextLayout = nri::TextureLayout::GENERAL;

	nri::TextureD3D12Desc textureDescSpecular = {};
	NRI.CreateTextureD3D12(*s_nriDevice, textureDescSpecular, (nri::Texture*&)specularHandle);
	textureDescs[1].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
	textureDescs[1].nextLayout = nri::TextureLayout::GENERAL;
}


void RenderAPI_D3D12::Denoise(int frameIndex)
{
	// Populate common settings
	//  - for the first time use defaults
	//  - currently NRD supports only the following view space: X - right, Y - top, Z - forward or backward
	nrd::CommonSettings commonSettings = {};
	//PopulateCommonSettings(commonSettings);

	// Set settings for each method in the NRD instance
	nrd::ReblurSettings settings = {};
	//PopulateXxxSettings(settings);

	NRD.SetMethodSettings(nrd::Method::REBLUR_DIFFUSE, &settings);
	NRD.SetMethodSettings(nrd::Method::REBLUR_SPECULAR, &settings);

	// Fill up the user pool
	NrdUserPool userPool = {};
	{
		// Fill only required "in-use" inputs and outputs in appropriate slots using entryDescs & entryFormat,
		// applying remapping if necessary. Unused slots will be {nullptr, nri::Format::UNKNOWN}
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, *(NrdIntegrationTexture*&)textureDescs[0].texture);
		NrdIntegration_SetResource(userPool, nrd::ResourceType::IN_SPEC_HITDIST, *(NrdIntegrationTexture*&)textureDescs[1].texture);

	};

	// Better use "true" if resources are not changing between frames (i.e. are not suballocated from a heap)
	bool enableDescriptorCaching = true;

	NRD.Denoise(frameIndex, *s_nriCommandBuffer, commonSettings, userPool, enableDescriptorCaching);
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


	// Better do it only once on shutdown
	for (uint32_t i = 0; i < 1; i++)
	{
		if (textureDescs[i].texture != nullptr) NRI.DestroyTexture(*(nri::Texture*&)textureDescs[i].texture);
	}

	if (s_nriCommandBuffer != nullptr) NRI.DestroyCommandBuffer(*s_nriCommandBuffer);

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


void RenderAPI_D3D12::DrawSimpleTriangles(const float worldMatrix[16], int triangleCount, const void* verticesFloat3Byte4)
{
	//@TODO: example not implemented yet :)
}


void* RenderAPI_D3D12::BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch)
{
	ID3D12Fence* fence = s_D3D12->GetFrameFence();

	// Wait on the previous job (example only - simplifies resource management)
	if (fence->GetCompletedValue() < s_D3D12FenceValue)
	{
		fence->SetEventOnCompletion(s_D3D12FenceValue, s_D3D12Event);
		WaitForSingleObject(s_D3D12Event, INFINITE);
	}

	// Begin a command list
	s_D3D12CmdAlloc->Reset();
	s_D3D12CmdList->Reset(s_D3D12CmdAlloc, nullptr);

	// Fill data
	// Clamp to minimum rowPitch of RGBA32
	*outRowPitch = max(AlignPow2(textureWidth * 4), 256);
	const UINT64 kDataSize = GetAlignedSize(textureWidth, textureHeight, 4, *outRowPitch);
	ID3D12Resource* upload = GetUploadResource(kDataSize);
	void* mapped = NULL;
	upload->Map(0, NULL, &mapped);
	return mapped;
}


void RenderAPI_D3D12::EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr)
{
	ID3D12Device* device = s_D3D12->GetDevice();

	const UINT64 kDataSize = GetAlignedSize(textureWidth, textureHeight, 4, rowPitch);
	ID3D12Resource* upload = GetUploadResource(kDataSize);
	upload->Unmap(0, NULL);

	ID3D12Resource* resource = (ID3D12Resource*)textureHandle;
	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	assert(desc.Width == textureWidth);
	assert(desc.Height == textureHeight);

	D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
	srcLoc.pResource = upload;
	srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	device->GetCopyableFootprints(&desc, 0, 1, 0, &srcLoc.PlacedFootprint, nullptr, nullptr, nullptr);

	D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
	dstLoc.pResource = resource;
	dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstLoc.SubresourceIndex = 0;

	// We inform Unity that we expect this resource to be in D3D12_RESOURCE_STATE_COPY_DEST state,
	// and because we do not barrier it ourselves, we tell Unity that no changes are done on our command list.
	UnityGraphicsD3D12ResourceState resourceState = {};
	resourceState.resource = resource;
	resourceState.expected = D3D12_RESOURCE_STATE_COPY_DEST;
	resourceState.current = D3D12_RESOURCE_STATE_COPY_DEST;

	// Queue data upload
	s_D3D12CmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

	// Execute the command list
	s_D3D12CmdList->Close();
	s_D3D12FenceValue = s_D3D12->ExecuteCommandList(s_D3D12CmdList, 1, &resourceState);
}


void* RenderAPI_D3D12::BeginModifyVertexBuffer(void* bufferHandle, size_t* outBufferSize)
{
	//@TODO
	return NULL;
}


void RenderAPI_D3D12::EndModifyVertexBuffer(void* bufferHandle)
{
	//@TODO
}

#endif // #if SUPPORT_D3D12
