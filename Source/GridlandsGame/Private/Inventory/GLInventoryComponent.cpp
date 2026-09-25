#include "Inventory/GLInventoryComponent.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Knowledge/GLKnowledge.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagsManager.h"
#include "Salvage/GLSalvageRules.h"

int32 UGLInventoryComponent::AddItem(FName Item, int32 Count)
{
	const int32 Added = Inventory.Add(GLContent::Get(), Item, Count);
	if (Added > 0)
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Item.Acquired"));
		Event.Subject = Item;
		Event.Instigator = GetOwner();
		Event.Numbers.Add(TEXT("count"), Added);
		UGLEventSubsystem::Emit(this, MoveTemp(Event));
		UpdateEncumbrance();
	}
	return Added;
}

FGLCraftCheck UGLInventoryComponent::Craft(FName RecipeId, const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach)
{
	const FGLCraftCheck Result = GLFabricationRules::Craft(GLContent::Get(), RecipeId, Inventory, Knowledge, StationsInReach);
	if (Result.CanCraft())
	{
		const FGLRecipeDef* Recipe = GLContent::Get().Find<FGLRecipeDef>(RecipeId);
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Item.Acquired"));
		Event.Subject = Recipe->Output.Item;
		Event.Instigator = GetOwner();
		Event.Numbers.Add(TEXT("count"), Recipe->Output.Count);
		UGLEventSubsystem::Emit(this, MoveTemp(Event));
		UpdateEncumbrance();
	}
	return Result;
}

void UGLInventoryComponent::RestoreContents(const TArray<TPair<FName, int32>>& Items)
{
	Inventory = FGLInventory(Inventory.GetMaxSlots(), Inventory.GetMaxWeight());
	for (const TPair<FName, int32>& Item : Items)
	{
		Inventory.Add(GLContent::Get(), Item.Key, Item.Value);
	}
	UpdateEncumbrance(/*bAnnounce*/ false); // a load is not a new moment to announce
}

bool UGLInventoryComponent::RemoveItem(FName Item, int32 Count)
{
	const bool bRemoved = Inventory.Remove(Item, Count);
	if (bRemoved)
	{
		UpdateEncumbrance();
	}
	return bRemoved;
}

const FGLItemDef* UGLInventoryComponent::BestToolFor(const FGLSalvageDef& Salvage) const
{
	const FGLItemDef* Best = nullptr;
	double BestMultiplier = 0.0;
	for (const FGLInventoryStack& Stack : Inventory.GetStacks())
	{
		const FGLItemDef* Item = GLContent::Get().Find<FGLItemDef>(Stack.Item);
		if (Item && Item->IsTool() && GLSalvageRules::CanSalvage(Salvage, Item))
		{
			const double Multiplier = GLSalvageRules::ToolMultiplier(Salvage, Item);
			if (!Best || Multiplier > BestMultiplier)
			{
				Best = Item;
				BestMultiplier = Multiplier;
			}
		}
	}
	return Best;
}

void UGLInventoryComponent::UpdateEncumbrance(bool bAnnounce)
{
	const bool bNow = Inventory.IsOverencumbered(GLContent::Get());
	if (bNow == bOverencumbered)
	{
		return;
	}
	bOverencumbered = bNow;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (NormalWalkSpeed < 0.f)
		{
			NormalWalkSpeed = Movement->MaxWalkSpeed;
		}
		Movement->MaxWalkSpeed = bOverencumbered ? NormalWalkSpeed * OverencumberedSpeedFactor : NormalWalkSpeed;
	}
	if (bOverencumbered && bAnnounce)
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Player.Overencumbered"));
		Event.Instigator = GetOwner();
		Event.Numbers.Add(TEXT("weight"), Inventory.TotalWeight(GLContent::Get()));
		UGLEventSubsystem::Emit(this, MoveTemp(Event));
	}
}
