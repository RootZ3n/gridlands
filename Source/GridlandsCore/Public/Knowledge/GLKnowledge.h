#pragma once

#include "CoreMinimal.h"

/**
 * What this world's player has learned (world-save-bound, ADR-0019). Recipes and build pieces
 * are available when every id in their unlockedBy list is known (AND semantics); an empty list
 * means available from the start.
 */
class GRIDLANDSCORE_API FGLKnowledge
{
public:
	/** Returns true if Id was newly learned. */
	bool Learn(FName Id) { bool bExisted = false; Known.Add(Id, &bExisted); return !bExisted; }
	bool Knows(FName Id) const { return Known.Contains(Id); }
	/** True if every id in UnlockedBy is known; OutMissing receives the ones that are not. */
	bool KnowsAll(const TArray<FName>& UnlockedBy, TArray<FName>* OutMissing = nullptr) const;
	const TSet<FName>& GetKnown() const { return Known; }

private:
	TSet<FName> Known;
};
