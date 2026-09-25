#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLStormArtifact.generated.h"

class UStaticMeshComponent;

/**
 * One falling Glitch Storm artifact (M11): a blocky, pixel-coloured cat or dog that reads as a
 * broken simulation sprite, not a real animal. It tumbles down, jitters, lands, and de-rezzes
 * (shrinks away). Harmless: no collision with anyone.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLStormArtifact : public AActor
{
	GENERATED_BODY()

public:
	AGLStormArtifact();
	void Setup(FName Kind, int32 Seed, double GroundZ);
	/** Physics-free fall and de-rez, driven by UGLStormSubsystem. Returns false once gone. */
	bool Advance(float DeltaSeconds);

	FName GetKind() const { return Kind; }
	bool HasLanded() const { return bLanded; }

private:
	void AddBlock(const FVector& Offset, const FVector& Size, const FLinearColor& Colour);

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Blocks;
	FName Kind;
	FRandomStream Random;
	double GroundZ = 0.0;
	double FallSpeed = 0.0;
	double DerezLeft = 1.5;
	bool bLanded = false;
	FRotator Spin = FRotator::ZeroRotator;
};
