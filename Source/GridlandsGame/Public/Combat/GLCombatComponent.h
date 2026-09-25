#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLCombatComponent.generated.h"

class AGLCreature;
class UGLHealthComponent;

/**
 * Zenny's fighting and dying (M11). Attack swings the best carried weapon (or fists) at the
 * nearest creature in front and in reach. On death Zenny keeps his things and wakes at his
 * respawn point after a moment (provisional; operator playtest decides any death penalty).
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLCombatComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Returns the creature hit, or null (nothing in reach, cooling down, or dead). */
	AGLCreature* Attack();
	/** Advances the attack cooldown and the respawn timer (Tick calls it; tests call it directly). */
	void Advance(float DeltaSeconds);

	void SetRespawnPoint(const FVector& Where) { RespawnPoint = Where; }
	bool IsAwaitingRespawn() const { return RespawnIn > 0.0; }

	/** Fists when no weapon is carried. */
	UPROPERTY(EditAnywhere, Category = "Combat") float FistDamage = 8.f;
	UPROPERTY(EditAnywhere, Category = "Combat") float FistReachCm = 150.f;
	UPROPERTY(EditAnywhere, Category = "Combat") float FistCooldown = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Combat") float RespawnSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category = "Combat") float MaxHealth = 100.f;

private:
	void HandleDamaged(double Taken, AActor* Instigator);
	void HandleDied(AActor* Killer);
	void Emit(const TCHAR* Tag, FName Subject, double Number = 0.0);
	UGLHealthComponent* Health() const;

	double Cooldown = 0.0;
	double RespawnIn = 0.0;
	FVector RespawnPoint = FVector::ZeroVector;
	bool bBound = false;
};
