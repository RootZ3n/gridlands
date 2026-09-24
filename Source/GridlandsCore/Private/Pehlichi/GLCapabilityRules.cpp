#include "Pehlichi/GLCapabilityRules.h"

#include "Content/GLContentDefinitions.h"

namespace GLCapabilityRules
{
	int32 MaxLevel(const FGLCapabilityDef& Capability)
	{
		int32 Max = 0;
		for (const FGLCapabilityLevelDef& Level : Capability.Levels)
		{
			Max = FMath::Max(Max, Level.Level);
		}
		return Max;
	}

	double EffectValue(const FGLCapabilityDef& Capability, int32 Level, FName Kind)
	{
		int32 BestLevel = 0;
		double Value = 0.0;
		for (const FGLCapabilityLevelDef& Def : Capability.Levels)
		{
			if (Def.Level > Level || Def.Level < BestLevel)
			{
				continue;
			}
			for (const FGLCapabilityEffectDef& Effect : Def.Effects)
			{
				if (Effect.Kind == Kind)
				{
					BestLevel = Def.Level;
					Value = Effect.Value;
				}
			}
		}
		return Value;
	}
}
