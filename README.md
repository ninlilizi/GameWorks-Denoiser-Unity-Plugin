# Unity Native Plugin for Nvidia GameWorks Denoiser

## Note, this does not work yet!

This only supports the Unity DX12 renderer. Make sure this is set first.

CMake NRI and NRD projects.
* https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRI/README.md
* https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRD/README.md

Then you can build the solution.

Copy the following to your Unity plugins location to use
* NDI.DLL
* NRD.DLL
* RTDenoising.DLL


Import functions

```
[DllImport("RTDenoising")]
private static extern void UpdateResources(int renderWidth, int renderHeight, System.IntPtr IN_MV, System.IntPtr IN_NORMAL_ROUGHNESS, System.IntPtr IN_VIEWZ, System.IntPtr IN_DIFF_RADIANCE_HITDIST, System.IntPtr OUT_DIFF_RADIANCE_HITDIST);

[DllImport("RTDenoising")]
private static extern void UpdateParams(int frameIndex, float[] _viewToClipMatrix, float[] _worldToViewMatrix);

[DllImport("RTDenoising")]
private static extern IntPtr Initialize();

[DllImport("RTDenoising")]
private static extern IntPtr Denoise();
```
  
Rough Usage: (Do this at the end of a frame)


Initialize
```
UpdateResources(RTWidth, RTHeight, MotionVectorsRT.GetNativeTexturePtr(), NormalRoughnessRT.GetNativeTexturePtr(), LinearDepthRT.GetNativeTexturePtr(), DiffuseRadianceHitDistInRT.GetNativeTexturePtr(), DiffuseRadianceHitDistOutRT.GetNativeTexturePtr());
GL.IssuePluginEvent(Initialize(), 0);
```

Run
```
UpdateParams(Frame_Index, projectionMatrixArray, projectionMatrixArray);
GL.IssuePluginEvent(Denoise(), 0);
```
