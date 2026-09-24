#pragma once

#include "CoreMinimal.h"

struct FGLCapabilityDef;

/** Capability level maths (pure). Effects are per level; a level inherits nothing it does not list. */
namespace GLCapabilityRules
{
	GRIDLANDSCORE_API int32 MaxLevel(const FGLCapabilityDef& Capability);

	/** Value of effect Kind at the highest defined level <= Level; 0 if none. */
	GRIDLANDSCORE_API double EffectValue(const FGLCapabilityDef& Capability, int32 Level, FName Kind);
}
