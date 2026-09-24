#include "World/GLGameMode.h"

#include "Character/GLCharacter.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Controller.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "GameplayTagsManager.h"

AGLGameMode::AGLGameMode()
{
	DefaultPawnClass = AGLCharacter::StaticClass();
}

void AGLGameMode::StartPlay()
{
	Super::StartPlay();
	FGLGameplayEvent Started;
	Started.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Game.Started"));
	UGLEventSubsystem::Emit(this, MoveTemp(Started));
}

void AGLGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	APawn* Zenny = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	AGLCharacter* Character = Cast<AGLCharacter>(Zenny);
	if (!Character || Character->GetPehlichi())
	{
		return;
	}
	// Pehlichi appears beside Zenny and follows him.
	const FVector Beside = Zenny->GetActorLocation() + Zenny->GetActorRightVector() * 150.0 - FVector(0, 0, 60.0);
	AGLPehlichi* Pehlichi = GetWorld()->SpawnActor<AGLPehlichi>(Beside, Zenny->GetActorRotation());
	if (Pehlichi)
	{
		Pehlichi->GetPositioning()->Follow(Zenny);
		Character->SetPehlichi(Pehlichi);
	}
}
