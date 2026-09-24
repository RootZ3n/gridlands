#include "Interaction/GLDebugInteractable.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagsManager.h"
#include "UObject/ConstructorHelpers.h"

AGLDebugInteractable::AGLDebugInteractable()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void AGLDebugInteractable::GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const
{
	FGLInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Verb = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Use"));
	Option.Label = NSLOCTEXT("Gridlands", "DebugUse", "Use");
	Option.bEnabled = bEnabled;
	if (!bEnabled)
	{
		Option.DisabledReason = NSLOCTEXT("Gridlands", "DebugDisabled", "Disabled");
	}
}

bool AGLDebugInteractable::Interact(AActor* Interactor, FGameplayTag Verb)
{
	if (!bEnabled)
	{
		return false;
	}
	++TimesUsed;
	return true;
}
