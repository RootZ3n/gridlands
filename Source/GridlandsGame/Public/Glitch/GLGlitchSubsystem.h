#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLGlitchSubsystem.generated.h"

class AGLGlitch;
class UGLGlitchComponent;

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

	void Register(AGLGlitch* Glitch);
	AGLGlitch* FindByPlacement(FName PlacementId) const;
	/** Glitches within RadiusCm of Origin, nearest first. */
	TArray<AGLGlitch*> GlitchesNear(const FVector& Origin, double RadiusCm) const;
	const TArray<TWeakObjectPtr<AGLGlitch>>& GetAll() const { return Glitches; }

	/** Whose inventory ItemDelivered requirements check. Defaults to player 0's pawn. */
	void SetCommander(AActor* Commander) { CommanderOverride = Commander; }
	AActor* GetCommander() const;

	/** Re-evaluates every glitch's requirements now (World authority). */
	void EvaluateRequirements();

private:
	TArray<TWeakObjectPtr<AGLGlitch>> Glitches;
	TWeakObjectPtr<AActor> CommanderOverride;
	double SinceEvaluation = 0.0;
};
