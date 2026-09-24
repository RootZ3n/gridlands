#include "Interaction/GLInteractorComponent.h"

#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Actor.h"
#include "GameplayTagsManager.h"

UGLInteractorComponent::UGLInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGLInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (const AActor* Owner = GetOwner())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Owner->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		UpdateFocus(ViewLocation, ViewRotation.Vector());
	}
}

UObject* UGLInteractorComponent::FindInteractable(AActor* HitActor)
{
	if (!HitActor)
	{
		return nullptr;
	}
	if (HitActor->Implements<UGLInteractable>())
	{
		return HitActor;
	}
	for (UActorComponent* Component : HitActor->GetComponents())
	{
		if (Component && Component->Implements<UGLInteractable>())
		{
			return Component;
		}
	}
	return nullptr;
}

UObject* UGLInteractorComponent::UpdateFocus(const FVector& ViewLocation, const FVector& ViewDirection)
{
	Focus = nullptr;
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GLInteractorTrace), false, GetOwner());
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewDirection.GetSafeNormal() * Reach, ECC_Visibility, Params))
	{
		Focus = FindInteractable(Hit.GetActor());
	}
	return Focus.Get();
}

void UGLInteractorComponent::GetFocusOptions(TArray<FGLInteractionOption>& OutOptions) const
{
	OutOptions.Reset();
	if (const IGLInteractable* Interactable = Cast<IGLInteractable>(Focus.Get()))
	{
		Interactable->GetInteractionOptions(GetOwner(), OutOptions);
	}
}

bool UGLInteractorComponent::TryInteract(FGameplayTag Verb)
{
	IGLInteractable* Interactable = Cast<IGLInteractable>(Focus.Get());
	if (!Interactable)
	{
		return false;
	}
	TArray<FGLInteractionOption> Options;
	Interactable->GetInteractionOptions(GetOwner(), Options);
	const FGLInteractionOption* Chosen = Options.FindByPredicate([Verb](const FGLInteractionOption& Option)
	{
		return Option.bEnabled && (!Verb.IsValid() || Option.Verb == Verb);
	});
	if (!Chosen || !Interactable->Interact(GetOwner(), Chosen->Verb))
	{
		return false;
	}
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Interaction.Performed"));
	Event.Subject = FName(*Chosen->Verb.ToString());
	Event.Instigator = GetOwner();
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
	return true;
}
