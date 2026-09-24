#pragma once

#include "CoreMinimal.h"

struct FGLItemDef;
struct FGLMaterialDef;
struct FGLSalvageDef;

/** Salvage rules: how hard a hit lands, and whether the player may salvage at all. */
namespace GLSalvageRules
{
	/** Damage of a bare-handed hit before material hardness. */
	constexpr double BaseHitDamage = 10.0;

	/** False (with Reason) if Salvage requires a tool class that Tool does not provide. Tool may be null. */
	GRIDLANDSCORE_API bool CanSalvage(const FGLSalvageDef& Salvage, const FGLItemDef* Tool, FText* OutReason = nullptr);

	/** Multiplier Tool earns on Salvage: the best matching toolEfficiency entry whose minTier the tool meets, else 1. */
	GRIDLANDSCORE_API double ToolMultiplier(const FGLSalvageDef& Salvage, const FGLItemDef* Tool);

	/** Integrity removed per hit: BaseHitDamage * tool multiplier / material hardness (hardness 1 if unknown). */
	GRIDLANDSCORE_API double DamagePerHit(const FGLSalvageDef& Salvage, const FGLMaterialDef* Material, const FGLItemDef* Tool);

	/** Hits needed to salvage a fresh object. */
	GRIDLANDSCORE_API int32 HitsToSalvage(const FGLSalvageDef& Salvage, const FGLMaterialDef* Material, const FGLItemDef* Tool);
}
