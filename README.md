# Unity Native Plugin for Nvidia GameWorks Denoiser

This only supports the Unity DX12 renderer. Make sure this is set first.

CMake NRI and NRD projects.
* https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRI/README.md
* https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRD/README.md

Then you can build the solution.

Copy the following to your Unity plugins location to use
* NDI.DLL
* NRD.DLL
* RTDenoising.DLL



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
[DllImport("RTDenoising")]
private static extern void NRDInitialize(int renderWidth, int renderHeight, System.IntPtr IN_MV, System.IntPtr IN_NORMAL_ROUGHNESS, System.IntPtr IN_VIEWZ, System.IntPtr IN_DIFF_RADIANCE_HITDIST, System.IntPtr OUT_DIFF_RADIANCE_HITDIST);

[DllImport("RTDenoising")]
private static extern void NRDSetMatrix(int frameIndex, float[] viewToClipMatrix, float[] worldToViewMatrix);

[DllImport("RTDenoising")]
private static extern void NRDReleaseResources();

[DllImport("RTDenoising")]
private static extern IntPtr NRDExecute();

```

Cleanup
```
void OnDisable()
{
    NRDReleaseResources();
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
Matrix4x4 projectionMatrix = _Camera.projectionMatrix;
projectionMatrix = GL.GetGPUProjectionMatrix(projectionMatrix, true);
float[] matrixArray = new float[16];
MatrixToFloatArray(projectionMatrix, ref matrixArray);
// Set float arrays to plugin
NRDSetMatrix(Time.frameCount, matrixArray, matrixArray);

// Excute denoiser on render thread
GL.IssuePluginEvent(NRDExecute(), 0);
```


