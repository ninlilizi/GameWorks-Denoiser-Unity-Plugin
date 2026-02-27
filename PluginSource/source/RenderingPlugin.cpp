// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"

#include <assert.h>
#include <math.h>
#include <vector>

#include "Unity/IUnityRenderingExtensions.h"

// Gameworks
#include "../NRI/Include/NRI.h"

bool _NRDInitializedRelax;
bool _NRDInitializedSigma;
bool _NRDInitializedReblur;

int _renderWidthRelax;
int _renderWidthSigma;
int _renderWidthReblur;
int _renderHeightRelax;
int _renderHeightSigma;
int _renderHeightReblur;
int _prevRenderWidthRelax;
int _prevRenderWidthSigma;
int _prevRenderWidthReblur;
int _prevRenderHeightRelax;
int _prevRenderHeightSigma;
int _prevRenderHeightReblur;

void* _IN_MV;
void* _IN_NORMAL_ROUGHNESS;
void* _IN_BASECOLOR_METALNESS;
void* _IN_VIEWZ;
void* _IN_DIFF_RADIANCE_HITDIST;
void* _OUT_DIFF_RADIANCE_HITDIST;

void* _Sigma_IN_MV;
void* _Sigma_IN_NORMAL_ROUGHNESS;
void* _Sigma_IN_SHADOWDATA;
void* _Sigma_IN_SHADOW_TRANSLUCENCY;
void* _Sigma_OUT_SHADOW_TRANSLUCENCY;

void* _Reblur_IN_MV;
void* _Reblur_IN_NORMAL_ROUGHNESS;
void* _Reblur_IN_BASECOLOR_METALNESS;
void* _Reblur_IN_VIEWZ;
void* _Reblur_IN_DIFF_RADIANCE_HITDIST;
void* _Reblur_OUT_DIFF_RADIANCE_HITDIST;


// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	
#if SUPPORT_VULKAN
	if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
	{
		extern void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces*);
		RenderAPI_Vulkan_OnPluginLoad(unityInterfaces);
	}
#endif // SUPPORT_VULKAN

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

#if UNITY_WEBGL
typedef void	(UNITY_INTERFACE_API * PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void	(UNITY_INTERFACE_API * PluginUnloadFunc)();

extern "C" void	UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
	UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif


void ClearLocalVarsRelax()
{
	_renderWidthRelax = 0;
	_renderHeightRelax = 0;
	_IN_MV = nullptr;
	_IN_NORMAL_ROUGHNESS = nullptr;
	_IN_VIEWZ = nullptr;
	_IN_DIFF_RADIANCE_HITDIST = nullptr;
	_OUT_DIFF_RADIANCE_HITDIST = nullptr;
	_NRDInitializedRelax = false;
}


void ClearLocalVarsSigma()
{
	_renderWidthSigma = 0;
	_renderHeightSigma = 0;
	_Sigma_IN_NORMAL_ROUGHNESS = nullptr;
	_Sigma_IN_SHADOWDATA = nullptr;
	_Sigma_IN_SHADOW_TRANSLUCENCY = nullptr;
	_Sigma_OUT_SHADOW_TRANSLUCENCY = nullptr;
	_NRDInitializedSigma = false;
}


void ClearLocalVarsReblur()
{
	_renderWidthReblur = 0;
	_renderHeightReblur = 0;
	_Reblur_IN_MV = nullptr;
	_Reblur_IN_NORMAL_ROUGHNESS = nullptr;
	_Reblur_IN_VIEWZ = nullptr;
	_Reblur_IN_DIFF_RADIANCE_HITDIST = nullptr;
	_Reblur_OUT_DIFF_RADIANCE_HITDIST = nullptr;
	_NRDInitializedReblur = false;
}

// --------------------------------------------------------------------------
// GraphicsDeviceEvent


static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete s_CurrentAPI;
		s_CurrentAPI = NULL;
		s_DeviceType = kUnityGfxRendererNull;

		ClearLocalVarsRelax();
		ClearLocalVarsSigma();
		ClearLocalVarsReblur();
	}
}


// Events
static void UNITY_INTERFACE_API OnExecuteEventRelax(int eventID)
{
	// Unknown / unsupported graphics device type, or we've not set the params? Do nothing
	if (s_CurrentAPI == NULL)
		return;

	if (_NRDInitializedRelax)
	{
		s_CurrentAPI->Denoise();
	}
}


static void UNITY_INTERFACE_API OnExecuteEventSigma(int eventID)
{
	// Unknown / unsupported graphics device type, or we've not set the params? Do nothing
	if (s_CurrentAPI == NULL)
		return;

	if (_NRDInitializedSigma)
	{
		s_CurrentAPI->DenoiseSigma();
	}
}


static void UNITY_INTERFACE_API OnExecuteEventReblur(int eventID)
{
	// Unknown / unsupported graphics device type, or we've not set the params? Do nothing
	if (s_CurrentAPI == NULL)
		return;

	if (_NRDInitializedReblur)
	{
		s_CurrentAPI->DenoiseReblur();
	}
}


// External functions
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseRelax()
{
	if (_NRDInitializedRelax && s_CurrentAPI != nullptr)
	{
		s_CurrentAPI->ReleaseNRDRelax();

		ClearLocalVarsRelax();
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseSigma()
{
	if (_NRDInitializedSigma && s_CurrentAPI != nullptr)
	{
		s_CurrentAPI->ReleaseNRDSigma();

		ClearLocalVarsSigma();
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseReblur()
{
	if (_NRDInitializedReblur && s_CurrentAPI != nullptr)
	{
		s_CurrentAPI->ReleaseNRDReblur();

		ClearLocalVarsReblur();
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseAll()
{
	NRDReleaseRelax();
	NRDReleaseSigma();
	NRDReleaseReblur();
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDInitializeRelax(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_BASECOLOR_METALNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	if ((renderWidth != _prevRenderWidthRelax || renderHeight != _prevRenderHeightRelax) && _NRDInitializedRelax)
	{
		NRDReleaseRelax();

		_prevRenderWidthRelax = renderWidth;
		_prevRenderHeightRelax = renderHeight;
	}


	/*if (!_NRDInitiazlized &&
		_renderWidth > 0 &&
		_renderHeight > 0 &&
		IN_MV != nullptr &&
		IN_NORMAL_ROUGHNESS != nullptr &&
		IN_VIEWZ != nullptr &&
		IN_DIFF_RADIANCE_HITDIST != nullptr &&
		OUT_DIFF_RADIANCE_HITDIST != nullptr)*/
	//if (!_NRDInitialized)
	{
		_IN_MV = IN_MV;
		_IN_NORMAL_ROUGHNESS = IN_NORMAL_ROUGHNESS;
		_IN_BASECOLOR_METALNESS = IN_BASECOLOR_METALNESS;
		_IN_VIEWZ = IN_VIEWZ;
		_IN_DIFF_RADIANCE_HITDIST = IN_DIFF_RADIANCE_HITDIST;
		_OUT_DIFF_RADIANCE_HITDIST = OUT_DIFF_RADIANCE_HITDIST;

		_renderWidthRelax = renderWidth;
		_renderHeightRelax = renderHeight;

		s_CurrentAPI->Initialize(_renderWidthRelax, _renderHeightRelax, _IN_MV, _IN_NORMAL_ROUGHNESS, _IN_BASECOLOR_METALNESS, _IN_VIEWZ, _IN_DIFF_RADIANCE_HITDIST, _OUT_DIFF_RADIANCE_HITDIST);
		_NRDInitializedRelax = true;
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDInitializeSigma(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_SHADOWDATA, void* IN_SHADOW_TRANSLUCENCY, void* OUT_SHADOW_TRANSLUCENCY)
{
	if ((renderWidth != _prevRenderWidthSigma || renderHeight != _prevRenderHeightSigma) && _NRDInitializedSigma)
	{
		NRDReleaseSigma();

		_prevRenderWidthSigma = renderWidth;
		_prevRenderHeightSigma = renderHeight;
	}


	/*if (!_NRDInitiazlizedSigma &&
		_renderWidthSigma > 0 &&
		_renderHeightSigma > 0 &&
		IN_NORMAL_ROUGHNESS != nullptr &&
		IN_SHADOWDATA != nullptr &&
		IN_SHADOW_TRANSLUCENCY != nullptr &&
		OUT_SHADOW_TRANSLUCENCY != nullptr)*/
	//if (!_NRDInitializedSigma)
	{
		_Sigma_IN_MV = IN_MV;
		_Sigma_IN_NORMAL_ROUGHNESS = IN_NORMAL_ROUGHNESS;
		_Sigma_IN_SHADOWDATA = IN_SHADOWDATA;
		_Sigma_IN_SHADOW_TRANSLUCENCY = IN_SHADOW_TRANSLUCENCY;
		_Sigma_OUT_SHADOW_TRANSLUCENCY = OUT_SHADOW_TRANSLUCENCY;

		_renderWidthSigma = renderWidth;
		_renderHeightSigma = renderHeight;

		s_CurrentAPI->InitializeSigma(_renderWidthSigma, _renderHeightSigma, _Sigma_IN_MV, _Sigma_IN_NORMAL_ROUGHNESS, _Sigma_IN_SHADOWDATA, _Sigma_IN_SHADOW_TRANSLUCENCY, _Sigma_OUT_SHADOW_TRANSLUCENCY);
		_NRDInitializedSigma = true;
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDInitializeReblur(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_BASECOLOR_METALNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	if ((renderWidth != _prevRenderWidthReblur || renderHeight != _prevRenderHeightReblur) && _NRDInitializedReblur)
	{
		NRDReleaseReblur();

		_prevRenderWidthReblur = renderWidth;
		_prevRenderHeightReblur = renderHeight;
	}


	/*if (!_NRDInitiazlized &&
		_renderWidth > 0 &&
		_renderHeight > 0 &&
		IN_MV != nullptr &&
		IN_NORMAL_ROUGHNESS != nullptr &&
		IN_VIEWZ != nullptr &&
		IN_DIFF_RADIANCE_HITDIST != nullptr &&
		OUT_DIFF_RADIANCE_HITDIST != nullptr)*/
	//if (!_NRDInitializedReblur)
	{
		_Reblur_IN_MV = IN_MV;
		_Reblur_IN_NORMAL_ROUGHNESS = IN_NORMAL_ROUGHNESS;
		_Reblur_IN_BASECOLOR_METALNESS = IN_BASECOLOR_METALNESS;
		_Reblur_IN_VIEWZ = IN_VIEWZ;
		_Reblur_IN_DIFF_RADIANCE_HITDIST = IN_DIFF_RADIANCE_HITDIST;
		_Reblur_OUT_DIFF_RADIANCE_HITDIST = OUT_DIFF_RADIANCE_HITDIST;

		_renderWidthReblur = renderWidth;
		_renderHeightReblur = renderHeight;

		s_CurrentAPI->InitializeReblur(_renderWidthReblur, _renderHeightReblur, _Reblur_IN_MV, _Reblur_IN_NORMAL_ROUGHNESS, _Reblur_IN_BASECOLOR_METALNESS, _Reblur_IN_VIEWZ, _Reblur_IN_DIFF_RADIANCE_HITDIST, _Reblur_OUT_DIFF_RADIANCE_HITDIST);
		_NRDInitializedReblur = true;
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDSetMatrix(int frameIndex, float viewToClipMatrix[16], float worldToViewMatrix[16])
{
	s_CurrentAPI->SetMatrix(frameIndex, viewToClipMatrix, worldToViewMatrix);
}


// External callback functions
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDExecuteRelax()
{
	return OnExecuteEventRelax;
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDExecuteSigma()
{
	return OnExecuteEventSigma;
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDExecuteReblur()
{
	return OnExecuteEventReblur;
}