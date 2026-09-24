#pragma once

#include "CoreMinimal.h"

struct FGLSettingsPresetDef;
struct FGLYieldCategoryDef;

/** Yield scaling (ADR-0016, invariants E-1/E-2). */
namespace GLYield
{
	/** The multiplier a world preset applies to a category: 1 for non-scalable categories. */
	GRIDLANDSCORE_API double MultiplierFor(const FGLYieldCategoryDef& Category, const FGLSettingsPresetDef& Settings);

	/**
	 * Final count of an authored yield. Non-scalable categories return AuthoredCount unchanged,
	 * whatever the settings. Scalable ones return max(1, round(AuthoredCount * multiplier)), so a
	 * discovered repeatable source never yields nothing.
	 */
	GRIDLANDSCORE_API int32 Apply(int32 AuthoredCount, const FGLYieldCategoryDef& Category, const FGLSettingsPresetDef& Settings);
}
