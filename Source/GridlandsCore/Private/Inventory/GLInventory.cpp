#include "Inventory/GLInventory.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"

int32 FGLInventory::SpaceFor(const FGLContentRegistry& Content, FName Item) const
{
	const FGLItemDef* Def = Content.Find<FGLItemDef>(Item);
	if (!Def || Def->StackSize <= 0)
	{
		return 0;
	}
	int32 Space = 0;
	for (const FGLInventoryStack& Stack : Stacks)
	{
		if (Stack.Item == Item)
		{
			Space += Def->StackSize - Stack.Count;
		}
	}
	return Space + (MaxSlots - Stacks.Num()) * Def->StackSize;
}

int32 FGLInventory::Add(const FGLContentRegistry& Content, FName Item, int32 Count)
{
	const FGLItemDef* Def = Content.Find<FGLItemDef>(Item);
	if (!Def || Count <= 0)
	{
		return 0;
	}
	int32 Remaining = Count;
	for (FGLInventoryStack& Stack : Stacks)
	{
		if (Stack.Item == Item && Stack.Count < Def->StackSize)
		{
			const int32 Moved = FMath::Min(Remaining, Def->StackSize - Stack.Count);
			Stack.Count += Moved;
			Remaining -= Moved;
		}
	}
	while (Remaining > 0 && Stacks.Num() < MaxSlots)
	{
		const int32 Moved = FMath::Min(Remaining, Def->StackSize);
		Stacks.Add({ Item, Moved });
		Remaining -= Moved;
	}
	return Count - Remaining;
}

bool FGLInventory::Remove(FName Item, int32 Count)
{
	if (Count <= 0 || CountOf(Item) < Count)
	{
		return false;
	}
	int32 Remaining = Count;
	for (int32 Index = Stacks.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		if (Stacks[Index].Item == Item)
		{
			const int32 Taken = FMath::Min(Remaining, Stacks[Index].Count);
			Stacks[Index].Count -= Taken;
			Remaining -= Taken;
			if (Stacks[Index].Count == 0)
			{
				Stacks.RemoveAt(Index);
			}
		}
	}
	return true;
}

int32 FGLInventory::CountOf(FName Item) const
{
	int32 Total = 0;
	for (const FGLInventoryStack& Stack : Stacks)
	{
		Total += Stack.Item == Item ? Stack.Count : 0;
	}
	return Total;
}

double FGLInventory::TotalWeight(const FGLContentRegistry& Content) const
{
	double Weight = 0.0;
	for (const FGLInventoryStack& Stack : Stacks)
	{
		if (const FGLItemDef* Def = Content.Find<FGLItemDef>(Stack.Item))
		{
			Weight += Def->Weight * Stack.Count;
		}
	}
	return Weight;
}
