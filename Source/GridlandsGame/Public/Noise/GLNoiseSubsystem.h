#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLNoiseSubsystem.generated.h"

/**
 * One authoritative world-noise event (P6, ADR-0031). Creatures hear it through their own hearing
 * model (GLCreatureRules::Hears); any future visualization must read these same values.
 */
struct GRIDLANDSGAME_API FGLNoiseEvent
{
	/** A Noise.* action (tuning.world.physical noise.radius names its loudness). */
	FName Action;
	FVector Location = FVector::ZeroVector;
	/** How far this sound carries (cm): the action's radius times the material's noiseScale. */
	double RadiusCm = 0.0;
	TWeakObjectPtr<AActor> Instigator;
	/** Seconds a creature that hears it goes to look. */
	double InvestigateSeconds = 0.0;
	/** Pehlichi's distraction: overrides even a chase (ADR-0017: still zero damage). */
	bool bDistraction = false;
	double WorldSeconds = 0.0;
	/** Creatures that heard it (filled by Emit). */
	int32 Heard = 0;
};

/**
 * World noise (P6). Actions that make sound (terraforming, chopping, salvage, building, demolition,
 * structural breakage and collapse, Pehlichi's distraction) emit here. Noise never spawns, summons,
 * raids or reaches anything beyond a creature's own hearing (ADR-0014 as amended).
 */
UCLASS()
class GRIDLANDSGAME_API UGLNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Loudness of an action (cm): the tuning radius for Action, times Material's noiseScale (1 if none). */
	static double RadiusFor(FName Action, FName Material = NAME_None);

	/** Builds a noise from data (radius, investigate seconds) at Location. */
	FGLNoiseEvent Make(FName Action, const FVector& Location, AActor* Instigator, FName Material = NAME_None) const;

	/** Every creature in the world gets to hear it (by its own hearing). Returns how many heard. */
	int32 Emit(FGLNoiseEvent Noise);

	/** Convenience: Make + Emit. */
	static int32 EmitAction(const UObject* Context, FName Action, const FVector& Location, AActor* Instigator, FName Material = NAME_None);

	/** The most recent noises, oldest first (bounded): tests and future visualization. */
	const TArray<FGLNoiseEvent>& GetRecent() const { return Recent; }
	int32 GetEmittedCount() const { return Emitted; }
	int32 CountOf(FName Action) const;

private:
	TArray<FGLNoiseEvent> Recent;
	int32 Emitted = 0;
	TMap<FName, int32> Counts;
};
