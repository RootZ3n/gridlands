#include "World/GLGameMode.h"

#include "Character/GLCharacter.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "GridlandsGame.h"
#include "Save/GLSaveSubsystem.h"
#include "Economy/GLWorldSettingsSubsystem.h"
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
	// Continue the world if it was saved (ADR-0019); -GLNewWorld starts fresh. Load before
	// Event.Game.Started so restored dialogue history keeps one-time lines from replaying.
	if (UGLSaveSubsystem* Saves = GetWorld()->GetSubsystem<UGLSaveSubsystem>())
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("GLNewWorld")) && UGLSaveSubsystem::SlotExists(Saves->AutosaveSlot))
		{
			Saves->LoadFromSlot(Saves->AutosaveSlot);
		}
		else if (FString Preset; FParse::Value(FCommandLine::Get(), TEXT("GLSettings="), Preset))
		{
			// A new world's resource-yield setting (ADR-0016), e.g. -GLSettings=settings.preset.relaxed.
			// It is saved with the world and kept on later loads.
			if (UGLWorldSettingsSubsystem* Settings = GetWorld()->GetSubsystem<UGLWorldSettingsSubsystem>(); !Settings || !Settings->SetPreset(FName(*Preset)))
			{
				UE_LOG(LogGridlands, Warning, TEXT("Unknown settings preset %s; keeping the default"), *Preset);
			}
		}
		Saves->bAutosave = true;
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

void AGLGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGLSaveSubsystem* Saves = GetWorld() ? GetWorld()->GetSubsystem<UGLSaveSubsystem>() : nullptr; Saves && Saves->bAutosave)
	{
		Saves->SaveToSlot(Saves->AutosaveSlot); // quitting keeps the world
	}
	Super::EndPlay(Reason);
}
