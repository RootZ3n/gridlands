#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"

struct FGLCollapseTuningDef;

enum class EGLCollapseMotion : uint8
{
	Drop,   // falls straight down (slabs, floors, ceilings)
	Topple, // tips over about a base edge (walls, trees)
};

/** Which way a topple goes. A policy, so later factors (cut, slope, lean, impact) can replace it. */
enum class EGLToppleDirection : uint8
{
	AwayFromInstigator, // provisional default (P6)
	PieceForward,       // the piece's +X
	PieceBackward,      // the piece's -X
};

/** A piece that is about to lose support, with how it moves (from its data). */
struct GRIDLANDSCORE_API FGLCollapseRequest
{
	FGLPlacedPiece Piece;
	EGLCollapseMotion Motion = EGLCollapseMotion::Drop;
	EGLToppleDirection Direction = EGLToppleDirection::AwayFromInstigator;
	/** Multiplies impact damage (the material's impactScale). */
	double DamageScale = 1.0;
};

/** An oriented box: a centre, three unit axes and half extents along them (cm). */
struct GRIDLANDSCORE_API FGLImpactVolume
{
	FVector Centre = FVector::ZeroVector;
	FVector Axis[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
	FVector HalfExtent = FVector::ZeroVector;

	/** True when a sphere of RadiusCm at Point touches the box. */
	bool Touches(const FVector& Point, double RadiusCm = 0.0) const;
};

/**
 * The authoritative outcome for one collapsing piece. Everything gameplay needs is decided here, up
 * front: presentation samples Motion() and may add cosmetic effects, but never changes this.
 */
struct GRIDLANDSCORE_API FGLCollapseOutcome
{
	int32 PieceId = 0;
	FName Def;
	EGLCollapseMotion Motion = EGLCollapseMotion::Drop;
	FTransform Start;
	/** Where it comes to rest: the persistent debris transform. */
	FTransform Rest;
	/** Seconds from the collapse decision: motion starts, then it hits. */
	double StartSeconds = 0.0;
	double ImpactSeconds = 0.0;
	/** Where anything with health is hit at ImpactSeconds, and how hard. */
	FGLImpactVolume Impact;
	double Damage = 0.0;
	/** Metres its centre of mass fell (drop: the fall; topple: half its height). */
	double FallMetres = 0.0;
	/** topple: the pivot (a point on the base edge) and the tilt axis. */
	FVector Pivot = FVector::ZeroVector;
	FVector TiltAxis = FVector::ZeroVector;
	/** topple: how far it first comes down to the surface it pivots on (its lost support), cm. */
	double ToppleDropCm = 0.0;
	/** topple: the tilt angle (radians) sampled every ToppleSampleSeconds from StartSeconds. */
	TArray<double> ToppleAngles;
};

/** The result of taking pieces away from a structure. */
struct GRIDLANDSCORE_API FGLCollapsePlan
{
	/** Pieces that lost support, in the order they were settled (lowest first). */
	TArray<FGLCollapseOutcome> Outcomes;
	bool IsEmpty() const { return Outcomes.Num() == 0; }
};

/**
 * Deterministic structural collapse (P6, ADR-0030). Pure: pieces, content, ground and tuning in,
 * a complete plan out. Support is ADR-0024's (GLStructureRules), so authored structures and player
 * building speak the same structural language. Losing a support can cascade: everything whose
 * support flowed through it falls together.
 */
namespace GLCollapseRules
{
	constexpr double ToppleSampleSeconds = 0.01;

	/** Ids of the pieces in Remaining that have no support (support <= 0). */
	GRIDLANDSCORE_API TArray<int32> Unsupported(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Remaining,
		GLStructureRules::FGroundHeight Ground);

	/**
	 * Plans the fall of Collapsing. Standing: the pieces that stay (debris lands on them or on the
	 * ground). Instigator: whoever caused it (direction policies use it).
	 */
	GRIDLANDSCORE_API FGLCollapsePlan Plan(const FGLContentRegistry& Content, TConstArrayView<FGLCollapseRequest> Collapsing,
		TConstArrayView<FGLPlacedPiece> Standing, GLStructureRules::FGroundHeight Ground, const FVector& Instigator,
		const FGLCollapseTuningDef& Tuning);

	/** The authoritative pose at Seconds after the decision (presentation follows it). */
	GRIDLANDSCORE_API FTransform Motion(const FGLCollapseOutcome& Outcome, double Seconds);

	GRIDLANDSCORE_API EGLCollapseMotion MotionFromData(FName Motion);
	GRIDLANDSCORE_API EGLToppleDirection DirectionFromData(FName Direction);
}
