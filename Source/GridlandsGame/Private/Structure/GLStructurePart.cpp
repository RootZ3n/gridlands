#include "Structure/GLStructurePart.h"

#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"
#include "Inventory/GLInventoryComponent.h"

#include "Salvage/GLSalvageableComponent.h"

AGLStructurePart::AGLStructurePart()
{
	Salvageable = CreateDefaultSubobject<UGLSalvageableComponent>(TEXT("Salvageable"));
}

void AGLStructurePart::GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const
{
	const UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
	const int32 Id = Building ? Building->PieceIdOf(this) : 0;
	if (const FGLInventory* Crate = Id ? Building->StorageOf(Id) : nullptr)
	{
		const UGLInventoryComponent* Carrier = Interactor ? Interactor->FindComponentByClass<UGLInventoryComponent>() : nullptr;
		bool bCarries = false;
		for (const FGLInventoryStack& Stack : Carrier ? Carrier->GetInventory().GetStacks() : TArray<FGLInventoryStack>())
		{
			const FGLItemDef* Def = GLContent::Get().Find<FGLItemDef>(Stack.Item);
			bCarries |= Def && !Def->IsTool() && Def->Weapon.Damage <= 0.0;
		}
		FGLInteractionOption& Store = OutOptions.AddDefaulted_GetRef();
		Store.Verb = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Store"));
		Store.Label = NSLOCTEXT("Gridlands", "StoreLabel", "Store materials");
		Store.bEnabled = bCarries;
		Store.DisabledReason = NSLOCTEXT("Gridlands", "StoreNothing", "Nothing to store");
		FGLInteractionOption& Take = OutOptions.AddDefaulted_GetRef();
		Take.Verb = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Take"));
		Take.Label = NSLOCTEXT("Gridlands", "TakeLabel", "Take everything");
		Take.bEnabled = !Crate->IsEmpty();
		Take.DisabledReason = NSLOCTEXT("Gridlands", "TakeNothing", "It is empty");
	}
	if (Salvageable)
	{
		Salvageable->GetInteractionOptions(Interactor, OutOptions);
	}
}

bool AGLStructurePart::Interact(AActor* Interactor, FGameplayTag Verb)
{
	UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
	const int32 Id = Building ? Building->PieceIdOf(this) : 0;
	if (Id && Verb == UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Store")))
	{
		return Building->Store(Interactor, Id) > 0;
	}
	if (Id && Verb == UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Take")))
	{
		return Building->Take(Interactor, Id) > 0;
	}
	return Salvageable && Salvageable->Interact(Interactor, Verb);
}
