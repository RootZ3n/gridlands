#pragma once

#include "CoreMinimal.h"
#include "Save/GLWorldSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLMechanismSubsystem.generated.h"

class AGLMechanism;
struct FGLMechanismDef;

/**
 * P9 (ADR-0037): a mechanism's authoritative record. Made with its cell's gameplay layer, before any actor;
 * saves read and restore it; its actor (control panel, cage, fan) is presentation made from it.
 */
struct FGLMechanismRecord
{
	FName Placement;
	FName Definition;
	FName Cell;
	FVector Location = FVector::ZeroVector;
	double Yaw = 0.0;
	FString State;
	int32 Switches = 0;
	/** World seconds of the last live switch (presentation only: a restored state is shown at rest). */
	double SwitchedAt = -1e9;
	TWeakObjectPtr<AGLMechanism> Actor;
};

/**
 * P9 model-first mechanisms (ADR-0037). Deliberately narrow: named states, one operation, and effects applied
 * at the moment a state is entered. A switch is an authoritative decision: a neutralize effect decides then,
 * from where every creature is at that instant, who is contained, and the visible drop only presents it. A
 * restore (streaming, loading) sets the state silently: no effect, event or noise replays.
 */
UCLASS()
class GRIDLANDSGAME_API UGLMechanismSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool AddRecord(FName Placement, FName Definition, FName Cell, const FVector& Location, double Yaw);
	/** Makes the presentation actor from the record as it is now. */
	AGLMechanism* Present(FName Placement);
	/** Unload: the records of Cell go at once; returns their actors for the caller to retire. */
	TArray<AGLMechanism*> RemoveCell(FName Cell);

	const FGLMechanismRecord* FindRecord(FName Placement) const { return Records.Find(Placement); }
	const TMap<FName, FGLMechanismRecord>& GetRecords() const { return Records; }
	static const FGLMechanismDef* DefOf(const FGLMechanismRecord& Record);

	/**
	 * The authoritative switch to To (a live operation). Applies the state's effects now: a neutralize box
	 * decides the outcome of every susceptible creature inside it at this instant (UGLPlacementSubsystem::
	 * TryNeutralize), an enter noise is made, Event.Mechanism.Switched is announced. False if unknown or no change.
	 */
	bool Switch(FName Placement, const FString& To, AActor* Instigator);
	/** A saved state, silently (no effect, event or noise). False if unknown or not one of its states. */
	bool Restore(const FGLSavedMechanism& Saved);
	void CaptureCell(FName Cell, TArray<FGLSavedMechanism>& Out) const;

	/** The nearest record within RadiusCm of Near that can be operated now (its state is the operation's from). */
	const FGLMechanismRecord* FindOperable(const FVector& Near, double RadiusCm) const;
	/** World box of a record's neutralize effect (empty if none). */
	static FBox NeutralizeBox(const FGLMechanismRecord& Record);
	/** Whether a record's ambient sound is sounding at world time Seconds (active state and duty cycle). */
	static bool IsAmbientOn(const FGLMechanismRecord& Record, double Seconds);
	/** P9 masking: the strongest active ambient mask (0..1) at a listener's location, now. */
	double MaskAt(const FVector& Listener) const;

private:
	TMap<FName, FGLMechanismRecord> Records;
};
