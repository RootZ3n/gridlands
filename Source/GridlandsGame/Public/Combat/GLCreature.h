#pragma once

#include "Combat/GLCreatureRules.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GLCreature.generated.h"

class UGLHealthComponent;
struct FGLCreatureDef;
struct FGLGameplayEvent;
struct FGLNoiseEvent;
struct FGLCreatureModel;

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
	/** VisualOverride (dev/proof only): another look than the definition's. */
	bool Setup(FName InDefId, FName InPlacementId, FName VisualOverride = NAME_None);
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** One behaviour step (Tick calls it; tests call it directly). */
	void Think(float DeltaSeconds);

	/**
	 * A world noise (P6): heard by its own hearing (GLCreatureRules::Hears). An ordinary noise sends it
	 * to look; Pehlichi's distraction overrides even a chase. Returns whether it heard it.
	 */
	bool HearNoise(const FGLNoiseEvent& Noise);

	/** Where it last saw Zenny, and for how much longer it will search there (P6). */
	const FVector& GetLastKnown() const { return LastKnown; }
	double GetSearchSecondsLeft() const { return SearchLeft; }
	/** P9: the rest of its gameplay memory (its model mirrors all of it). */
	const FVector& GetNoise() const { return Noise; }
	double GetNoiseSecondsLeft() const { return NoiseLeft; }
	const FVector& GetLure() const { return Lure; }
	double GetLureSecondsLeft() const { return LureLeft; }
	int32 GetPatrolIndex() const { return PatrolIndex; }

	/**
	 * P9 (ADR-0037): made from its model as it is now: position, home, patrol, awareness, timers (remaining
	 * as of Now), health and outcome. A neutralized one is presented held and inert. Never a gameplay event.
	 */
	void RestoreFromModel(const FGLCreatureModel& Model, double Now);
	/** P9: neutralized (alive, no damage): inert at HeldAt (no behaviour, hearing, strikes or navigation), still shown. */
	void PresentNeutralized(const FVector& HeldAt, double HeldYaw);
	/** P9: part of the encounter as a hostile (neither defeated nor neutralized). */
	bool IsActiveHostile() const { return GLCreatureRules::IsActiveHostile(State); }
	bool IsNeutralized() const { return State == EGLCreatureState::Neutralized; }
	/** P9: its own navigation is on (only while active gameplay needs it and no region provides it). */
	bool HasOwnNavigation() const;

	/** Save support: a defeated creature stays defeated, silently. */
	void RestoreDefeated();
	/** P8: its placement's cell streamed out: out of play at once (no behaviour, hearing, collision or navigation), destroyed later within the presentation budget. */
	void Retire();

	EGLCreatureState GetState() const { return State; }
	bool IsDefeated() const { return State == EGLCreatureState::Defeated; }
	FName GetDefId() const { return DefId; }
	FName GetPlacementId() const { return PlacementId; }
	UGLHealthComponent* GetHealth() const { return Health; }
	class UGLDerezComponent* GetDerez() const { return Derez; }
	class UNavigationInvokerComponent* GetNavInvoker() const { return NavInvoker; }
	const FVector& GetHome() const { return Home; }

private:
	void HandleDied(AActor* Killer);
	void HandleDamaged(double Taken, AActor* Instigator);
	void UpdateNavigationNeed();
	void WriteThrough();
	void Enter(EGLCreatureState Next);
	void Emit(const TCHAR* Tag);
	bool LineOfSightTo(const AActor* Target) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLHealthComponent> Health;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UGLDerezComponent> Derez;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UNavigationInvokerComponent> NavInvoker;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Body;
	FName DefId;
	FName PlacementId;
	FVector Home = FVector::ZeroVector;
	EGLCreatureState State = EGLCreatureState::Idle;
	double SinceAttack = 1e9;
	double LureLeft = 0.0;
	FVector Lure = FVector::ZeroVector;
	double NoiseLeft = 0.0;
	FVector Noise = FVector::ZeroVector;
	double SearchLeft = 0.0;
	FVector LastKnown = FVector::ZeroVector;
	FVector LastMoveTarget = FVector(1e12);
	/** P9: its patrol loop (world cm) and the waypoint it heads for. */
	TArray<FVector> Patrol;
	int32 PatrolIndex = 0;
};
