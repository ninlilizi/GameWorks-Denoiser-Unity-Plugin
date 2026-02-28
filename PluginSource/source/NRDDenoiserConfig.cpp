#include "NRDDenoiserConfig.h"

// Master resource layout table for all 19 NRD denoiser types.
// Resource order per entry: common inputs first, type-specific inputs, then outputs.
// The C# caller must pack resources in this exact order.
//
// Common inputs (used by most denoisers):
//   IN_MV, IN_NORMAL_ROUGHNESS, IN_VIEWZ
// Exceptions:
//   SIGMA_SHADOW/TRANSLUCENCY: IN_VIEWZ required (used in classify, blur, post-blur, temporal stabilization, split screen)
//   REFERENCE: no common inputs
//
// IN_BASECOLOR_METALNESS is included for REBLUR/RELAX types that use stabilization.

const DenoiserTypeDesc g_DenoiserTypeDescs[NRD_DENOISER_COUNT] =
{
	// [0] REBLUR_DIFFUSE
	// INPUTS: IN_DIFF_RADIANCE_HITDIST (optional: IN_DIFF_CONFIDENCE, IN_DISOCCLUSION_THRESHOLD_MIX)
	// OUTPUTS: OUT_DIFF_RADIANCE_HITDIST
	{
		nrd::Denoiser::REBLUR_DIFFUSE, SettingsFamily::REBLUR, 6,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_BASECOLOR_METALNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, true },
		}
	},

	// [1] REBLUR_DIFFUSE_OCCLUSION
	// INPUTS: IN_DIFF_HITDIST
	// OUTPUTS: OUT_DIFF_HITDIST
	{
		nrd::Denoiser::REBLUR_DIFFUSE_OCCLUSION, SettingsFamily::REBLUR, 5,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_HITDIST, false },
			{ nrd::ResourceType::OUT_DIFF_HITDIST, true },
		}
	},

	// [2] REBLUR_DIFFUSE_SH
	// INPUTS: IN_DIFF_SH0, IN_DIFF_SH1
	// OUTPUTS: OUT_DIFF_SH0, OUT_DIFF_SH1
	{
		nrd::Denoiser::REBLUR_DIFFUSE_SH, SettingsFamily::REBLUR, 7,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_SH0, false },
			{ nrd::ResourceType::IN_DIFF_SH1, false },
			{ nrd::ResourceType::OUT_DIFF_SH0, true },
			{ nrd::ResourceType::OUT_DIFF_SH1, true },
		}
	},

	// [3] REBLUR_SPECULAR
	// INPUTS: IN_SPEC_RADIANCE_HITDIST (optional: IN_BASECOLOR_METALNESS)
	// OUTPUTS: OUT_SPEC_RADIANCE_HITDIST
	{
		nrd::Denoiser::REBLUR_SPECULAR, SettingsFamily::REBLUR, 6,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_BASECOLOR_METALNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, true },
		}
	},

	// [4] REBLUR_SPECULAR_OCCLUSION
	// INPUTS: IN_SPEC_HITDIST
	// OUTPUTS: OUT_SPEC_HITDIST
	{
		nrd::Denoiser::REBLUR_SPECULAR_OCCLUSION, SettingsFamily::REBLUR, 5,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_SPEC_HITDIST, false },
			{ nrd::ResourceType::OUT_SPEC_HITDIST, true },
		}
	},

	// [5] REBLUR_SPECULAR_SH
	// INPUTS: IN_SPEC_SH0, IN_SPEC_SH1 (optional: IN_BASECOLOR_METALNESS)
	// OUTPUTS: OUT_SPEC_SH0, OUT_SPEC_SH1
	{
		nrd::Denoiser::REBLUR_SPECULAR_SH, SettingsFamily::REBLUR, 8,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_BASECOLOR_METALNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_SPEC_SH0, false },
			{ nrd::ResourceType::IN_SPEC_SH1, false },
			{ nrd::ResourceType::OUT_SPEC_SH0, true },
			{ nrd::ResourceType::OUT_SPEC_SH1, true },
		}
	},

	// [6] REBLUR_DIFFUSE_SPECULAR
	// INPUTS: IN_DIFF_RADIANCE_HITDIST, IN_SPEC_RADIANCE_HITDIST (optional: IN_BASECOLOR_METALNESS)
	// OUTPUTS: OUT_DIFF_RADIANCE_HITDIST, OUT_SPEC_RADIANCE_HITDIST
	{
		nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR, SettingsFamily::REBLUR, 8,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_BASECOLOR_METALNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, true },
			{ nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, true },
		}
	},

	// [7] REBLUR_DIFFUSE_SPECULAR_OCCLUSION
	// INPUTS: IN_DIFF_HITDIST, IN_SPEC_HITDIST
	// OUTPUTS: OUT_DIFF_HITDIST, OUT_SPEC_HITDIST
	{
		nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR_OCCLUSION, SettingsFamily::REBLUR, 7,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_HITDIST, false },
			{ nrd::ResourceType::IN_SPEC_HITDIST, false },
			{ nrd::ResourceType::OUT_DIFF_HITDIST, true },
			{ nrd::ResourceType::OUT_SPEC_HITDIST, true },
		}
	},

	// [8] REBLUR_DIFFUSE_SPECULAR_SH
	// INPUTS: IN_DIFF_SH0, IN_DIFF_SH1, IN_SPEC_SH0, IN_SPEC_SH1 (optional: IN_BASECOLOR_METALNESS)
	// OUTPUTS: OUT_DIFF_SH0, OUT_DIFF_SH1, OUT_SPEC_SH0, OUT_SPEC_SH1
	{
		nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR_SH, SettingsFamily::REBLUR, 12,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_BASECOLOR_METALNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_SH0, false },
			{ nrd::ResourceType::IN_DIFF_SH1, false },
			{ nrd::ResourceType::IN_SPEC_SH0, false },
			{ nrd::ResourceType::IN_SPEC_SH1, false },
			{ nrd::ResourceType::OUT_DIFF_SH0, true },
			{ nrd::ResourceType::OUT_DIFF_SH1, true },
			{ nrd::ResourceType::OUT_SPEC_SH0, true },
			{ nrd::ResourceType::OUT_SPEC_SH1, true },
		}
	},

	// [9] REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION
	// INPUTS: IN_DIFF_DIRECTION_HITDIST
	// OUTPUTS: OUT_DIFF_DIRECTION_HITDIST
	{
		nrd::Denoiser::REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION, SettingsFamily::REBLUR, 5,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_DIRECTION_HITDIST, false },
			{ nrd::ResourceType::OUT_DIFF_DIRECTION_HITDIST, true },
		}
	},

	// [10] RELAX_DIFFUSE
	// INPUTS: IN_DIFF_RADIANCE_HITDIST
	// OUTPUTS: OUT_DIFF_RADIANCE_HITDIST
	// IN_BASECOLOR_METALNESS included for legacy compatibility
	{
		nrd::Denoiser::RELAX_DIFFUSE, SettingsFamily::RELAX, 6,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_BASECOLOR_METALNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, true },
		}
	},

	// [11] RELAX_DIFFUSE_SH
	// INPUTS: IN_DIFF_SH0, IN_DIFF_SH1
	// OUTPUTS: OUT_DIFF_SH0, OUT_DIFF_SH1
	{
		nrd::Denoiser::RELAX_DIFFUSE_SH, SettingsFamily::RELAX, 7,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_SH0, false },
			{ nrd::ResourceType::IN_DIFF_SH1, false },
			{ nrd::ResourceType::OUT_DIFF_SH0, true },
			{ nrd::ResourceType::OUT_DIFF_SH1, true },
		}
	},

	// [12] RELAX_SPECULAR
	// INPUTS: IN_SPEC_RADIANCE_HITDIST
	// OUTPUTS: OUT_SPEC_RADIANCE_HITDIST
	{
		nrd::Denoiser::RELAX_SPECULAR, SettingsFamily::RELAX, 5,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, true },
		}
	},

	// [13] RELAX_SPECULAR_SH
	// INPUTS: IN_SPEC_SH0, IN_SPEC_SH1
	// OUTPUTS: OUT_SPEC_SH0, OUT_SPEC_SH1
	{
		nrd::Denoiser::RELAX_SPECULAR_SH, SettingsFamily::RELAX, 7,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_SPEC_SH0, false },
			{ nrd::ResourceType::IN_SPEC_SH1, false },
			{ nrd::ResourceType::OUT_SPEC_SH0, true },
			{ nrd::ResourceType::OUT_SPEC_SH1, true },
		}
	},

	// [14] RELAX_DIFFUSE_SPECULAR
	// INPUTS: IN_DIFF_RADIANCE_HITDIST, IN_SPEC_RADIANCE_HITDIST
	// OUTPUTS: OUT_DIFF_RADIANCE_HITDIST, OUT_SPEC_RADIANCE_HITDIST
	{
		nrd::Denoiser::RELAX_DIFFUSE_SPECULAR, SettingsFamily::RELAX, 7,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST, false },
			{ nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, true },
			{ nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, true },
		}
	},

	// [15] RELAX_DIFFUSE_SPECULAR_SH
	// INPUTS: IN_DIFF_SH0, IN_DIFF_SH1, IN_SPEC_SH0, IN_SPEC_SH1
	// OUTPUTS: OUT_DIFF_SH0, OUT_DIFF_SH1, OUT_SPEC_SH0, OUT_SPEC_SH1
	{
		nrd::Denoiser::RELAX_DIFFUSE_SPECULAR_SH, SettingsFamily::RELAX, 11,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_DIFF_SH0, false },
			{ nrd::ResourceType::IN_DIFF_SH1, false },
			{ nrd::ResourceType::IN_SPEC_SH0, false },
			{ nrd::ResourceType::IN_SPEC_SH1, false },
			{ nrd::ResourceType::OUT_DIFF_SH0, true },
			{ nrd::ResourceType::OUT_DIFF_SH1, true },
			{ nrd::ResourceType::OUT_SPEC_SH0, true },
			{ nrd::ResourceType::OUT_SPEC_SH1, true },
		}
	},

	// [16] SIGMA_SHADOW
	// INPUTS: IN_PENUMBRA, OUT_SHADOW_TRANSLUCENCY (history)
	// OUTPUTS: OUT_SHADOW_TRANSLUCENCY
	// IN_VIEWZ required (used in classify tiles, blur, post-blur, temporal stabilization, split screen)
	// IN_MV used only if stabilizationStrength != 0
	{
		nrd::Denoiser::SIGMA_SHADOW, SettingsFamily::SIGMA, 5,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_PENUMBRA, false },
			{ nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY, true },
		}
	},

	// [17] SIGMA_SHADOW_TRANSLUCENCY
	// INPUTS: IN_PENUMBRA, IN_TRANSLUCENCY, OUT_SHADOW_TRANSLUCENCY (history)
	// OUTPUTS: OUT_SHADOW_TRANSLUCENCY
	// IN_VIEWZ required (same passes as SIGMA_SHADOW)
	{
		nrd::Denoiser::SIGMA_SHADOW_TRANSLUCENCY, SettingsFamily::SIGMA, 6,
		{
			{ nrd::ResourceType::IN_MV, false },
			{ nrd::ResourceType::IN_NORMAL_ROUGHNESS, false },
			{ nrd::ResourceType::IN_VIEWZ, false },
			{ nrd::ResourceType::IN_PENUMBRA, false },
			{ nrd::ResourceType::IN_TRANSLUCENCY, false },
			{ nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY, true },
		}
	},

	// [18] REFERENCE
	// INPUTS: IN_SIGNAL
	// OUTPUTS: OUT_SIGNAL
	// Does not use IN_MV, IN_NORMAL_ROUGHNESS, or IN_VIEWZ
	{
		nrd::Denoiser::REFERENCE, SettingsFamily::REFERENCE, 2,
		{
			{ nrd::ResourceType::IN_SIGNAL, false },
			{ nrd::ResourceType::OUT_SIGNAL, true },
		}
	},
};
