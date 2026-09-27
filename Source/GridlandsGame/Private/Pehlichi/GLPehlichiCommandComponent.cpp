#include "Pehlichi/GLPehlichiCommandComponent.h"

#include "Noise/GLNoiseSubsystem.h"

#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Mechanism/GLMechanismSubsystem.h"
#include "Pehlichi/GLOperateComponent.h"
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
	UGLOperateComponent* Operate = Pehlichi->FindComponentByClass<UGLOperateComponent>();
	EGLCommandRejection Result = EGLCommandRejection::None;

	if (Operate && Operate->IsWorking() && Command != TEXT("Command.Pehlichi.Operate") && Command != TEXT("Command.Pehlichi.Scan"))
	{
		Operate->Stop(); // any other order interrupts an operation (it must be commanded again)
	}
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
			// Creatures hear it through the one world-noise model (P6), as a distraction.
			if (UGLNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UGLNoiseSubsystem>())
			{
				FGLNoiseEvent Distraction = Noise->Make(TEXT("Noise.Pehlichi.Lure"), Pehlichi->GetActorLocation(), Pehlichi);
				Distraction.bDistraction = true;
				Distraction.InvestigateSeconds = Seconds;
				Noise->Emit(MoveTemp(Distraction));
			}
		}
	}
	else if (Command == TEXT("Command.Pehlichi.Operate"))
	{
		// P9 (ADR-0037): go and work the nearest operable control. The mechanism decides what happens, when it
		// switches; Pehlichi deals no damage (ADR-0017).
		const UGLMechanismSubsystem* Mechanisms = GetWorld()->GetSubsystem<UGLMechanismSubsystem>();
		const FGLMechanismRecord* Control = Mechanisms ? Mechanisms->FindOperable(Commander->GetActorLocation(), OperateSearchRadius) : nullptr;
		const FGLMechanismDef* Def = Control ? UGLMechanismSubsystem::DefOf(*Control) : nullptr;
		const UGLCapabilityComponent* Capabilities = Pehlichi->FindComponentByClass<UGLCapabilityComponent>();
		if (!Operate || !Def)
		{
			Result = EGLCommandRejection::NothingToOperate;
		}
		else if (Operate->IsWorking())
		{
			Result = EGLCommandRejection::Busy;
		}
		else if (!Capabilities || Capabilities->Level(Def->Operate.Capability) < FMath::Max(1, Def->Operate.MinLevel))
		{
			Result = EGLCommandRejection::RequirementsUnmet;
		}
		else
		{
			Repair->Stop();
			Operate->Assign(Control->Placement, Commander);
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
