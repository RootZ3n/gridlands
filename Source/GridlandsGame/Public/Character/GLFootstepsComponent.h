#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLFootstepsComponent.generated.h"

/**
 * P9: Zenny's movement makes world noise (ADR-0031's one model): a footstep every stride while walking on the
 * ground (Noise.Move.Walk) and a landing after a fall (Noise.Move.Land), each scaled by the floor's material
 * noiseScale (sheet steel carries 3x as far as timber; terrain has no scale). Hearing and ambient masking decide
 * who hears it; nothing here special-cases a surface or a listener.
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLFootstepsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLFootstepsComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The material of what Zenny stands on (a structure part or build piece), or None (terrain, air). */
	FName FloorMaterial() const;
	/** One footstep / one landing now (Tick calls them; tests call them directly). Returns how many heard. */
	int32 EmitStep();
	int32 EmitLanding();

	UPROPERTY(EditAnywhere, Category = "Gridlands") float StrideCm = 120.f;
	/** Downward speed (cm/s) a landing needs to be heard (a step down is not a landing). */
	UPROPERTY(EditAnywhere, Category = "Gridlands") float LandingSpeedCm = 450.f;

private:
	double Walked = 0.0;
	bool bWasFalling = false;
	double FallSpeed = 0.0;
};
