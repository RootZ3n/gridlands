#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLStormSubsystem.generated.h"

class AGLStormArtifact;
struct FGLGameplayEvent;

/**
 * Glitch Storms (M11): NICE bending the world. One representative, bounded event: when a storm's
 * trigger count is reached (e.g. the second repair), it rains glitch artifacts around Zenny for
 * its duration, then stops and cleans up every artifact. Emits Event.Storm.Started/Ended for
 * dialogue. Persistence: a storm in progress is never saved (it is over on load); that it
 * happened is, so it does not repeat.
 */
UCLASS()
class GRIDLANDSGAME_API UGLStormSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGLStormSubsystem, STATGROUP_Tickables); }

	/** Starts StormId around Center now (triggers call this; so can a debug command). */
	bool Start(FName StormId, const AActor* Center);
	/** One step (Tick calls it; tests call it directly). */
	void Step(float DeltaTime);

	bool IsRaining() const { return !Active.IsNone(); }
	FName GetActive() const { return Active; }
	int32 NumArtifacts() const;
	const TArray<FName>& GetOccurred() const { return Occurred; }
	void Restore(const TArray<FName>& InOccurred, const TMap<FName, int32>& EventCounts);

private:
	void HandleEvent(const FGLGameplayEvent& Event);
	void Stop();
	void Emit(const TCHAR* Tag, FName Subject);

	TArray<FName> Occurred;
	TMap<FName, int32> Counts;
	FName Active;
	TWeakObjectPtr<const AActor> CenterActor;
	double Elapsed = 0.0;
	double SpawnDebt = 0.0;
	int32 Spawned = 0;
	FRandomStream Random{ 0xCA7D06 };
	UPROPERTY() TArray<TObjectPtr<AGLStormArtifact>> Artifacts;
	FDelegateHandle Subscription;
};
