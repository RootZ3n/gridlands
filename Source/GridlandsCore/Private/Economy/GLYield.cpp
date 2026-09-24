#include "Economy/GLYield.h"

#include "Content/GLContentDefinitions.h"

namespace GLYield
{
	double MultiplierFor(const FGLYieldCategoryDef& Category, const FGLSettingsPresetDef& Settings)
	{
		if (!Category.Scalable)
		{
			return 1.0;
		}
		if (Category.Setting == TEXT("resourceYield"))
		{
			return Settings.YieldMultipliers.ResourceYield;
		}
		if (Category.Setting == TEXT("creatureDrops"))
		{
			return Settings.YieldMultipliers.CreatureDrops;
		}
		ensureMsgf(false, TEXT("Yield category %s names unknown setting %s"), *Category.Id.ToString(), *Category.Setting.ToString());
		return 1.0;
	}

	int32 Apply(int32 AuthoredCount, const FGLYieldCategoryDef& Category, const FGLSettingsPresetDef& Settings)
	{
		if (!Category.Scalable || AuthoredCount <= 0)
		{
			return AuthoredCount;
		}
		return FMath::Max(1, FMath::RoundToInt(AuthoredCount * MultiplierFor(Category, Settings)));
	}
}
