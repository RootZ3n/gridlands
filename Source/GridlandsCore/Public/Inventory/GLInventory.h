#pragma once

#include "CoreMinimal.h"

class FGLContentRegistry;

/** One inventory stack. A stack never exceeds its item's stackSize. */
struct GRIDLANDSCORE_API FGLInventoryStack
{
	FName Item;
	int32 Count = 0;

	bool operator==(const FGLInventoryStack& Other) const { return Item == Other.Item && Count == Other.Count; }
};

/**
 * Pure inventory rules: stacking, slot capacity and carried weight. Item stack sizes and
 * weights come from content definitions, so the registry is passed in.
 *
 * Carrying past MaxWeight is allowed (the player is overencumbered: slowed, and NICE notices);
 * running out of slots is not.
 */
class GRIDLANDSCORE_API FGLInventory
{
public:
	explicit FGLInventory(int32 InMaxSlots = 32, double InMaxWeight = 150.0) : MaxSlots(InMaxSlots), MaxWeight(InMaxWeight) {}

	/** Adds up to Count of Item; returns how many were added (0 if the item is unknown). */
	int32 Add(const FGLContentRegistry& Content, FName Item, int32 Count);
	/** Removes exactly Count of Item, or nothing if there are fewer. */
	bool Remove(FName Item, int32 Count);
	/** How many of Item would fit right now. */
	int32 SpaceFor(const FGLContentRegistry& Content, FName Item) const;

	int32 CountOf(FName Item) const;
	double TotalWeight(const FGLContentRegistry& Content) const;
	bool IsOverencumbered(const FGLContentRegistry& Content) const { return TotalWeight(Content) > MaxWeight; }

	const TArray<FGLInventoryStack>& GetStacks() const { return Stacks; }
	int32 GetMaxSlots() const { return MaxSlots; }
	double GetMaxWeight() const { return MaxWeight; }

private:
	TArray<FGLInventoryStack> Stacks;
	int32 MaxSlots;
	double MaxWeight;
};
