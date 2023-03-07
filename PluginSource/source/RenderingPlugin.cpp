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


void* _DiffuseTextureOutput;

static float g_FrameNum;
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetFrameNumber(float t) { g_FrameNum = t; }


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


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitializeNDR(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	NDRInitiazlized = true;

	s_CurrentAPI->Initialize(renderWidth, renderHeight, IN_MV, IN_NORMAL_ROUGHNESS, IN_VIEWZ, IN_DIFF_RADIANCE_HITDIST, OUT_DIFF_RADIANCE_HITDIST, &_DiffuseTextureOutput);
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Denoise(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16])
{
	if (NDRInitiazlized)
	{
		s_CurrentAPI->Denoise(frameIndex, _viewToClipMatrix, _worldToViewMatrix);
	}
}


static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL)
		return;


}



void TextureDiffuseOutputCallback(int eventID, void* data)
{
	if (eventID == kUnityRenderingExtEventUpdateTextureBeginV2)
	{
		// UpdateTextureBegin: Generate and return texture image data.
		UnityRenderingExtTextureUpdateParamsV2* params = (UnityRenderingExtTextureUpdateParamsV2*)data;

		params->texData = _DiffuseTextureOutput;
	}
	else if (eventID == kUnityRenderingExtEventUpdateTextureEndV2)
	{
		// UpdateTextureEnd: Free up the temporary memory.
		UnityRenderingExtTextureUpdateParamsV2* params = (UnityRenderingExtTextureUpdateParamsV2*)data;
		free(params->texData);
	}
}

UnityRenderingEventAndData UNITY_INTERFACE_EXPORT GetDiffuseOutputTexture()
{
	return TextureDiffuseOutputCallback;
}


// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

