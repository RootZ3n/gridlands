#pragma once

#include "CoreMinimal.h"

/**
 * Content id grammar (Docs/CONTENT-IDS-AND-TAGS.md section 1). Mirrors Tools/gldata/grammar.py;
 * Gridlands.Core.Content.IdGrammarMatchesCorpus proves both accept exactly the same strings.
 */
namespace GLContentId
{
	/** Returns true if Id is well formed (ID-1); otherwise explains why in OutProblem. */
	GRIDLANDSCORE_API bool IsValid(FStringView Id, FString* OutProblem = nullptr);

	/** The first segment, e.g. "item" for "item.material.copper_wire". */
	GRIDLANDSCORE_API FString KindOf(FStringView Id);
}
