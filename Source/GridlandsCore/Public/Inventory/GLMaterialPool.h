#pragma once

#include "CoreMinimal.h"

class FGLContentRegistry;
class FGLInventory;
struct FGLItemStackDef;

/**
 * P11 (ADR-0039): the inventories one operation may draw from and deliver to (shared base storage). Consumption takes
 * from the sources in their order (base storage before personal inventory inside a base); delivery fills them in
 * DeliverOrder (personal inventory first, then storage). Every operation is all-or-nothing: it is proven on copies
 * first, so a failure can never partly consume or partly deliver. Pure; the caller builds the order.
 */
class GRIDLANDSCORE_API FGLMaterialPool
{
public:
	/** DeliverOrder: indices into Sources (empty: the same order as consumption). */
	explicit FGLMaterialPool(TArray<FGLInventory*> InSources, TArray<int32> InDeliverOrder = TArray<int32>());

	int32 CountOf(FName Item) const;
	bool CanConsume(TConstArrayView<FGLItemStackDef> Cost) const;
	/**
	 * P12 (ADR-0040): what Consume(Cost) would take from each source (source index -> item -> count), without taking it:
	 * the same consumption run on copies, so a cost display is the commit's own plan. False (and partial) when short.
	 */
	bool PlanConsume(TConstArrayView<FGLItemStackDef> Cost, TArray<TMap<FName, int32>>& OutTaken) const;
	bool Consume(TConstArrayView<FGLItemStackDef> Cost);
	bool CanDeliver(const FGLContentRegistry& Content, const TMap<FName, int32>& Items) const;
	bool Deliver(const FGLContentRegistry& Content, const TMap<FName, int32>& Items);
	/** Consumes Cost, then delivers Items (crafting): both happen, or nothing does. */
	bool CanExchange(const FGLContentRegistry& Content, TConstArrayView<FGLItemStackDef> Cost, const TMap<FName, int32>& Items) const;
	bool Exchange(const FGLContentRegistry& Content, TConstArrayView<FGLItemStackDef> Cost, const TMap<FName, int32>& Items);

	/** What the last Consume took from each source (source index -> item -> count), for tests and evidence. */
	const TArray<TMap<FName, int32>>& LastTaken() const { return Taken; }

private:
	static bool ConsumeInto(TArrayView<FGLInventory*> Sources, TConstArrayView<FGLItemStackDef> Cost, TArray<TMap<FName, int32>>* OutTaken);
	static bool DeliverInto(const FGLContentRegistry& Content, TArrayView<FGLInventory*> Sources, TConstArrayView<int32> Order, const TMap<FName, int32>& Items);

	TArray<FGLInventory*> Sources;
	TArray<int32> DeliverOrder;
	TArray<TMap<FName, int32>> Taken;
};
