#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Glitch/GLGlitchLifecycle.h"
#include "GLGlitchComponent.generated.h"

class UGLGlitchSubsystem;
class UGLRepairComponent;
class UGLScanComponent;

// Authority passkeys (ADR-0005). Only the named class can construct each one, so only Pehlichi's
// scan and repair systems (and the world's requirement evaluation) can change a glitch's state.
// There is deliberately no player key: the player cannot write code that repairs a glitch.
struct FGLScanAuthority
{
private:
	FGLScanAuthority() = default;
	friend class UGLScanComponent;
};

struct FGLRepairAuthority
{
private:
	FGLRepairAuthority() = default;
	friend class UGLRepairComponent;
};

struct FGLWorldAuthority
{
private:
	FGLWorldAuthority() = default;
	friend class UGLGlitchSubsystem;
};

// Loading a save is not a gameplay transition: it restores a persisted fact. Only the save
// system holds this key, and it cannot restore Repairing (never persisted).
struct FGLRestoreAuthority
{
private:
	FGLRestoreAuthority() = default;
	friend class UGLSaveSubsystem;
};

// Presenting a glitch (P8) is not a gameplay transition either: its actor is made from the glitch
// subsystem's authoritative record, already in the recorded state. Only that subsystem holds this key.
struct FGLPresentAuthority
{
private:
	FGLPresentAuthority() = default;
	friend class UGLGlitchSubsystem;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FGLOnGlitchStateChanged, EGLGlitchState /*From*/, EGLGlitchState /*To*/);

/**
 * A glitch in the world. It does NOT implement IGLInteractable: the player never repairs a glitch.
 * Every state change goes through FGLGlitchLifecycle::IsTransitionAllowed with the caller's authority.
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLGlitchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Initializes from a glitch definition and its placement's bindings. Returns false if the id is not a glitch. */
	bool Setup(FName InGlitchId, FName InPlacementId, const TMap<FString, FName>& InBindings);

	// Transitions, each requiring its authority.
	bool Reveal(const FGLScanAuthority& Authority);
	bool SetRequirementsMet(bool bMet, const FGLWorldAuthority& Authority);
	bool BeginRepair(const FGLRepairAuthority& Authority);
	/** Adds repair time; returns true when the repair completes (state becomes Repaired). */
	bool AddRepairProgress(double Seconds, const FGLRepairAuthority& Authority);
	bool Interrupt(const FGLRepairAuthority& Authority);
	bool Interrupt(const FGLWorldAuthority& Authority);
	/** Pehlichi took the ItemDelivered items at repair start; they stay delivered through interruptions. */
	void MarkItemsDelivered(const FGLRepairAuthority& Authority) { bItemsDelivered = true; NotifyModel(); }
	bool AreItemsDelivered() const { return bItemsDelivered; }
	/** Restores saved facts without emitting gameplay events. Refuses Repairing. */
	bool RestoreFromSave(EGLGlitchState SavedState, double SavedProgress, bool bSavedItemsDelivered, const FGLRestoreAuthority& Authority);
	/** P8: takes its authoritative record's state as it is being presented (no events; visuals follow). */
	void PresentFromModel(EGLGlitchState ModelState, double ModelProgress, bool bModelItemsDelivered, const FGLPresentAuthority& Authority);

	EGLGlitchState GetState() const { return State; }
	FName GetGlitchId() const { return GlitchId; }
	FName GetPlacementId() const { return PlacementId; }
	const TMap<FString, FName>& GetBindings() const { return Bindings; }
	double GetProgressSeconds() const { return ProgressSeconds; }
	double GetRequiredSeconds() const;

	FGLOnGlitchStateChanged OnStateChanged;

private:
	bool Transition(EGLGlitchState To, EGLGlitchAuthority By);
	/** P8: the glitch subsystem's record follows every change made here (state, progress, delivery). */
	void NotifyModel() const;
	/** After an interruption: keep or reset progress per the definition's interruptPolicy. Always true. */
	bool ApplyInterruptPolicy();

	UPROPERTY(VisibleAnywhere) FName GlitchId;
	UPROPERTY(VisibleAnywhere) FName PlacementId;
	UPROPERTY(VisibleAnywhere) EGLGlitchState State = EGLGlitchState::Latent;
	UPROPERTY(VisibleAnywhere) double ProgressSeconds = 0.0;
	UPROPERTY(VisibleAnywhere) bool bItemsDelivered = false;
	TMap<FString, FName> Bindings;
};
