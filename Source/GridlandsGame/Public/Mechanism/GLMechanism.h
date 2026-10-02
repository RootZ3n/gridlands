#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLMechanism.generated.h"

struct FGLMechanismRecord;
class UStaticMeshComponent;

/**
 * P9 (ADR-0037): a mechanism's presentation, made from its record. Ugly on purpose (engine cubes): a control
 * panel where Pehlichi operates it, a cage frame over its neutralize box (raised or dropped), a fan block that
 * spins while its ambient sound is on. No gameplay here: collision-free, and the state it shows is the record's.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLMechanism : public AActor
{
	GENERATED_BODY()

public:
	AGLMechanism();
	virtual void Tick(float DeltaSeconds) override;

	void Setup(const FGLMechanismRecord& Record);
	/** Shows the record's state: over its presentSeconds after a live switch, at rest after a restore. */
	void PresentState(const FGLMechanismRecord& Record);
	/** Unload: hidden and inert at once, destroyed later within the presentation budget. */
	void Retire();

	FName PlacementId;
	/** Where the cage frame is now (tests read the presentation catching up with the decided state). */
	double GetCageLift() const { return CageLift; }

private:
	UStaticMeshComponent* AddCube(const FVector& Local, const FVector& Scale, const FLinearColor& Colour);

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CagePieces;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Fan;
	FVector CageBase = FVector::ZeroVector;
	double CageHeight = 0.0;
	double CageLift = 0.0;
	double CageTarget = 0.0;
	double DropSeconds = 0.0;
	bool bFanSpinning = false;
};
