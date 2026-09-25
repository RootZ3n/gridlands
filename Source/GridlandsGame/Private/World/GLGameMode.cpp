#include "World/GLGameMode.h"

#include "Character/GLCharacter.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "GridlandsGame.h"
#include "UI/GLHUD.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Controller.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "GameplayTagsManager.h"

AGLGameMode::AGLGameMode()
{
	DefaultPawnClass = AGLCharacter::StaticClass();
	HUDClass = AGLHUD::StaticClass();
}

void AGLGameMode::StartPlay()
{
	Super::StartPlay();
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogGridlands, Log, TEXT("Lighting: %s rotation %s, direction %s"), *It->GetName(), *It->GetActorRotation().ToString(), *It->GetComponent()->GetDirection().ToString());
	}
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
