# Unity Native Plugin for NVIDIA NRD (Real-Time Denoisers)

A Unity native plugin wrapping NVIDIA's [NRD](https://github.com/NVIDIAGameWorks/RayTracingDenoiser) library. Provides access to all 19 NRD denoiser types (Reblur, Relax, Sigma, Reference) through a generic data-driven API.

Supports the Unity **D3D12** renderer.

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

#### Lifecycle

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

// Get the last error code from NRDInitialize (main thread)
[DllImport("NKLIDenoising")]
private static extern int NRDGetLastError();
```

#### Per-Frame State

```csharp
// Set camera matrices and timing for the current frame (main thread, call before execute)
[DllImport("NKLIDenoising")]
private static extern void NRDSetMatrix(int frameIndex, float[] viewToClipMatrix, float[] worldToViewMatrix, float deltaTime);

// Set the primary light direction for SIGMA shadow denoisers (main thread, call before execute)
[DllImport("NKLIDenoising")]
private static extern void NRDSetLightDirection(float x, float y, float z);
```

#### Denoiser Settings

Settings are cached on the native side. Only re-send when values change.

```csharp
// Configure RELAX_DIFFUSE and RELAX_SPECULAR instances
[DllImport("NKLIDenoising")]
private static extern void NRDSetRelaxSettings(
    float diffusePrepassBlurRadius, float specularPrepassBlurRadius,
    float diffusePhiLuminance, float specularPhiLuminance,
    float diffuseMinLuminanceWeight, float specularMinLuminanceWeight,
    uint diffuseMaxAccumulatedFrameNum, uint specularMaxAccumulatedFrameNum,
    uint diffuseMaxFastAccumulatedFrameNum, uint specularMaxFastAccumulatedFrameNum,
    uint atrousIterationNum, uint hitDistanceReconstructionMode, bool enableAntiFirefly);

// Configure REBLUR_DIFFUSE instance
[DllImport("NKLIDenoising")]
private static extern void NRDSetReblurSettings(
    float diffusePrepassBlurRadius, float specularPrepassBlurRadius,
    float minBlurRadius, float maxBlurRadius,
    uint maxAccumulatedFrameNum, uint maxFastAccumulatedFrameNum,
    float lobeAngleFraction, float roughnessFraction,
    float minHitDistanceWeight, float planeDistanceSensitivity,
    uint hitDistanceReconstructionMode, bool enableAntiFirefly);

// Configure SIGMA_SHADOW instance
[DllImport("NKLIDenoising")]
private static extern void NRDSetSigmaSettings(
    float planeDistanceSensitivity, uint maxStabilizedFrameNum);
```

#### Dispatch

```csharp
// Get render-thread callback that executes a SINGLE denoiser (legacy — see NRDGetBatchCallback)
[DllImport("NKLIDenoising")]
private static extern IntPtr NRDGetExecuteCallback();

// Get render-thread callback that executes ALL initialized denoisers in one submission (preferred)
[DllImport("NKLIDenoising")]
private static extern IntPtr NRDGetBatchCallback();
```

### Batch Dispatch vs Individual Execute

The plugin provides two dispatch mechanisms:

**`NRDGetExecuteCallback()`** — Executes a single denoiser per `IssuePluginEvent` call. The event ID encodes both the denoiser type (bits 0-7) and the matrix ring buffer slot (bits 8-9): `(frameSlot << 8) | denoiserType`. Each call creates a separate D3D12 command list, submits it, and waits for the previous frame's fence.

**`NRDGetBatchCallback()`** (preferred) — Executes all initialized denoisers in a single `IssuePluginEvent` call. The event ID contains only the frame slot (bits 0-1): `frameCount & 0x3`. Internally, the batch callback:

1. Iterates all `NRD_DENOISER_COUNT` slots, collecting those where `g_initialized[i] == true`
2. Resets a single shared D3D12 command allocator and command list
3. Records all NRD dispatches into the shared command list, with `restoreInitialState = false`
4. Tracks which input resources have been transitioned to compute-SRV state by prior denoisers; subsequent denoisers declare these as already-in-SRV, so NRI sees matching states and skips the barrier entirely
5. Closes and submits the command list once
6. Reports both output (UAV) and input (compute-SRV) resource states back to Unity's resource tracking

This eliminates ~28 redundant UAV/SRV barrier transitions per frame from shared input textures (MV, NR, VZ, BCM) that would otherwise be transitioned independently by each denoiser. It also reduces render-thread overhead from repeated plugin callback entry/exit and D3D12 command list create/close/submit cycles.

### Error Handling

`NRDGetLastError()` returns the error code from the most recent `NRDInitialize` call:

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | `denoiserType` out of range |
| 2 | Graphics API not initialized (`s_CurrentAPI` is null) |
| 3 | Resource count mismatch |
| 4 | `CreateCommandObjects` failed |
| 5 | `RecreateD3D12` failed |
| 6 | Unknown |

**Note:** `NRDGetLastError()` may throw `System.EntryPointNotFoundException` on older plugin builds. Wrap in try/catch:

```csharp
bool ok = NRDInitialize(...);
int err = -1;
try { err = NRDGetLastError(); } catch (System.EntryPointNotFoundException) { err = -99; }
```

### Resource Arrays

Each denoiser type expects a specific set of texture resource pointers passed as an `IntPtr[]` to `NRDInitialize`. The resource order is defined per-denoiser in `NRDDenoiserConfig.cpp`. Common inputs shared by most denoisers:

* `IN_MV` — motion vectors
* `IN_NORMAL_ROUGHNESS` — packed normal + roughness
* `IN_BASECOLOR_METALNESS` — base color + metalness (Reblur/Relax families)
* `IN_VIEWZ` — view-space depth (not used by Sigma)

Denoiser-specific inputs/outputs vary — consult `NRDDenoiserConfig.cpp` for the exact resource table.

### Input Conventions

#### Matrices

NRD requires **non-jittered**, **column-major** view and projection matrices. The plugin handles the D3D12 render-texture Y-flip internally (negates column 1 of the projection matrix before passing to NRD), so the caller should pass `GL.GetGPUProjectionMatrix(proj, false)` — i.e. **without** the render-into-texture flip.

* **`viewToClipMatrix`** — `GL.GetGPUProjectionMatrix(_Camera.nonJitteredProjectionMatrix, false)`. Using `projectionMatrix` instead of `nonJitteredProjectionMatrix` will corrupt temporal reprojection if TAA jitter is active.
* **`worldToViewMatrix`** — `_Camera.worldToCameraMatrix` (Unity's standard world-to-camera transform).
* **`deltaTime`** — `Time.deltaTime` in seconds. The plugin converts to milliseconds for NRD's `timeDeltaBetweenFrames`. If 0, NRD falls back to internal timing (less reliable).

The plugin maintains a 4-slot ring buffer for matrices, indexed by `frameIndex & 3`. Previous-frame matrices are tracked automatically — the caller only needs to provide the current frame's matrices.

#### Motion Vectors

NRD expects **backward-pointing** screen-space motion vectors: `pixelUvPrev = pixelUv + mv.xy`, where UV is in [0, 1] range. Unity's `_CameraMotionVectorsTexture` is **forward-pointing** (`currentUv - previousUv`), so negate both components in your blit shader:

```hlsl
float2 mv = -_CameraMotionVectorsTexture[id.xy].xy;
```

The plugin sets `motionVectorScale = {1, 1, 0}` and `isMotionVectorInWorldSpace = false`.

#### Normal + Roughness

Pack using NRD's front-end function. Roughness is `1 - smoothness` (Unity's GBuffer1.a stores smoothness):

```hlsl
#include "NRDEncoding.hlsli"
#define NRD_HEADER_ONLY
#include "NRD.hlsli"

_OutNormalRoughness[id.xy] = NRD_FrontEnd_PackNormalAndRoughness(
    worldNormal, 1 - smoothness, 0);
```

#### Base Color + Metalness

Use GBuffer0 (diffuse albedo = `baseColor * (1 - metallic)`), **not** GBuffer1 (specular F0). Using specular F0 causes extreme amplification during NRD's internal demodulation:

```hlsl
_OutBaseMetalness[id.xy] = float4(_CameraGBufferTexture0[id.xy].rgb, 0);
```

#### View Z

Linear eye depth, computed from Unity's depth buffer:

```hlsl
float viewZ = LinearEyeDepth(_CameraDepthTexture[id.xy]);
```

#### Radiance + Hit Distance (Relax/Reblur)

Pack using the appropriate NRD front-end function. Set `sanitize = false` — enabling sanitize corrupts hit distances and causes temporal jitter:

```hlsl
_OutDiffuse[id.xy] = RELAX_FrontEnd_PackRadianceAndHitDist(radiance, hitDist, false);
```

#### Light Direction (Sigma)

`NRDSetLightDirection` takes the direction **from surface toward the light** in **world space**. NRD internally converts to view space. For a Unity directional light:

```csharp
Vector3 sunDir = -Sun.transform.forward;  // forward points away from light
NRDSetLightDirection(sunDir.x, sunDir.y, sunDir.z);
```

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

Set matrices, light direction, optionally update settings, flush the GPU, then issue the plugin event.

```csharp
// 1. Prepare NRD input textures (compute shader dispatches, blits, etc.)
//    ... your compute dispatches here ...

// 2. Set camera matrices — NRD requires NON-JITTERED matrices.
//    Use nonJitteredProjectionMatrix to avoid TAA sub-pixel jitter corrupting
//    NRD's temporal reprojection. The plugin applies the D3D12 render-texture
//    Y-flip internally, so pass renderIntoTexture: false.
Matrix4x4 viewToClip = GL.GetGPUProjectionMatrix(_Camera.nonJitteredProjectionMatrix, false);
Matrix4x4 worldToView = _Camera.worldToCameraMatrix;
float[] viewToClipArray = new float[16];
float[] worldToViewArray = new float[16];
MatrixToFloatArray(viewToClip, ref viewToClipArray);
MatrixToFloatArray(worldToView, ref worldToViewArray);
NRDSetMatrix(Time.frameCount, viewToClipArray, worldToViewArray, Time.deltaTime);

// 3. Set light direction for SIGMA shadow denoisers (world space).
//    Pass the direction FROM the surface TOWARD the light source.
if (Sun != null)
{
    Vector3 sunDir = -Sun.transform.forward;
    NRDSetLightDirection(sunDir.x, sunDir.y, sunDir.z);
}

// 4. Update settings (only when changed — cached on the native side)
if (settingsDirty)
{
    NRDSetRelaxSettings(...);
    NRDSetReblurSettings(...);
    NRDSetSigmaSettings(...);
    settingsDirty = false;
}

// 5. CRITICAL (D3D12): Flush all pending GPU commands before issuing plugin events.
//    Without this, the plugin's D3D12 command list may execute BEFORE the compute
//    dispatches that prepared NRD's input textures, causing NRD to read stale data
//    from the previous frame and producing temporal drift in the denoised output.
GL.Flush();

// 6. Execute ALL denoisers in one batch (preferred).
//    Event ID = frame slot only (bits 0-1). No denoiser type encoding needed.
int frameSlot = Time.frameCount & 0x3;
IntPtr batchCallback = NRDGetBatchCallback();
GL.IssuePluginEvent(batchCallback, frameSlot);
```

#### Individual Execute (legacy)

To execute denoisers individually, use `NRDGetExecuteCallback()`. The event ID encodes both the denoiser type (bits 0-7) and the matrix ring buffer slot (bits 8-9):

```csharp
int frameSlot = Time.frameCount & 0x3;
IntPtr executeCallback = NRDGetExecuteCallback();
GL.IssuePluginEvent(executeCallback, (frameSlot << 8) | (int)NRDDenoiserType.RELAX_DIFFUSE);
GL.IssuePluginEvent(executeCallback, (frameSlot << 8) | (int)NRDDenoiserType.SIGMA_SHADOW);
```

**Note the different event ID encoding**: batch uses `frameSlot` directly, individual execute uses `(frameSlot << 8) | denoiserType`.

### Async Compute

The batch callback is compatible with `CommandBuffer.IssuePluginEvent` and `Graphics.ExecuteCommandBufferAsync`, allowing NRD to overlap with other GPU work:

```csharp
// GPU fence after all RT dispatches complete
GraphicsFence fenceInputs = Graphics.CreateAsyncGraphicsFence();

// Submit NRD on async compute queue
CommandBuffer cmdNRD = new CommandBuffer();
cmdNRD.SetExecutionFlags(CommandBufferExecutionFlags.AsyncCompute);
cmdNRD.WaitOnAsyncGraphicsFence(fenceInputs);
cmdNRD.IssuePluginEvent(NRDGetBatchCallback(), frameCount & 0x3);
GraphicsFence fenceNRD = cmdNRD.CreateAsyncGraphicsFence();

Graphics.ExecuteCommandBufferAsync(cmdNRD, ComputeQueueType.Background);

// ... do other work (volumetric dispatches, upsamples, etc.) ...

// Wait for NRD before reading output textures
cmd.WaitOnAsyncGraphicsFence(fenceNRD);
```

Requires `SystemInfo.supportsAsyncCompute && SystemInfo.supportsGraphicsFence`. Fall back to synchronous graphics-queue dispatch otherwise.

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

## Important Notes

### Temporal History Initialization

All input/output render textures **must** be cleared to known-safe values before the first NRD dispatch. Uninitialized GPU memory in history buffers causes persistent ghosting artifacts — NRD's temporal accumulation blends the garbage into its internal state and it takes many frames (or never) to purge.

Safe clear values:
* **Radiance/hit-distance inputs** — `(0,0,0,0)`: NRD interprets as "no contribution"
* **Shadow penumbra inputs** — `(0,0,0,0)`: no shadow data

This also applies after any resource recreation (e.g., resolution change). Clear all textures passed to `NRDInitialize` before the first dispatch following reinitialization.

### Texture Pointer Persistence

`GetNativeTexturePtr()` values are captured at `NRDInitialize` time. If a `RenderTexture` is released and recreated (e.g., on resolution change), you **must** call `NRDReleaseAll()` and reinitialize all instances with fresh pointers. Stale pointers will cause GPU faults or silent corruption.

### Auto-Resize

`NRDInitialize` automatically detects dimension changes. If `renderWidth` or `renderHeight` differs from the previous call for the same `denoiserType`, it releases and recreates the instance internally. No need to call `NRDRelease` first.

## Legacy API

The legacy per-denoiser functions (`NRDInitializeRelax`, `NRDExecuteRelax`, `NRDReleaseRelax`, and equivalents for Sigma/Reblur) are still exported for backward compatibility. They are thin wrappers around the generic API. New code should use the generic API with `NRDGetBatchCallback()` for dispatch.
