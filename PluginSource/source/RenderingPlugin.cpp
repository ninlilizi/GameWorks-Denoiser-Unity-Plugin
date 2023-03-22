// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"

#include <assert.h>
#include <math.h>
#include <vector>

#include "Unity/IUnityRenderingExtensions.h"

// Gameworks
#include "../NRI/_NRI_SDK/Include/NRI.h"

bool _NRDInitiazlized = false;
bool _NRDParamsSet = false;

int _renderWidth;
int _renderHeight;
int _prevRenderWidth;
int _prevRenderHeight;
int _frameIndex;

void* _IN_MV;
void* _IN_NORMAL_ROUGHNESS;
void* _IN_VIEWZ;
void* _IN_DIFF_RADIANCE_HITDIST;
void* _OUT_DIFF_RADIANCE_HITDIST;

float _viewToClipMatrix[16];
float _worldToViewMatrix[16];


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
	}
}


// Events
static void UNITY_INTERFACE_API OnExecuteEvent(int eventID)
{
	// Unknown / unsupported graphics device type, or we've not set the params? Do nothing
	if (s_CurrentAPI == NULL  || !_NRDParamsSet)
		return;


	if ((_renderWidth != _prevRenderWidth || _renderHeight != _prevRenderHeight) && _NRDInitiazlized)
	{
		s_CurrentAPI->ReleaseNRD();

		_NRDInitiazlized = false;
	}

	if (!_NRDInitiazlized &&
		_renderWidth > 0 &&
		_renderHeight > 0 &&
		_IN_MV != nullptr &&
		_IN_NORMAL_ROUGHNESS != nullptr &&
		_IN_VIEWZ != nullptr &&
		_IN_DIFF_RADIANCE_HITDIST != nullptr &&
		_OUT_DIFF_RADIANCE_HITDIST != nullptr)
	{
		s_CurrentAPI->Initialize(_renderWidth, _renderHeight, _IN_MV, _IN_NORMAL_ROUGHNESS, _IN_VIEWZ, _IN_DIFF_RADIANCE_HITDIST, _OUT_DIFF_RADIANCE_HITDIST);

		_prevRenderWidth = _renderWidth;
		_prevRenderHeight = _renderHeight;

		_NRDInitiazlized = true;
	}

	if (_NRDInitiazlized)
	{
		s_CurrentAPI->Denoise(_frameIndex, _viewToClipMatrix, _worldToViewMatrix);
		s_CurrentAPI->Execute();
	}
}


// External functions
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseResources()
{
	if (_NRDInitiazlized && s_CurrentAPI != nullptr)
	{
		s_CurrentAPI->ReleaseResources();

		_renderWidth = 0;
		_renderHeight = 0;
		_NRDInitiazlized = false;

		_NRDParamsSet = false;
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDSetParams(int frameIndex, int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST, float viewToClipMatrix[16], float worldToViewMatrix[16])
{
	_IN_MV = IN_MV;
	_IN_NORMAL_ROUGHNESS = IN_NORMAL_ROUGHNESS;
	_IN_VIEWZ = IN_VIEWZ;
	_IN_DIFF_RADIANCE_HITDIST = IN_DIFF_RADIANCE_HITDIST;
	_OUT_DIFF_RADIANCE_HITDIST = OUT_DIFF_RADIANCE_HITDIST;

	_renderWidth = renderWidth;
	_renderHeight = renderHeight;

	_frameIndex = frameIndex;

	for (int i = 0; i < 16; ++i)
	{
		_viewToClipMatrix[i] = viewToClipMatrix[i];
		_worldToViewMatrix[i] = worldToViewMatrix[i];
	}

	_NRDParamsSet = true;
}


// External callback functions
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDExecute()
{
	return OnExecuteEvent;
}

