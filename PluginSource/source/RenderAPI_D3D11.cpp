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
	bool NRDInitialize(int denoiserType, int renderWidth, int renderHeight, void** resources, int resourceCount);
	void NRDDenoise(int denoiserType);
	void NRDRelease(int denoiserType);
	void SetMatrix(int frameIndex, float viewToClipMatrix[16], float worldToViewMatrix[16]);

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


bool RenderAPI_D3D11::NRDInitialize(int denoiserType, int renderWidth, int renderHeight, void** resources, int resourceCount)
{
	return false;
}


void RenderAPI_D3D11::NRDDenoise(int denoiserType)
{
}


void RenderAPI_D3D11::NRDRelease(int denoiserType)
{
}


void RenderAPI_D3D11::SetMatrix(int frameIndex, float viewToClipMatrix[16], float worldToViewMatrix[16])
{
}

#endif // #if SUPPORT_D3D11
