#pragma once

#include "CoreMinimal.h"

struct FGLCapabilityDef;
struct FGLGlitchDef;

/** World facts requirement evaluation needs (supplied by the game layer). */
struct GRIDLANDSCORE_API FGLRequirementFacts
{
	/** Has the placement with this id been salvaged? */
	TFunction<bool(FName PlacementId)> IsSalvaged;
	/** How many of an item the commanding player carries. */
	TFunction<int32(FName Item)> CarriedCount;
};

/** Glitch rules beyond the lifecycle table (pure). */
namespace GLGlitchRules
{
	/**
	 * True when every requirement holds. Bindings map requirement names to placements (ADR-0018).
	 * Unmet requirement names go to OutUnmet. Unknown requirement kinds are never met.
	 */
	GRIDLANDSCORE_API bool RequirementsMet(const FGLGlitchDef& Glitch, const TMap<FString, FName>& Bindings, const FGLRequirementFacts& Facts, TArray<FString>* OutUnmet = nullptr);

	/** Can a scan with this capability at ScanLevel, from DistanceCm away, detect the glitch? */
	GRIDLANDSCORE_API bool CanDetect(const FGLGlitchDef& Glitch, const FGLCapabilityDef& ScanCapability, int32 ScanLevel, double DistanceCm);
}
