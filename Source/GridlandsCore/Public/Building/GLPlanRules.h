#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"

/** One entry of a plan: an ordinary building piece relative to the plan's anchor. */
struct GRIDLANDSCORE_API FGLPlanEntry
{
	FName Def;
	/** cm in the anchor's frame (rotated into it). */
	FVector Offset = FVector::ZeroVector;
	/** Yaw steps relative to the anchor's. */
	int32 YawStep = 0;
	TArray<FName> Layers;
};

/**
 * A PLAN (P11 compatibility proof for P12 Ofi/player plans, ADR-0039): a set of ordinary canonical building pieces
 * relative to an anchor. Never a second building representation: instantiating a plan yields ordinary placed pieces,
 * built and checked by the same rules. Integer yaw keeps it exact under any whole-step rotation.
 */
struct GRIDLANDSCORE_API FGLPlan
{
	TArray<FGLPlanEntry> Entries;
};

namespace GLPlanRules
{
	/** Captures Pieces relative to an anchor location and yaw. Order is kept. */
	GRIDLANDSCORE_API FGLPlan Capture(TConstArrayView<FGLPlacedPiece> Pieces, const FVector& AnchorLocation, int32 AnchorYawStep);
	/** Ordinary placed pieces for the plan at an anchor (ids from FirstId up). */
	GRIDLANDSCORE_API TArray<FGLPlacedPiece> Instantiate(const FGLPlan& Plan, const FVector& AnchorLocation, int32 AnchorYawStep,
		int32 FirstId, EGLPieceOrigin Origin);
}
