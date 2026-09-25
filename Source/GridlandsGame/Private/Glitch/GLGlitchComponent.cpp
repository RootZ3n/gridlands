#include "Glitch/GLGlitchComponent.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"

namespace
{
	FName EventFor(EGLGlitchState To)
	{
		switch (To)
		{
		case EGLGlitchState::Detected:    return TEXT("Event.Glitch.Detected");
		case EGLGlitchState::Repairable:  return TEXT("Event.Glitch.Repairable");
		case EGLGlitchState::Repairing:   return TEXT("Event.Glitch.RepairStarted");
		case EGLGlitchState::Interrupted: return TEXT("Event.Glitch.RepairInterrupted");
		case EGLGlitchState::Repaired:    return TEXT("Event.Glitch.Repaired");
		default:                          return NAME_None;
		}
	}
}

bool UGLGlitchComponent::Setup(FName InGlitchId, FName InPlacementId, const TMap<FString, FName>& InBindings)
{
	if (!GLContent::Get().Find<FGLGlitchDef>(InGlitchId))
	{
		return false;
	}
	GlitchId = InGlitchId;
	PlacementId = InPlacementId;
	Bindings = InBindings;
	State = EGLGlitchState::Latent;
	ProgressSeconds = 0.0;
	bItemsDelivered = false;
	return true;
}

double UGLGlitchComponent::GetRequiredSeconds() const
{
	const FGLGlitchDef* Def = GLContent::Get().Find<FGLGlitchDef>(GlitchId);
	return Def ? Def->Repair.Seconds : 0.0;
}

bool UGLGlitchComponent::Transition(EGLGlitchState To, EGLGlitchAuthority By)
{
	const EGLGlitchState From = State;
	if (!FGLGlitchLifecycle::IsTransitionAllowed(From, To, By))
	{
		UE_LOG(LogGridlands, Verbose, TEXT("Glitch %s: %s -> %s refused for %s"), *GlitchId.ToString(),
			FGLGlitchLifecycle::StateName(From), FGLGlitchLifecycle::StateName(To), FGLGlitchLifecycle::AuthorityName(By));
		return false;
	}
	State = To;
	OnStateChanged.Broadcast(From, To);
	if (const FName Tag = EventFor(To); !Tag.IsNone())
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
		Event.Subject = GlitchId;
		Event.Instigator = GetOwner();
		UGLEventSubsystem::Emit(this, MoveTemp(Event));
	}
	return true;
}

bool UGLGlitchComponent::Reveal(const FGLScanAuthority&)
{
	return Transition(EGLGlitchState::Detected, EGLGlitchAuthority::PehlichiScan);
}

bool UGLGlitchComponent::SetRequirementsMet(bool bMet, const FGLWorldAuthority&)
{
	if (bMet && State == EGLGlitchState::Detected)
	{
		return Transition(EGLGlitchState::Repairable, EGLGlitchAuthority::World);
	}
	if (!bMet && (State == EGLGlitchState::Repairable || State == EGLGlitchState::Interrupted))
	{
		return Transition(EGLGlitchState::Detected, EGLGlitchAuthority::World);
	}
	return false;
}

bool UGLGlitchComponent::BeginRepair(const FGLRepairAuthority&)
{
	// Legal from Repairable (start) and Interrupted (resume); the lifecycle table decides.
	return Transition(EGLGlitchState::Repairing, EGLGlitchAuthority::PehlichiRepair);
}

bool UGLGlitchComponent::RestoreFromSave(EGLGlitchState SavedState, double SavedProgress, bool bSavedItemsDelivered, const FGLRestoreAuthority&)
{
	if (SavedState == EGLGlitchState::Repairing || SavedState >= EGLGlitchState::Count)
	{
		return false;
	}
	const EGLGlitchState From = State;
	State = SavedState;
	ProgressSeconds = FMath::Clamp(SavedProgress, 0.0, GetRequiredSeconds());
	bItemsDelivered = bSavedItemsDelivered;
	OnStateChanged.Broadcast(From, State); // visuals only; no Event.Glitch.* for a restore
	return true;
}

bool UGLGlitchComponent::ApplyInterruptPolicy()
{
	const FGLGlitchDef* Def = GLContent::Get().Find<FGLGlitchDef>(GlitchId);
	if (Def && Def->Repair.InterruptPolicy == EGLInterruptPolicy::ResetProgress)
	{
		ProgressSeconds = 0.0;
	}
	return true;
}

bool UGLGlitchComponent::AddRepairProgress(double Seconds, const FGLRepairAuthority&)
{
	if (State != EGLGlitchState::Repairing)
	{
		return false;
	}
	ProgressSeconds += Seconds;
	if (ProgressSeconds + UE_KINDA_SMALL_NUMBER >= GetRequiredSeconds())
	{
		ProgressSeconds = GetRequiredSeconds();
		return Transition(EGLGlitchState::Repaired, EGLGlitchAuthority::PehlichiRepair);
	}
	return false;
}

bool UGLGlitchComponent::Interrupt(const FGLRepairAuthority&)
{
	return Transition(EGLGlitchState::Interrupted, EGLGlitchAuthority::PehlichiRepair) && ApplyInterruptPolicy();
}

bool UGLGlitchComponent::Interrupt(const FGLWorldAuthority&)
{
	return Transition(EGLGlitchState::Interrupted, EGLGlitchAuthority::World) && ApplyInterruptPolicy();
}
