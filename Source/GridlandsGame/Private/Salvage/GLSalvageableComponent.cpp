#include "Salvage/GLSalvageableComponent.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Economy/GLWorldSettingsSubsystem.h"
#include "Economy/GLYield.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Actor.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Salvage/GLSalvageRules.h"

namespace
{
	const FGameplayTag& SalvageVerb()
	{
		static const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Salvage"));
		return Tag;
	}

	void EmitEvent(const UObject* Context, FName TagName, FName Subject, AActor* Instigator)
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TagName);
		Event.Subject = Subject;
		Event.Instigator = Instigator;
		UGLEventSubsystem::Emit(Context, MoveTemp(Event));
	}
}

bool UGLSalvageableComponent::Setup(FName InSalvageId, AActor* InLinkedVisual)
{
	const FGLSalvageDef* Def = GLContent::Get().Find<FGLSalvageDef>(InSalvageId);
	if (!Def)
	{
		return false;
	}
	SalvageId = InSalvageId;
	Integrity = Def->Integrity;
	bSalvaged = false;
	LinkedVisual = InLinkedVisual;
	return true;
}

void UGLSalvageableComponent::GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const
{
	const FGLSalvageDef* Def = GLContent::Get().Find<FGLSalvageDef>(SalvageId);
	if (!Def || bSalvaged)
	{
		return;
	}
	const UGLInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	FGLInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Verb = SalvageVerb();
	Option.Label = FText::Format(NSLOCTEXT("Gridlands", "SalvageLabel", "Salvage {0}"), FText::FromString(Def->DisplayName));
	Option.bEnabled = GLSalvageRules::CanSalvage(*Def, Inventory ? Inventory->BestToolFor(*Def) : nullptr, &Option.DisabledReason);
}

bool UGLSalvageableComponent::Interact(AActor* Interactor, FGameplayTag Verb)
{
	const FGLSalvageDef* Def = GLContent::Get().Find<FGLSalvageDef>(SalvageId);
	if (!Def || bSalvaged || Verb != SalvageVerb())
	{
		return false;
	}
	const UGLInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	const FGLItemDef* Tool = Inventory ? Inventory->BestToolFor(*Def) : nullptr;
	if (!GLSalvageRules::CanSalvage(*Def, Tool))
	{
		return false;
	}
	Integrity -= GLSalvageRules::DamagePerHit(*Def, GLContent::Get().Find<FGLMaterialDef>(Def->Material), Tool);
	// Every hit is heard (P6): salvage is never silent. Chopping a tree says so in its data.
	UGLNoiseSubsystem::EmitAction(this, Def->Noise.IsNone() ? FName(TEXT("Noise.Salvage.Hit")) : Def->Noise,
		GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector, Interactor, Def->Material);
	if (Integrity <= UE_KINDA_SMALL_NUMBER)
	{
		Complete(Interactor);
	}
	return true;
}

void UGLSalvageableComponent::RestoreSalvaged()
{
	bSalvaged = true;
	Integrity = 0.0;
	for (AActor* Actor : { GetOwner(), LinkedVisual.Get() })
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}
}

void UGLSalvageableComponent::Complete(AActor* Interactor)
{
	const FGLSalvageDef* Def = GLContent::Get().Find<FGLSalvageDef>(SalvageId);
	bSalvaged = true;
	Integrity = 0.0;

	UGLInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	const UGLWorldSettingsSubsystem* Settings = GetWorld() ? GetWorld()->GetSubsystem<UGLWorldSettingsSubsystem>() : nullptr;
	const FGLSettingsPresetDef* Preset = Settings ? Settings->GetPreset() : nullptr;
	for (const FGLSalvageYieldDef& Yield : Def->Yields)
	{
		const FGLYieldCategoryDef* Category = GLContent::Get().Find<FGLYieldCategoryDef>(Yield.YieldCategory);
		const int32 Count = Category && Preset ? GLYield::Apply(Yield.Count, *Category, *Preset) : Yield.Count;
		const int32 Added = Inventory ? Inventory->AddItem(Yield.Item, Count) : 0;
		if (Added < Count)
		{
			UE_LOG(LogGridlands, Warning, TEXT("Salvage %s: %d of %d %s did not fit"), *SalvageId.ToString(), Count - Added, Count, *Yield.Item.ToString());
		}
	}
	// Knowledge unlocks (onSalvageUnlocks) are granted by the knowledge system in M6.

	EmitEvent(this, TEXT("Event.Salvage.Completed"), SalvageId, Interactor);
	for (const FName& Tag : Def->OnSalvageEvents)
	{
		EmitEvent(this, Tag, SalvageId, Interactor);
	}

	for (AActor* Actor : { GetOwner(), LinkedVisual.Get() })
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}
	OnSalvaged.Broadcast(Interactor);
}
