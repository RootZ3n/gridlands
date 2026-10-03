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
#include "Building/GLBuildingSubsystem.h"
#include "Inventory/GLInventoryComponent.h"
#include "Inventory/GLMaterialPool.h"
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

bool UGLSalvageableComponent::Setup(FName InSalvageId, AActor* InLinkedVisual, EGLSalvagePath InPath)
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
	Path = InPath;
	ExtraYield.Reset();
	return true;
}

TMap<FName, int32> UGLSalvageableComponent::CompletionYield() const
{
	TMap<FName, int32> Items;
	const FGLSalvageDef* Def = GLContent::Get().Find<FGLSalvageDef>(SalvageId);
	if (!Def)
	{
		return Items;
	}
	const UGLWorldSettingsSubsystem* Settings = GetWorld() ? GetWorld()->GetSubsystem<UGLWorldSettingsSubsystem>() : nullptr;
	const FGLSettingsPresetDef* Preset = Settings ? Settings->GetPreset() : nullptr;
	for (const FGLSalvageYieldDef& Yield : GLConstructionRules::YieldsFor(*Def, Path))
	{
		const FGLYieldCategoryDef* Category = GLContent::Get().Find<FGLYieldCategoryDef>(Yield.YieldCategory);
		Items.FindOrAdd(Yield.Item) += Category && Preset ? GLYield::Apply(Yield.Count, *Category, *Preset) : Yield.Count;
	}
	for (const TPair<FName, int32>& Extra : ExtraYield)
	{
		Items.FindOrAdd(Extra.Key) += Extra.Value; // what it held is returned as held (never scaled)
	}
	return Items;
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
	const double Hit = GLSalvageRules::DamagePerHit(*Def, GLContent::Get().Find<FGLMaterialDef>(Def->Material), Tool);
	if (Integrity - Hit <= UE_KINDA_SMALL_NUMBER)
	{
		// P11: the finishing hit is refused, not paid partly, when the whole result would not fit (nothing is destroyed).
		const UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
		FGLMaterialSources Sources = Building ? Building->SourcesFor(Interactor, GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector) : FGLMaterialSources();
		if (!Sources.Pool().CanDeliver(GLContent::Get(), CompletionYield()))
		{
			if (UGLInventoryComponent* Carrier = Interactor ? Interactor->FindComponentByClass<UGLInventoryComponent>() : nullptr)
			{
				Carrier->AnnounceFull(SalvageId);
			}
			return false;
		}
	}
	Integrity -= Hit;
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

void UGLSalvageableComponent::Retire()
{
	bSalvaged = true; // refuses Interact; nothing is paid and no event fires
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorHiddenInGame(true);
		Owner->SetActorEnableCollision(false);
	}
}

void UGLSalvageableComponent::Complete(AActor* Interactor)
{
	const FGLSalvageDef* Def = GLContent::Get().Find<FGLSalvageDef>(SalvageId);
	bSalvaged = true;
	Integrity = 0.0;

	// All or nothing (proven before the finishing hit): personal inventory first, then base storage inside a claim.
	const UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
	FGLMaterialSources Sources = Building ? Building->SourcesFor(Interactor, GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector) : FGLMaterialSources();
	const TMap<FName, int32> Items = CompletionYield();
	if (!Sources.Pool().Deliver(GLContent::Get(), Items))
	{
		UE_LOG(LogGridlands, Warning, TEXT("Salvage %s: the result no longer fits; nothing delivered"), *SalvageId.ToString());
	}
	Sources.AnnounceAcquired(Interactor, Items);
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
