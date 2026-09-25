#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/GLStabilityModel.h"
#include "GLStabilitySubsystem.generated.h"

class AExponentialHeightFog;

/**
 * World stability, derived on demand from the live glitch states (ADR-0013, S-1): nothing here
 * is stored or saved. Consumers: the static HUD, fog, scan confidence, and banter (through
 * Event.World.InterferenceTierChanged and Event.World.Stabilized).
 */
UCLASS()
class GRIDLANDSGAME_API UGLStabilitySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGLStabilitySubsystem, STATGROUP_Tickables); }

	double InterferenceAt(const FVector& Location) const;
	EGLInterferenceTier TierAt(const FVector& Location) const { return GLStabilityModel::TierFor(InterferenceAt(Location)); }
	double NiceComposure() const;
	/** The band baseline at a location: the containing cell's band, or the next band beyond its playable area. */
	double BaselineAt(const FVector& Location) const;

private:
	TArray<FGLStabilitySample> Samples() const;
	void HandleGlitchRepaired(const struct FGLGameplayEvent& Event);

	TWeakObjectPtr<AExponentialHeightFog> Fog;
	EGLInterferenceTier PlayerTier = EGLInterferenceTier::Clear;
	bool bHavePlayerTier = false;
};
