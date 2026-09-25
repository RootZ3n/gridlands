#pragma once

#include "CoreMinimal.h"
#include "GLStabilityModel.generated.h"

/** Interference tiers (WORLD-AND-PROGRESSION section 4). */
UENUM(BlueprintType)
enum class EGLInterferenceTier : uint8
{
	Clear,
	Hazy,
	Static,
	Blizzard,
};

/** One glitch as the model sees it: where, how much it matters, and whether it is repaired. */
struct GRIDLANDSCORE_API FGLStabilitySample
{
	FVector2D Location = FVector2D::ZeroVector;
	double Weight = 1.0;
	double InfluenceRadiusCm = 4000.0;
	bool bRepaired = false;
};

/**
 * The derived world-stability model (ADR-0013, invariant S-1). Pure: the same glitch states give
 * the same numbers, and nothing here is ever saved. Tuning constants are documented in the .cpp.
 */
namespace GLStabilityModel
{
	/** Smooth falloff: 1 at the glitch, 0 at its influence radius. */
	GRIDLANDSCORE_API double Falloff(double DistanceCm, double RadiusCm);

	/**
	 * Interference at a point, 0..1:
	 *   baseline + CorruptionScale * sum(unrepaired weight * falloff) - ReliefScale * sum(repaired weight * falloff)
	 */
	GRIDLANDSCORE_API double InterferenceAt(const FVector2D& Point, double Baseline, TConstArrayView<FGLStabilitySample> Samples);

	GRIDLANDSCORE_API EGLInterferenceTier TierFor(double Interference);

	/** NICE's composure, 0..1: the unrepaired share of total glitch weight (1 when nothing is repaired or nothing exists). */
	GRIDLANDSCORE_API double NiceComposure(TConstArrayView<FGLStabilitySample> Samples);

	GRIDLANDSCORE_API const TCHAR* TierName(EGLInterferenceTier Tier);
}
