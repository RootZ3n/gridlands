#include "World/GLTravelPoint.h"

#include "Character/GLCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Controller.h"
#include "GameplayTagsManager.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "UObject/ConstructorHelpers.h"

AGLTravelPoint::AGLTravelPoint()
{
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	SetRootComponent(Marker);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Marker->SetStaticMesh(Cylinder.Object);
	}
	Marker->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.1f)); // a manhole-sized disc
	Marker->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Marker->SetCanEverAffectNavigation(false);
}

void AGLTravelPoint::GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const
{
	FGLInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Verb = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Travel"));
	Option.Label = Label;
}

bool AGLTravelPoint::Interact(AActor* Interactor, FGameplayTag Verb)
{
	if (!Interactor)
	{
		return false;
	}
	Interactor->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	if (const APawn* Pawn = Cast<APawn>(Interactor); Pawn && Pawn->GetController())
	{
		Pawn->GetController()->SetControlRotation(FRotator(-10.0, DestinationYaw, 0.0));
	}
	if (const AGLCharacter* Zenny = Cast<AGLCharacter>(Interactor))
	{
		if (AGLPehlichi* Pehlichi = Zenny->GetPehlichi())
		{
			Pehlichi->SetActorLocation(Destination + FVector(-120.0, 80.0, 40.0), false, nullptr, ETeleportType::TeleportPhysics);
			Pehlichi->GetPositioning()->Follow(Interactor);
		}
	}
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Area.Entered"));
	Event.Subject = AreaName;
	Event.Instigator = Interactor;
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
	return true;
}
