#include "RenderAPI.h"
#include "PlatformBase.h"

// Direct3D 11 implementation of RenderAPI.

#if SUPPORT_D3D11

#include <assert.h>
#include <d3d11.h>
#include "Unity/IUnityGraphicsD3D11.h"


class RenderAPI_D3D11 : public RenderAPI
{
public:
	RenderAPI_D3D11();
	virtual ~RenderAPI_D3D11() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

private:
	void CreateResources();
	void ReleaseResources();
	void Initialize(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST);
	void Denoise(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16]);

private:
	ID3D11Device* m_Device;
};


RenderAPI* CreateRenderAPI_D3D11()
{
	return new RenderAPI_D3D11();
}

RenderAPI_D3D11::RenderAPI_D3D11()
	: m_Device(NULL)

{
}


void RenderAPI_D3D11::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
		m_Device = d3d->GetDevice();
		CreateResources();
		break;
	}
	case kUnityGfxDeviceEventShutdown:
		ReleaseResources();
		break;
	}
}


void RenderAPI_D3D11::CreateResources()
{
	D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));
}


void RenderAPI_D3D11::ReleaseResources()
{
}


void RenderAPI_D3D11::Initialize(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
}


void RenderAPI_D3D11::Denoise(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16])
{
}

#endif // #if SUPPORT_D3D11
