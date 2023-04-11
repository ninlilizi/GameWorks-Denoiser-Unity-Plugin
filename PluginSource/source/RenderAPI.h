#pragma once

#include "Unity/IUnityGraphics.h"

#include <stddef.h>
#include <d3d12.h>

struct IUnityInterfaces;


// Super-simple "graphics abstraction". This is nothing like how a proper platform abstraction layer would look like;
// all this does is a base interface for whatever our plugin sample needs. Which is only "draw some triangles"
// and "modify a texture" at this point.
//
// There are implementations of this base class for D3D9, D3D11, OpenGL etc.; see individual RenderAPI_* files.
class RenderAPI
{
public:
	virtual ~RenderAPI() { }


	// Process general event like initialization, shutdown, device loss/reset etc.
	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;

	virtual void ReleaseResources() = 0;
	virtual void ReleaseResourcesSigma() = 0;

	// Initialize NDR
	virtual void Initialize(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_VIEWZ, void* IN_DIFF_RADIANCE_HITDIST, void* OUT_DIFF_RADIANCE_HITDIST) = 0;
	virtual void InitializeSigma(int renderWidth, int renderHeight, void* IN_MV, void* IN_NORMAL_ROUGHNESS, void* IN_SHADOWDATA, void* IN_SHADOW_TRANSLUCENCY, void* OUT_SHADOW_TRANSLUCENCY) = 0;
	virtual void Denoise() = 0;
	virtual void DenoiseSigma() = 0;
	virtual void SetMatrix(int frameIndex, float _viewToClipMatrix[16], float _worldToViewMatrix[16]) = 0;
	virtual void ReleaseNRD() = 0;
	virtual void ReleaseNRDSigma() = 0;
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);

