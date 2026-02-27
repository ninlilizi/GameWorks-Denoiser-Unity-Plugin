#pragma once

#include "../NRD/Include/NRD.h"

// Maximum number of resource slots a single denoiser type can use
static constexpr int MAX_DENOISER_RESOURCES = 12;

// Maximum number of output resources for Unity resource state tracking
static constexpr int MAX_OUTPUT_RESOURCES = 4;

// Total number of denoiser types (matches nrd::Denoiser::MAX_NUM)
static constexpr int NRD_DENOISER_COUNT = (int)nrd::Denoiser::MAX_NUM;

// Settings family for grouping denoiser-specific settings
enum class SettingsFamily : uint32_t
{
	REBLUR,
	RELAX,
	SIGMA,
	REFERENCE
};

// Describes a single resource slot in the denoiser's resource layout
struct ResourceSlotDesc
{
	nrd::ResourceType type;
	bool isOutput;
};

// Describes the complete resource layout for a denoiser type
struct DenoiserTypeDesc
{
	nrd::Denoiser denoiser;
	SettingsFamily settingsFamily;
	int resourceCount;
	ResourceSlotDesc resources[MAX_DENOISER_RESOURCES];
};

// Master table of all denoiser type descriptors (indexed by nrd::Denoiser enum value)
extern const DenoiserTypeDesc g_DenoiserTypeDescs[NRD_DENOISER_COUNT];
