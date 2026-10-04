#include "Inventory/GLMaterialPool.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Inventory/GLInventory.h"

namespace
{
	TMap<FName, int32> Totals(TConstArrayView<FGLItemStackDef> Cost)
	{
		TMap<FName, int32> Out;
		for (const FGLItemStackDef& Stack : Cost)
		{
			Out.FindOrAdd(Stack.Item) += Stack.Count;
		}
		return Out;
	}

	/** Items in a stable order (lexical), so filling shared slots never depends on map iteration. */
	TArray<FName> SortedKeys(const TMap<FName, int32>& Items)
	{
		TArray<FName> Keys;
		Items.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		return Keys;
	}

	/** Copies of the sources and pointers to them (simulations never touch the real inventories). */
	struct FTrial
	{
		TArray<FGLInventory> Copies;
		TArray<FGLInventory*> Pointers;
		explicit FTrial(TConstArrayView<FGLInventory*> Sources)
		{
			for (const FGLInventory* Source : Sources)
			{
				Copies.Add(Source ? *Source : FGLInventory(0));
			}
			for (FGLInventory& Copy : Copies)
			{
				Pointers.Add(&Copy);
			}
		}
	};
}

FGLMaterialPool::FGLMaterialPool(TArray<FGLInventory*> InSources, TArray<int32> InDeliverOrder)
	: Sources(MoveTemp(InSources)), DeliverOrder(MoveTemp(InDeliverOrder))
{
	if (DeliverOrder.Num() == 0)
	{
		for (int32 I = 0; I < Sources.Num(); ++I)
		{
			DeliverOrder.Add(I);
		}
	}
}

int32 FGLMaterialPool::CountOf(FName Item) const
{
	int32 Total = 0;
	for (const FGLInventory* Source : Sources)
	{
		Total += Source ? Source->CountOf(Item) : 0;
	}
	return Total;
}

bool FGLMaterialPool::PlanConsume(TConstArrayView<FGLItemStackDef> Cost, TArray<TMap<FName, int32>>& OutTaken) const
{
	FTrial Trial(Sources);
	OutTaken.Reset();
	OutTaken.SetNum(Sources.Num());
	return ConsumeInto(Trial.Pointers, Cost, &OutTaken);
}

bool FGLMaterialPool::ConsumeInto(TArrayView<FGLInventory*> In, TConstArrayView<FGLItemStackDef> Cost, TArray<TMap<FName, int32>>* OutTaken)
{
	const TMap<FName, int32> Need = Totals(Cost);
	for (const TPair<FName, int32>& Item : Need)
	{
		int32 Have = 0;
		for (const FGLInventory* Source : In)
		{
			Have += Source ? Source->CountOf(Item.Key) : 0;
		}
		if (Have < Item.Value)
		{
			return false;
		}
	}
	for (const FName& Item : SortedKeys(Need))
	{
		int32 Remaining = Need[Item];
		for (int32 I = 0; I < In.Num() && Remaining > 0; ++I)
		{
			const int32 Here = In[I] ? FMath::Min(Remaining, In[I]->CountOf(Item)) : 0;
			if (Here > 0)
			{
				verify(In[I]->Remove(Item, Here));
				if (OutTaken)
				{
					(*OutTaken)[I].FindOrAdd(Item) += Here;
				}
				Remaining -= Here;
			}
		}
	}
	return true;
}

bool FGLMaterialPool::DeliverInto(const FGLContentRegistry& Content, TArrayView<FGLInventory*> In, TConstArrayView<int32> Order, const TMap<FName, int32>& Items)
{
	for (const FName& Item : SortedKeys(Items))
	{
		int32 Remaining = Items[Item];
		for (const int32 Index : Order)
		{
			if (In.IsValidIndex(Index) && In[Index] && Remaining > 0)
			{
				Remaining -= In[Index]->Add(Content, Item, Remaining);
			}
		}
		if (Remaining > 0)
		{
			return false;
		}
	}
	return true;
}

bool FGLMaterialPool::CanConsume(TConstArrayView<FGLItemStackDef> Cost) const
{
	FTrial Trial(Sources);
	return ConsumeInto(Trial.Pointers, Cost, nullptr);
}

bool FGLMaterialPool::Consume(TConstArrayView<FGLItemStackDef> Cost)
{
	Taken.Reset();
	Taken.SetNum(Sources.Num());
	if (!CanConsume(Cost))
	{
		return false;
	}
	verify(ConsumeInto(Sources, Cost, &Taken));
	return true;
}

bool FGLMaterialPool::CanDeliver(const FGLContentRegistry& Content, const TMap<FName, int32>& Items) const
{
	FTrial Trial(Sources);
	return DeliverInto(Content, Trial.Pointers, DeliverOrder, Items);
}

bool FGLMaterialPool::Deliver(const FGLContentRegistry& Content, const TMap<FName, int32>& Items)
{
	if (!CanDeliver(Content, Items))
	{
		return false;
	}
	verify(DeliverInto(Content, Sources, DeliverOrder, Items));
	return true;
}

bool FGLMaterialPool::CanExchange(const FGLContentRegistry& Content, TConstArrayView<FGLItemStackDef> Cost, const TMap<FName, int32>& Items) const
{
	FTrial Trial(Sources);
	return ConsumeInto(Trial.Pointers, Cost, nullptr) && DeliverInto(Content, Trial.Pointers, DeliverOrder, Items);
}

bool FGLMaterialPool::Exchange(const FGLContentRegistry& Content, TConstArrayView<FGLItemStackDef> Cost, const TMap<FName, int32>& Items)
{
	Taken.Reset();
	Taken.SetNum(Sources.Num());
	if (!CanExchange(Content, Cost, Items))
	{
		return false;
	}
	verify(ConsumeInto(Sources, Cost, &Taken));
	verify(DeliverInto(Content, Sources, DeliverOrder, Items));
	return true;
}
