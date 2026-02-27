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

	// Generic NRD interface — denoiserType maps to nrd::Denoiser enum value (0..18)
	virtual bool NRDInitialize(int denoiserType, int renderWidth, int renderHeight, void** resources, int resourceCount) = 0;
	virtual void NRDDenoise(int denoiserType) = 0;
	virtual void NRDRelease(int denoiserType) = 0;
	virtual void SetMatrix(int frameIndex, float viewToClipMatrix[16], float worldToViewMatrix[16]) = 0;
	virtual int GetLastInitError() { return 6; }
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);
