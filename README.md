# Unity Native Plugin for NVIDIA NRD (Real-Time Denoisers)

A Unity native plugin wrapping NVIDIA's [NRD](https://github.com/NVIDIAGameWorks/RayTracingDenoiser) library. Provides access to all 19 NRD denoiser types (Reblur, Relax, Sigma, Reference) through a generic data-driven API.

Supports the Unity **D3D11** and **D3D12** renderers.

## Building

CMake the NRI and NRD projects first:
* [NRI build instructions](https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRI/README.md)
* [NRD build instructions](https://github.com/ninlilizi/GameWorks-Denoiser-Unity-Plugin/blob/main/PluginSource/NRD/README.md)

Then build the Visual Studio solution in `PluginSource/projects/VisualStudio2022/`.

Copy the following to your Unity project's `Assets/Plugins/x86_64/` folder:
* `NRI.dll`
* `NRD.dll`
* `NKLIDenoising.dll`

## Denoiser Types

The plugin supports all 19 NRD denoiser types, selected by integer index:

| Index | Denoiser | Family |
|-------|----------|--------|
| 0 | REBLUR_DIFFUSE | Reblur |
| 1 | REBLUR_DIFFUSE_OCCLUSION | Reblur |
| 2 | REBLUR_DIFFUSE_SH | Reblur |
| 3 | REBLUR_SPECULAR | Reblur |
| 4 | REBLUR_SPECULAR_OCCLUSION | Reblur |
| 5 | REBLUR_SPECULAR_SH | Reblur |
| 6 | REBLUR_DIFFUSE_SPECULAR | Reblur |
| 7 | REBLUR_DIFFUSE_SPECULAR_OCCLUSION | Reblur |
| 8 | REBLUR_DIFFUSE_SPECULAR_SH | Reblur |
| 9 | REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION | Reblur |
| 10 | RELAX_DIFFUSE | Relax |
| 11 | RELAX_DIFFUSE_SH | Relax |
| 12 | RELAX_SPECULAR | Relax |
| 13 | RELAX_SPECULAR_SH | Relax |
| 14 | RELAX_DIFFUSE_SPECULAR | Relax |
| 15 | RELAX_DIFFUSE_SPECULAR_SH | Relax |
| 16 | SIGMA_SHADOW | Sigma |
| 17 | SIGMA_SHADOW_TRANSLUCENCY | Sigma |
| 18 | REFERENCE | Reference |

## API

### Exported Functions

```csharp
// Initialize a denoiser with its required resources (main thread)
[DllImport("NKLIDenoising")]
private static extern bool NRDInitialize(int denoiserType, int renderWidth, int renderHeight, IntPtr[] resources, int resourceCount);

// Release a specific denoiser (main thread)
[DllImport("NKLIDenoising")]
private static extern void NRDRelease(int denoiserType);

// Release all active denoisers (main thread)
[DllImport("NKLIDenoising")]
private static extern void NRDReleaseAll();

// Get the render-thread callback pointer — pass denoiser type as eventID (main thread)
[DllImport("NKLIDenoising")]
private static extern IntPtr NRDGetExecuteCallback();

// Set camera matrices for the current frame (main thread, call before execute)
[DllImport("NKLIDenoising")]
private static extern void NRDSetMatrix(int frameIndex, float[] viewToClipMatrix, float[] worldToViewMatrix);
```

### Resource Arrays

Each denoiser type expects a specific set of texture resource pointers passed as an `IntPtr[]` to `NRDInitialize`. The resource order is defined per-denoiser in `NRDDenoiserConfig.cpp`. Common inputs shared by most denoisers:

* `IN_MV` — motion vectors
* `IN_NORMAL_ROUGHNESS` — packed normal + roughness
* `IN_BASECOLOR_METALNESS` — base color + metalness (Reblur/Relax families)
* `IN_VIEWZ` — view-space depth (not used by Sigma)

Denoiser-specific inputs/outputs vary — consult `NRDDenoiserConfig.cpp` for the exact resource table.

## Usage (Unity C#)

### Render Texture Format

All textures passed to the plugin must use `R16G16B16A16_SFloat`:

```csharp
RenderTextureDescriptor rtDesc = new RenderTextureDescriptor()
{
    colorFormat = RenderTextureFormat.ARGBHalf,
    graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat,
    enableRandomWrite = true,
    // ... other fields
};
```

### Enum (mirror in C# to match plugin indices)

```csharp
public enum NRDDenoiserType
{
    REBLUR_DIFFUSE = 0,
    REBLUR_DIFFUSE_OCCLUSION = 1,
    REBLUR_DIFFUSE_SH = 2,
    REBLUR_SPECULAR = 3,
    REBLUR_SPECULAR_OCCLUSION = 4,
    REBLUR_SPECULAR_SH = 5,
    REBLUR_DIFFUSE_SPECULAR = 6,
    REBLUR_DIFFUSE_SPECULAR_OCCLUSION = 7,
    REBLUR_DIFFUSE_SPECULAR_SH = 8,
    REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION = 9,
    RELAX_DIFFUSE = 10,
    RELAX_DIFFUSE_SH = 11,
    RELAX_SPECULAR = 12,
    RELAX_SPECULAR_SH = 13,
    RELAX_DIFFUSE_SPECULAR = 14,
    RELAX_DIFFUSE_SPECULAR_SH = 15,
    SIGMA_SHADOW = 16,
    SIGMA_SHADOW_TRANSLUCENCY = 17,
    REFERENCE = 18,
}
```

### Initialize

Call after creating render textures. Pass resources as an `IntPtr[]` in the order the denoiser expects:

```csharp
// Example: RELAX_DIFFUSE requires 6 resources
NRDInitialize((int)NRDDenoiserType.RELAX_DIFFUSE, width, height,
    new IntPtr[] {
        RT_NRD_IN_MV.GetNativeTexturePtr(),
        RT_NRD_IN_NORMAL_ROUGHNESS.GetNativeTexturePtr(),
        RT_NRD_IN_BASECOLOR_METALNESS.GetNativeTexturePtr(),
        RT_NRD_IN_VIEWZ.GetNativeTexturePtr(),
        RT_NRD_IN_DIFF_RADIANCE_HITDIST.GetNativeTexturePtr(),
        RT_NRD_OUT_DIFF_RADIANCE_HITDIST.GetNativeTexturePtr()
    }, 6);

// Example: SIGMA_SHADOW requires 4 resources (no VIEWZ or BASECOLOR_METALNESS)
NRDInitialize((int)NRDDenoiserType.SIGMA_SHADOW, width, height,
    new IntPtr[] {
        RT_NRD_IN_MV.GetNativeTexturePtr(),
        RT_NRD_IN_NORMAL_ROUGHNESS.GetNativeTexturePtr(),
        RT_NRD_IN_SHADOWDATA.GetNativeTexturePtr(),
        RT_NRD_OUT_SHADOW_TRANSLUCENCY.GetNativeTexturePtr()
    }, 4);
```

### Execute (per frame)

Set matrices first, then issue plugin events. The denoiser type is passed as the `eventID`:

```csharp
// Set camera matrices
Matrix4x4 viewToClip = GL.GetGPUProjectionMatrix(_Camera.projectionMatrix, false);
Matrix4x4 worldToView = _Camera.worldToCameraMatrix;
float[] viewToClipArray = new float[16];
float[] worldToViewArray = new float[16];
MatrixToFloatArray(viewToClip, ref viewToClipArray);
MatrixToFloatArray(worldToView, ref worldToViewArray);
NRDSetMatrix(Time.frameCount, viewToClipArray, worldToViewArray);

// Execute denoisers on the render thread
IntPtr executeCallback = NRDGetExecuteCallback();
GL.IssuePluginEvent(executeCallback, (int)NRDDenoiserType.RELAX_DIFFUSE);
GL.IssuePluginEvent(executeCallback, (int)NRDDenoiserType.REBLUR_DIFFUSE);
```

### Cleanup

```csharp
void OnDisable()
{
    NRDReleaseAll();  // releases all active denoisers
}
```

Or release individually:

```csharp
NRDRelease((int)NRDDenoiserType.RELAX_DIFFUSE);
```

## Legacy API

The legacy per-denoiser functions (`NRDInitializeRelax`, `NRDExecuteRelax`, `NRDReleaseRelax`, and equivalents for Sigma/Reblur) are still exported for backward compatibility. New code should use the generic API above.
