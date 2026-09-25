#include "World/GLStabilityModel.h"

namespace GLStabilityModel
{
	// Tuning. Weights and radii are content (glitch data); these shape the curve.
	constexpr double CorruptionScale = 0.5;
	constexpr double ReliefScale = 0.5;
	constexpr double HazyAt = 0.2;
	constexpr double StaticAt = 0.45;
	constexpr double BlizzardAt = 0.7;

	double Falloff(double DistanceCm, double RadiusCm)
	{
		if (RadiusCm <= 0.0)
		{
			return 0.0;
		}
		const double T = FMath::Clamp(1.0 - DistanceCm / RadiusCm, 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	double InterferenceAt(const FVector2D& Point, double Baseline, TConstArrayView<FGLStabilitySample> Samples)
	{
		double Corruption = 0.0, Relief = 0.0;
		for (const FGLStabilitySample& Sample : Samples)
		{
			const double Influence = Sample.Weight * Falloff(FVector2D::Distance(Point, Sample.Location), Sample.InfluenceRadiusCm);
			(Sample.bRepaired ? Relief : Corruption) += Influence;
		}
		return FMath::Clamp(Baseline + CorruptionScale * Corruption - ReliefScale * Relief, 0.0, 1.0);
	}

	EGLInterferenceTier TierFor(double Interference)
	{
		return Interference >= BlizzardAt ? EGLInterferenceTier::Blizzard
			: Interference >= StaticAt ? EGLInterferenceTier::Static
			: Interference >= HazyAt ? EGLInterferenceTier::Hazy
			: EGLInterferenceTier::Clear;
	}

	double NiceComposure(TConstArrayView<FGLStabilitySample> Samples)
	{
		double Total = 0.0, Repaired = 0.0;
		for (const FGLStabilitySample& Sample : Samples)
		{
			Total += Sample.Weight;
			Repaired += Sample.bRepaired ? Sample.Weight : 0.0;
		}
		return Total > 0.0 ? 1.0 - Repaired / Total : 1.0;
	}

	const TCHAR* TierName(EGLInterferenceTier Tier)
	{
		switch (Tier)
		{
		case EGLInterferenceTier::Clear:    return TEXT("Clear");
		case EGLInterferenceTier::Hazy:     return TEXT("Hazy");
		case EGLInterferenceTier::Static:   return TEXT("Static");
		case EGLInterferenceTier::Blizzard: return TEXT("Blizzard");
		default:                            return TEXT("Invalid");
		}
	}
}
