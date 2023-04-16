# Unity Native Plugin for Nvidia GameWorks Denoiser

This only supports the Unity DX12 renderer. Make sure this is set first.

CMake NRI and NRD projects.
* https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRI/README.md
* https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRD/README.md

Then you can build the solution.

Copy the following to your Unity plugins location to use
* NDI.DLL
* NRD.DLL
* NKLIDenoising.DLL



Create the required render targets with this format
```
RenderTextureDescriptor rtDesc = new RenderTextureDescriptor()
{
    rtDesc.colorFormat = RenderTextureFormat.ARGBHalf;
    rtDesc.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
};
```

Import functions

```
[DllImport("NKLIDenoising")]
private static extern void NRDInitializeRelax(int renderWidth, int renderHeight, System.IntPtr IN_MV, System.IntPtr IN_NORMAL_ROUGHNESS, System.IntPtr IN_VIEWZ, System.IntPtr IN_DIFF_RADIANCE_HITDIST, System.IntPtr OUT_DIFF_RADIANCE_HITDIST);

[DllImport("NKLIDenoising")]
private static extern void NRDSetMatrix(int frameIndex, float[] viewToClipMatrix, float[] worldToViewMatrix);

[DllImport("NKLIDenoising")]
private static extern void NRDReleaseRelax();

[DllImport("NKLIDenoising")]
private static extern IntPtr NRDExecuteRelax();

```

Cleanup
```
void OnDisable()
{
    NRDReleaseRelax();
}
```
  
Rough Usage: (Do this at the end of a frame)

Initalize after resource creation
```
NRDInitialize(
    RT_DiffuseOutput.width,
    RT_DiffuseOutput.height,
    RT_NRD_IN_MV.GetNativeTexturePtr(),
    RT_NRD_IN_NORMAL_ROUGHNESS.GetNativeTexturePtr(),
    RT_NRD_IN_VIEWZ.GetNativeTexturePtr(),
    RT_NRD_IN_DIFF_RADIANCE_HITDIST.GetNativeTexturePtr(),
    RT_NRD_OUT_DIFF_RADIANCE_HITDIST.GetNativeTexturePtr());
```

Run
```
// Transfer required projection matrices to floats
Matrix4x4 viewToClip = _Camera.projectionMatrix;
Matrix4x4 worldToView = _Camera.worldToCameraMatrix;
float[] viewToClipArray = new float[16];
float[] worldToViewArray = new float[16];
MatrixToFloatArray(viewToClip, ref viewToClipArray);
MatrixToFloatArray(worldToView, ref worldToViewArray);
// Set float arrays to plugin
NRDSetMatrix(Time.frameCount, viewToClipArray, worldToViewArray);

// Excute denoiser on render thread
GL.IssuePluginEvent(NRDExecuteRelax(), 0);
```

Both *Reblur* and *Relax* methods are supported. Replace *Relax* with *Reblur* in the function calls to use *Reblur*.
