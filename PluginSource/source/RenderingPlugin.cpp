// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"

#include <assert.h>
#include <math.h>
#include <vector>

#include "Unity/IUnityRenderingExtensions.h"

// Gameworks
#include "../NRI/_NRI_SDK/Include/NRI.h"

bool NDRInitiazlized = false;
bool NDRResourcesSet = false;

int _frameIndex;
int _renderWidth;
int _renderHeight;
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
static void UNITY_INTERFACE_API OnInitializeEvent(int eventID)
{
	if (!NDRInitiazlized && NDRResourcesSet)
	{
		s_CurrentAPI->Initialize(_renderWidth, _renderHeight, _IN_MV, _IN_NORMAL_ROUGHNESS, _IN_VIEWZ, _IN_DIFF_RADIANCE_HITDIST, _OUT_DIFF_RADIANCE_HITDIST);

		NDRInitiazlized = true;
	}
}


static void UNITY_INTERFACE_API OnDenoiseEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL)
		return;

	if (NDRInitiazlized && NDRResourcesSet)
	{
		s_CurrentAPI->Denoise(_frameIndex, _viewToClipMatrix, _worldToViewMatrix);
	}
}


// Update functions
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateResources(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	_renderWidth = renderWidth;
	_renderHeight = renderHeight;
	_IN_MV = IN_MV;
	_IN_NORMAL_ROUGHNESS = IN_NORMAL_ROUGHNESS;
	_IN_VIEWZ = IN_VIEWZ;
	_IN_DIFF_RADIANCE_HITDIST = IN_DIFF_RADIANCE_HITDIST;
	_OUT_DIFF_RADIANCE_HITDIST = OUT_DIFF_RADIANCE_HITDIST;

	NDRResourcesSet = true;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateParams(int frameIndex, float viewToClipMatrix[16], float worldToViewMatrix[16])
{
	_frameIndex = frameIndex;

	// Set matrices
	for (int i = 0; i < 16; ++i)
	{
		_viewToClipMatrix[i] = viewToClipMatrix[i];
		_worldToViewMatrix[i] = worldToViewMatrix[i];
	}
}


// External callback functions
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Initialize()
{
	return OnInitializeEvent;
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Denoise()
{
	return OnDenoiseEvent;
}

