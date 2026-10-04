#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"

struct FGLCostView;

/**
 * P12 (ADR-0040): what the build UI says, generated only from canonical machine-readable results (the placement check, the
 * cost plan, the commit's yield). It never re-derives a rule: WHAT THE UI SAYS == WHY THE COMMIT ACCEPTS OR REFUSES.
 * Structural state is never colour alone: each state has a word and an icon too.
 */
namespace GLBuildText
{
	/** "OK" / "LIMIT" / "NO". */
	GRIDLANDSGAME_API FString StateWord(EGLPreview Preview);
	/** An icon that reads without colour: "[+]" / "[!]" / "[x]". */
	GRIDLANDSGAME_API FString StateIcon(EGLPreview Preview);
	GRIDLANDSGAME_API FLinearColor StateColour(EGLPreview Preview);
	/** Why: from the check's refusal and its structured detail (BlockingName: the blocking piece's name, if known). */
	GRIDLANDSGAME_API FString Explain(const FGLBuildCheck& Check, const FString& BlockingName = FString());
	GRIDLANDSGAME_API FString ItemName(FName Item);
	/** "6 studs: base 40 / you 3" lines, from the commit's own plan. */
	GRIDLANDSGAME_API FString Cost(const FGLCostView& View);
	/** "5 studs, 2 planks". */
	GRIDLANDSGAME_API FString Items(const TMap<FName, int32>& Items);
}
