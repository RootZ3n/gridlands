#pragma once

#include "Building/GLCollapseRules.h"
#include "CoreMinimal.h"
#include "Save/GLWorldSave.h"

/**
 * P10 (ADR-0038): a collapse in flight as a durable fact. Capture keeps exactly what the decision fixed; Reconstruct
 * rebuilds the same outcome from it without re-planning (the world may have changed since the support failed). A
 * topple's angle samples are re-integrated from their three inputs (GLCollapseRules::IntegrateTopple).
 */
namespace GLPendingCollapse
{
	GRIDLANDSCORE_API FGLSavedCollapse Capture(FName Placement, FName Part, const FGLCollapseOutcome& Outcome, double ElapsedSeconds,
		FName Material, FName Cause, FName Credit);

	/**
	 * The outcome as it was decided (PieceId and Def are the live part's, set by the caller). False, with a reason, if the
	 * record cannot be the plan it claims to be (a topple whose re-integrated duration disagrees with its impact time).
	 */
	GRIDLANDSCORE_API bool Reconstruct(const FGLSavedCollapse& Saved, FGLCollapseOutcome& Out, FString* OutProblem = nullptr);

	/** Bit-for-bit equality of everything gameplay and presentation read from an outcome (PieceId and Def excluded). */
	GRIDLANDSCORE_API bool Identical(const FGLCollapseOutcome& A, const FGLCollapseOutcome& B, FString* OutFirstDifference = nullptr);
}
