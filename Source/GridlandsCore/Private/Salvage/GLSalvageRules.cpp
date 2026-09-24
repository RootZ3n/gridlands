#include "Salvage/GLSalvageRules.h"

#include "Content/GLContentDefinitions.h"

namespace GLSalvageRules
{
	bool CanSalvage(const FGLSalvageDef& Salvage, const FGLItemDef* Tool, FText* OutReason)
	{
		if (Salvage.RequiresTool.IsNone() || (Tool && Tool->IsTool() && Tool->Tool.ToolClass == Salvage.RequiresTool))
		{
			return true;
		}
		if (OutReason)
		{
			*OutReason = FText::Format(NSLOCTEXT("Gridlands", "NeedsTool", "Needs a {0}"), FText::FromName(Salvage.RequiresTool));
		}
		return false;
	}

	double ToolMultiplier(const FGLSalvageDef& Salvage, const FGLItemDef* Tool)
	{
		double Best = 1.0;
		if (Tool && Tool->IsTool())
		{
			for (const FGLToolEfficiencyDef& Entry : Salvage.ToolEfficiency)
			{
				if (Entry.ToolClass == Tool->Tool.ToolClass && Tool->Tool.Tier >= Entry.MinTier)
				{
					Best = FMath::Max(Best, Entry.Multiplier);
				}
			}
		}
		return Best;
	}

	double DamagePerHit(const FGLSalvageDef& Salvage, const FGLMaterialDef* Material, const FGLItemDef* Tool)
	{
		const double Hardness = Material && Material->SalvageHardness > 0.0 ? Material->SalvageHardness : 1.0;
		return BaseHitDamage * ToolMultiplier(Salvage, Tool) / Hardness;
	}

	int32 HitsToSalvage(const FGLSalvageDef& Salvage, const FGLMaterialDef* Material, const FGLItemDef* Tool)
	{
		const double Damage = DamagePerHit(Salvage, Material, Tool);
		return Damage > 0.0 ? FMath::CeilToInt(Salvage.Integrity / Damage - UE_KINDA_SMALL_NUMBER) : MAX_int32;
	}
}
