#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLAmbientSubsystem.generated.h"

/**
 * The world noticing Zenny (M11): discoveries (Zenny reaches a discovery site: knowledge learned,
 * Event.Discovery.Found), NICE's ambient beat (Event.Ambient.Tick every AmbientSeconds; the
 * dialogue director's cooldowns and frequency decide whether anyone speaks), and Zenny's silence
 * (Event.Player.Silent once per stretch of standing still). Emits events only; never speaks itself.
 */
UCLASS()
class GRIDLANDSGAME_API UGLAmbientSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGLAmbientSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bEnabled; }

	/** One step against a given Zenny (Tick uses the player pawn; tests pass their own). */
	void Step(float DeltaTime, const AActor* Zenny);

	void Restore(const TArray<FName>& Found) { Discovered = TSet<FName>(Found); }
	TArray<FName> GetDiscovered() const { return Discovered.Array(); }

	UPROPERTY(EditAnywhere, Category = "Ambient") float AmbientSeconds = 90.f;
	UPROPERTY(EditAnywhere, Category = "Ambient") float SilenceSeconds = 45.f;
	bool bEnabled = true;

private:
	void Emit(const TCHAR* Tag, FName Subject);

	TSet<FName> Discovered;
	double SinceAmbient = 0.0;
	double StillFor = 0.0;
	bool bSilenceAnnounced = false;
	FVector LastPosition = FVector(1e12);
};
