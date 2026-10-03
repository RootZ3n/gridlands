#pragma once

#include "CoreMinimal.h"

class FGLContentRegistry;

/** One inventory stack. A stack never exceeds its item's stackSize (except a restored over-capacity stack, see ForceAdd). */
struct GRIDLANDSCORE_API FGLInventoryStack
{
	FName Item;
	int32 Count = 0;

	bool operator==(const FGLInventoryStack& Other) const { return Item == Other.Item && Count == Other.Count; }
};

/**
 * Pure inventory rules: stacking and slot capacity. Item stack sizes come from content definitions, so the registry is
 * passed in. P11 (operator, 2026-10-03): there is no carried weight; slots and per-item stack limits are the limit.
 * Used for Zenny's personal inventory and for a storage piece's contents alike.
 */
class GRIDLANDSCORE_API FGLInventory
{
public:
	explicit FGLInventory(int32 InMaxSlots = 32) : MaxSlots(InMaxSlots) {}

	/** Adds up to Count of Item; returns how many were added (0 if the item is unknown). Never exceeds slots or stacks. */
	int32 Add(const FGLContentRegistry& Content, FName Item, int32 Count);
	/**
	 * Restores Count of Item regardless of capacity (a save is never truncated): fills stacks and free slots first, and
	 * any remainder goes into extra stacks beyond MaxSlots. While over capacity, Add refuses new stacks. Returns Count.
	 */
	int32 ForceAdd(const FGLContentRegistry& Content, FName Item, int32 Count);
	/** Removes exactly Count of Item, or nothing if there are fewer. */
	bool Remove(FName Item, int32 Count);
	/** How many of Item would fit right now. */
	int32 SpaceFor(const FGLContentRegistry& Content, FName Item) const;

	int32 CountOf(FName Item) const;
	bool IsEmpty() const { return Stacks.Num() == 0; }
	/** More stacks than slots (only after a restore of an over-full save). */
	bool IsOverCapacity() const { return Stacks.Num() > MaxSlots; }

	const TArray<FGLInventoryStack>& GetStacks() const { return Stacks; }
	int32 GetMaxSlots() const { return MaxSlots; }

private:
	TArray<FGLInventoryStack> Stacks;
	int32 MaxSlots;
};
