# NRD Build Configuration

This documents the CMake configuration used to build NRD for this project.
Rebuild with `2-Build.bat` after running the deploy/configure step below.

## Configure & Deploy

```bat
cd PluginSource\NRD
1-Deploy.bat -DNRD_NORMAL_ENCODING=4
```

Or reconfigure an existing build:

```bat
cd PluginSource\NRD\_Build
cmake -DNRD_NORMAL_ENCODING=4 ..
```

Then build:

```bat
cd PluginSource\NRD
2-Build.bat
```

## Key Settings

| Setting | Value | Reason |
|---------|-------|--------|
| `NRD_NORMAL_ENCODING` | **4** (RGBA16_SNORM) | Matches Unity's `R16G16B16A16_SFloat` textures. Encoding 4 accepts RGBA16_SNORM, RGBA16_SFLOAT, and RGBA32_SFLOAT. |
| `NRD_ROUGHNESS_ENCODING` | 1 (default) | Linear roughness squared = perceptual roughness. |
| `NRD_NRI` | ON | Build NRI alongside NRD. |

## Normal Encoding Reference

| Value | Encoding | Expected Texture Format |
|-------|----------|------------------------|
| 0 | RGBA8_UNORM | RGBA8_UNORM |
| 1 | RGBA8_SNORM | RGBA8_SNORM |
| 2 | R10_G10_B10_A2_UNORM | R10_G10_B10_A2_UNORM (NRD default) |
| 3 | RGBA16_UNORM | RGBA16_UNORM |
| 4 | RGBA16_SNORM | RGBA16_SNORM, **RGBA16_SFLOAT**, RGBA32_SFLOAT |

## Output

Built DLLs are placed in `NRD/_Bin/Release/` and `NRD/_Bin/Debug/`.
Copy `NRD.dll` and `NRI.dll` to your Unity project's `Assets/Plugins/x86_64/` folder.
