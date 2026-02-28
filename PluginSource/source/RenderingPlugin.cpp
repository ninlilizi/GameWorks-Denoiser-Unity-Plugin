// Low level rendering Unity plugin — NRD denoiser integration

#include "PlatformBase.h"
#include "RenderAPI.h"
#include "NRDDenoiserConfig.h"

#include <assert.h>
#include <math.h>
#include <mutex>
#include <vector>

#include "Unity/IUnityRenderingExtensions.h"

// Gameworks — use NRI fetched by NRD's CMake to match built NRI.dll
#include "../NRD/_Build/_deps/nri-src/Include/NRI.h"

// Per-denoiser state arrays (indexed by nrd::Denoiser enum value)
static bool g_initialized[NRD_DENOISER_COUNT] = {};
static int g_prevWidth[NRD_DENOISER_COUNT] = {};
static int g_prevHeight[NRD_DENOISER_COUNT] = {};
static std::mutex g_mutex;

// Diagnostic: last error code from NRDInitialize
static int g_lastInitError = 0;
// Error codes:
// 0 = success
// 1 = denoiserType out of range
// 2 = s_CurrentAPI is null (graphics not initialized)
// 3 = resource count mismatch
// 4 = CreateCommandObjects failed
// 5 = RecreateD3D12 failed
// 6 = unknown


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

		for (int i = 0; i < NRD_DENOISER_COUNT; i++)
		{
			g_initialized[i] = false;
			g_prevWidth[i] = 0;
			g_prevHeight[i] = 0;
		}
	}
}


// --------------------------------------------------------------------------
// Generic render thread callback (try_to_lock — skip frame if init/release in progress)

static void UNITY_INTERFACE_API OnExecuteEventGeneric(int eventID)
{
	if (eventID < 0 || eventID >= NRD_DENOISER_COUNT)
		return;

	std::unique_lock<std::mutex> lock(g_mutex, std::try_to_lock);
	if (!lock.owns_lock())
		return;

	if (s_CurrentAPI == NULL || !g_initialized[eventID])
		return;

	s_CurrentAPI->NRDDenoise(eventID);
}


// --------------------------------------------------------------------------
// Legacy render thread trampolines (map old per-denoiser callbacks to generic)

static void UNITY_INTERFACE_API OnExecuteEventLegacyRelax(int eventID)
{
	OnExecuteEventGeneric((int)nrd::Denoiser::RELAX_DIFFUSE);
}

static void UNITY_INTERFACE_API OnExecuteEventLegacySigma(int eventID)
{
	OnExecuteEventGeneric((int)nrd::Denoiser::SIGMA_SHADOW);
}

static void UNITY_INTERFACE_API OnExecuteEventLegacyReblur(int eventID)
{
	OnExecuteEventGeneric((int)nrd::Denoiser::REBLUR_DIFFUSE);
}


// --------------------------------------------------------------------------
// Generic external API (main thread — lock_guard blocks until render thread finishes)

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDInitialize(int denoiserType, int renderWidth, int renderHeight, void** resources, int resourceCount)
{
	if (denoiserType < 0 || denoiserType >= NRD_DENOISER_COUNT)
	{
		g_lastInitError = 1;
		return false;
	}

	std::lock_guard<std::mutex> lock(g_mutex);

	if (s_CurrentAPI == nullptr)
	{
		g_lastInitError = 2;
		return false;
	}

	// Check for resize — release and re-create if dimensions changed
	if ((renderWidth != g_prevWidth[denoiserType] || renderHeight != g_prevHeight[denoiserType]) && g_initialized[denoiserType])
	{
		s_CurrentAPI->NRDRelease(denoiserType);
		g_initialized[denoiserType] = false;

		g_prevWidth[denoiserType] = renderWidth;
		g_prevHeight[denoiserType] = renderHeight;
	}

	g_initialized[denoiserType] = s_CurrentAPI->NRDInitialize(denoiserType, renderWidth, renderHeight, resources, resourceCount);
	if (!g_initialized[denoiserType])
		g_lastInitError = s_CurrentAPI->GetLastInitError();
	else
		g_lastInitError = 0;

	return g_initialized[denoiserType];
}


extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDGetLastError()
{
	return g_lastInitError;
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDRelease(int denoiserType)
{
	if (denoiserType < 0 || denoiserType >= NRD_DENOISER_COUNT)
		return;

	std::lock_guard<std::mutex> lock(g_mutex);

	if (g_initialized[denoiserType] && s_CurrentAPI != nullptr)
	{
		s_CurrentAPI->NRDRelease(denoiserType);
		g_initialized[denoiserType] = false;
	}
}


extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDGetExecuteCallback()
{
	return OnExecuteEventGeneric;
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseAll()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (s_CurrentAPI != nullptr)
	{
		s_CurrentAPI->NRDReleaseAllSlots();
		for (int i = 0; i < NRD_DENOISER_COUNT; i++)
			g_initialized[i] = false;
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDSetMatrix(int frameIndex, float viewToClipMatrix[16], float worldToViewMatrix[16])
{
	if (s_CurrentAPI == nullptr)
		return;

	s_CurrentAPI->SetMatrix(frameIndex, viewToClipMatrix, worldToViewMatrix);
}


// --------------------------------------------------------------------------
// Legacy external API — thin wrappers calling the generic API

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDInitializeRelax(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_BASECOLOR_METALNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	// Resource order must match RELAX_DIFFUSE table: MV, NR, BCM, VZ, DIFF, OUT
	void* resources[] = { IN_MV, IN_NORMAL_ROUGHNESS, IN_BASECOLOR_METALNESS, IN_VIEWZ, IN_DIFF_RADIANCE_HITDIST, OUT_DIFF_RADIANCE_HITDIST };
	NRDInitialize((int)nrd::Denoiser::RELAX_DIFFUSE, renderWidth, renderHeight, resources, 6);
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDInitializeSigma(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_SHADOWDATA, void* IN_SHADOW_TRANSLUCENCY, void* OUT_SHADOW_TRANSLUCENCY)
{
	// SIGMA_SHADOW table: MV, NR, PENUMBRA, OUT_SHADOW (4 resources)
	// IN_SHADOW_TRANSLUCENCY is dropped — SIGMA_SHADOW does not use it
	void* resources[] = { IN_MV, IN_NORMAL_ROUGHNESS, IN_SHADOWDATA, OUT_SHADOW_TRANSLUCENCY };
	NRDInitialize((int)nrd::Denoiser::SIGMA_SHADOW, renderWidth, renderHeight, resources, 4);
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDInitializeReblur(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_BASECOLOR_METALNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST)
{
	// Resource order must match REBLUR_DIFFUSE table: MV, NR, BCM, VZ, DIFF, OUT
	void* resources[] = { IN_MV, IN_NORMAL_ROUGHNESS, IN_BASECOLOR_METALNESS, IN_VIEWZ, IN_DIFF_RADIANCE_HITDIST, OUT_DIFF_RADIANCE_HITDIST };
	NRDInitialize((int)nrd::Denoiser::REBLUR_DIFFUSE, renderWidth, renderHeight, resources, 6);
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseRelax()
{
	NRDRelease((int)nrd::Denoiser::RELAX_DIFFUSE);
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseSigma()
{
	NRDRelease((int)nrd::Denoiser::SIGMA_SHADOW);
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDReleaseReblur()
{
	NRDRelease((int)nrd::Denoiser::REBLUR_DIFFUSE);
}


// --------------------------------------------------------------------------
// Legacy execute callback functions

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDExecuteRelax()
{
	return OnExecuteEventLegacyRelax;
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDExecuteSigma()
{
	return OnExecuteEventLegacySigma;
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NRDExecuteReblur()
{
	return OnExecuteEventLegacyReblur;
}
