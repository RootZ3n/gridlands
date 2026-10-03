#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"

class FGLContentRegistry;

/** One area of a claim: a circle around the piece that establishes it. */
struct GRIDLANDSCORE_API FGLClaimArea
{
	FVector2D Centre = FVector2D::ZeroVector;
	double RadiusCm = 0.0;
	/** The base core (or, later, an expansion piece) whose fact establishes the area. */
	int32 SourcePiece = 0;
};

/**
 * A recognized player base (P11, ADR-0039). A claim is a set of areas, derived from player-built claim pieces (never a
 * second saved source of truth: the pieces are the facts). P11 BEHAVIOUR: one base core makes one claim of one 32 m
 * area, and claims may not overlap. Not an architectural invariant: an area list leaves room for expansion, connected
 * areas, cooperating cores and estate-scale projects without a new representation.
 */
struct GRIDLANDSCORE_API FGLClaim
{
	FName Id;
	FName Cell;
	TArray<FGLClaimArea> Areas;

	bool Contains(const FVector2D& Point) const;
	bool Overlaps(const FGLClaim& Other) const;
};

namespace GLClaimRules
{
	constexpr TCHAR RoleBaseCore[] = TEXT("base_core");

	/** Claims from the intact player-built base cores among Pieces (P11: one area each, RadiusCm). Sorted by id. */
	GRIDLANDSCORE_API TArray<FGLClaim> ClaimsFrom(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces, double RadiusCm);

	/** The claim containing Point, or null. */
	GRIDLANDSCORE_API const FGLClaim* ClaimAt(TConstArrayView<FGLClaim> Claims, const FVector2D& Point);

	/**
	 * May world renewal (regeneration) delete or replace a fact at Location? NEVER for player-built construction, and
	 * never inside a recognized claim. Every renewal path must ask this (architecture-tested).
	 */
	GRIDLANDSCORE_API bool MayRenew(EGLPieceOrigin Origin, const FVector2D& Location, TConstArrayView<FGLClaim> Claims);
}
