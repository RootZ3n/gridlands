#include "Pehlichi/GLPehlichiCommandComponent.h"

#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLCapabilityRules.h"

namespace
{
	void Announce(UObject* Context, AActor* Pehlichi, FName Command, EGLCommandRejection Rejection)
	{
		FGLGameplayEvent Event;
		const bool bAccepted = Rejection == EGLCommandRejection::None;
		// Each rejection reason is its own child tag, so banter can react to one reason specifically.
		const FString Tag = bAccepted ? FString(TEXT("Event.Pehlichi.CommandAccepted"))
			: TEXT("Event.Pehlichi.CommandRejected.") + StaticEnum<EGLCommandRejection>()->GetNameStringByValue(static_cast<int64>(Rejection));
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Tag));
		Event.Subject = Command;
		Event.Instigator = Pehlichi;
		UGLEventSubsystem::Emit(Context, MoveTemp(Event));
	}
}

EGLCommandRejection UGLPehlichiCommandComponent::Issue(FName Command, AActor* Commander)
{
	AActor* Pehlichi = GetOwner();
	UGLCompanionPositioningComponent* Positioning = Pehlichi->FindComponentByClass<UGLCompanionPositioningComponent>();
	UGLRepairComponent* Repair = Pehlichi->FindComponentByClass<UGLRepairComponent>();
	EGLCommandRejection Result = EGLCommandRejection::None;

	if (Command == TEXT("Command.Pehlichi.Follow"))
	{
		Repair->Stop(); // an active repair becomes Interrupted
		Positioning->Follow(Commander);
	}
	else if (Command == TEXT("Command.Pehlichi.Stay"))
	{
		Repair->Stop();
		Positioning->Stay();
	}
	else if (Command == TEXT("Command.Pehlichi.Distract"))
	{
		// A glitchy noise where Pehlichi is: nearby creatures go and look (zero damage, ADR-0017).
		const UGLCapabilityComponent* Capabilities = Pehlichi->FindComponentByClass<UGLCapabilityComponent>();
		const FGLCapabilityDef* Distract = GLContent::Get().Find<FGLCapabilityDef>(TEXT("capability.pehlichi.distract"));
		const int32 Level = Capabilities ? Capabilities->Level(TEXT("capability.pehlichi.distract")) : 0;
		const double Seconds = Distract && Level > 0 ? GLCapabilityRules::EffectValue(*Distract, Level, TEXT("distract")) : 0.0;
		const double Now = GetWorld()->GetTimeSeconds();
		if (Seconds <= 0.0)
		{
			Result = EGLCommandRejection::UnknownCommand;
		}
		else if (Now < DistractReadyAt)
		{
			Result = EGLCommandRejection::Busy;
		}
		else
		{
			DistractReadyAt = Now + DistractCooldownSeconds;
			FGLGameplayEvent Lure;
			Lure.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Pehlichi.Lure"));
			Lure.Instigator = Pehlichi;
			Lure.Numbers.Add(TEXT("seconds"), Seconds);
			UGLEventSubsystem::Emit(this, MoveTemp(Lure));
		}
	}
	else if (Command == TEXT("Command.Pehlichi.Scan"))
	{
		Pehlichi->FindComponentByClass<UGLScanComponent>()->Scan();
	}
	else if (Command == TEXT("Command.Pehlichi.Repair"))
	{
		const UGLGlitchSubsystem* Glitches = GetWorld()->GetSubsystem<UGLGlitchSubsystem>();
		AGLGlitch* Best = nullptr;
		Result = EGLCommandRejection::NothingRevealedNearby;
		if (Repair->IsWorking() && Repair->GetTarget()->GetGlitch()->GetState() == EGLGlitchState::Repairing)
		{
			Result = EGLCommandRejection::Busy;
		}
		else
		{
			for (AGLGlitch* Glitch : Glitches->GlitchesNear(Commander->GetActorLocation(), RepairSearchRadius))
			{
				const EGLGlitchState State = Glitch->GetGlitch()->GetState();
				if (State == EGLGlitchState::Repairable || State == EGLGlitchState::Interrupted)
				{
					Best = Glitch;
					Result = EGLCommandRejection::None;
					break;
				}
				if (State == EGLGlitchState::Detected && Result == EGLCommandRejection::NothingRevealedNearby)
				{
					Result = EGLCommandRejection::RequirementsUnmet; // revealed, but the player has work to do first
				}
			}
		}
		if (Best)
		{
			Repair->AssignTarget(Best, Commander);
		}
	}
	else
	{
		Result = EGLCommandRejection::UnknownCommand;
	}
	Announce(this, Pehlichi, Command, Result);
	return Result;
}
