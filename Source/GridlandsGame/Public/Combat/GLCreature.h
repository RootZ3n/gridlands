#pragma once

#include "Combat/GLCreatureRules.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GLCreature.generated.h"

class UGLHealthComponent;
struct FGLCreatureDef;
struct FGLGameplayEvent;

/**
 * A corrupted creature (M11) placed by data (ADR-0014: threat from place). Behaviour is
 * GLCreatureRules; this actor senses (distance, view cone, line of sight, Pehlichi's lures),
 * moves through navigation, strikes Zenny, and announces Event.Creature.*.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLCreature : public ACharacter
{
	GENERATED_BODY()

public:
	AGLCreature();
	bool Setup(FName InDefId, FName InPlacementId);
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** One behaviour step (Tick calls it; tests call it directly). */
	void Think(float DeltaSeconds);

	/** Save support: a defeated creature stays defeated, silently. */
	void RestoreDefeated();

	EGLCreatureState GetState() const { return State; }
	bool IsDefeated() const { return State == EGLCreatureState::Defeated; }
	FName GetDefId() const { return DefId; }
	FName GetPlacementId() const { return PlacementId; }
	UGLHealthComponent* GetHealth() const { return Health; }
	const FVector& GetHome() const { return Home; }

private:
	void HandleLure(const FGLGameplayEvent& Event);
	void HandleDied(AActor* Killer);
	void Enter(EGLCreatureState Next);
	void Emit(const TCHAR* Tag);
	bool LineOfSightTo(const AActor* Target) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLHealthComponent> Health;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Body;
	FName DefId;
	FName PlacementId;
	FVector Home = FVector::ZeroVector;
	EGLCreatureState State = EGLCreatureState::Idle;
	double SinceAttack = 1e9;
	double LureLeft = 0.0;
	FVector Lure = FVector::ZeroVector;
	FVector LastMoveTarget = FVector(1e12);
	FDelegateHandle LureSubscription;
};
