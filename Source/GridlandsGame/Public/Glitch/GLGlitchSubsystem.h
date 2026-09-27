#pragma once

#include "CoreMinimal.h"
#include "Glitch/GLGlitchLifecycle.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLGlitchSubsystem.generated.h"

class AGLGlitch;
class UGLGlitchComponent;
struct FGLRestoreAuthority;

/**
 * P8: a glitch placement's authoritative model. It exists from the moment its cell's gameplay layer is
 * made, holds the lifecycle state the save restores and reads, and outlives any actor. The actor (the
 * presentation, and the thing Pehlichi scans and repairs) is made from it, already in its state.
 */
struct FGLGlitchRecord
{
	FName Placement;
	FName Glitch;
	FName Cell;
	FVector Location = FVector::ZeroVector;
	double Yaw = 0.0;
	TMap<FString, FName> Bindings;
	EGLGlitchState State = EGLGlitchState::Latent;
	double ProgressSeconds = 0.0;
	bool bItemsDelivered = false;
	TWeakObjectPtr<AGLGlitch> Actor;
};

/**
 * Registry of the world's glitches, spatial queries for scans, and the World authority: it
 * re-evaluates requirements (Detected <-> Repairable, and interrupting a repair whose
 * requirements are lost) a few times per second.
 */
UCLASS()
class GRIDLANDSGAME_API UGLGlitchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGLGlitchSubsystem, STATGROUP_Tickables); }

	/** P8: the authoritative model of a glitch placement (no actor yet). False if GlitchId is not a glitch or it exists. */
	bool AddRecord(FName Placement, FName GlitchId, FName Cell, const FVector& Location, double Yaw, const TMap<FString, FName>& Bindings);
	/** Makes the record's actor now, in the record's state (no events). Null if there is no record or it already has one. */
	AGLGlitch* Present(FName Placement);
	/** Streaming out: the cell's records go now; their actors are returned, already out of every query, to be retired. */
	TArray<AGLGlitch*> RemoveCell(FName Cell);
	const FGLGlitchRecord* FindRecord(FName Placement) const { return Records.Find(Placement); }
	const TMap<FName, FGLGlitchRecord>& GetRecords() const { return Records; }
	/** A saved state onto the record (and onto its actor, if presented), silently. False if there is no record or the state is not restorable. */
	bool RestoreRecord(FName Placement, EGLGlitchState State, double ProgressSeconds, bool bItemsDelivered, const FGLRestoreAuthority& Authority);
	/** A presented glitch's component changed: its record follows. */
	void SyncFromComponent(const UGLGlitchComponent& Component);

	void Register(AGLGlitch* Glitch);
	/** Forgets glitches that no longer exist (a streamed-out cell's). */
	void Compact() { Glitches.RemoveAll([](const TWeakObjectPtr<AGLGlitch>& G) { return !G.IsValid(); }); }
	/** The presented glitch of a placement (null while it waits to be presented, or once its cell is gone). */
	AGLGlitch* FindByPlacement(FName PlacementId) const;
	/** Presented glitches within RadiusCm of Origin, nearest first (what Pehlichi can scan and repair). */
	TArray<AGLGlitch*> GlitchesNear(const FVector& Origin, double RadiusCm) const;
	/** Presented glitches. The authoritative state of every loaded glitch is GetRecords(). */
	const TArray<TWeakObjectPtr<AGLGlitch>>& GetAll() const { return Glitches; }

	/** Whose inventory ItemDelivered requirements check. Defaults to player 0's pawn. */
	void SetCommander(AActor* Commander) { CommanderOverride = Commander; }
	AActor* GetCommander() const;

	/** Re-evaluates every glitch's requirements now (World authority). */
	void EvaluateRequirements();

private:
	TMap<FName, FGLGlitchRecord> Records;
	TArray<TWeakObjectPtr<AGLGlitch>> Glitches;
	TWeakObjectPtr<AActor> CommanderOverride;
	double SinceEvaluation = 0.0;
};
