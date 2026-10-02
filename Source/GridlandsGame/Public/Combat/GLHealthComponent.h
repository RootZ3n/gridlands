#pragma once

#include "Combat/GLCreatureRules.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLHealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FGLOnDamaged, double /*Taken*/, AActor* /*Instigator*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FGLOnDied, AActor* /*Instigator*/);

/**
 * Hit points for Zenny and creatures (M11). Pehlichi never has code that calls ApplyDamage
 * (P-3, ADR-0017, enforced by test_architecture_rules.py).
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Returns damage actually taken; fires OnDied once when health reaches zero. */
	double ApplyDamage(double Amount, AActor* Instigator);
	void SetMax(double Max) { Health.Max = Max; Health.Current = Max; }
	void Restore(double Current) { Health.Current = FMath::Clamp(Current, 0.0, Health.Max); }
	void Revive() { Health.Reset(); }

	double GetCurrent() const { return Health.Current; }
	double GetMax() const { return Health.Max; }
	bool IsDead() const { return Health.IsDead(); }
	/** P9: a neutralized creature is out of the fight: nothing damages it (environmental killing of a held creature is P10's decision). */
	void SetIgnoresDamage(bool bIgnore) { bIgnoresDamage = bIgnore; }
	bool IgnoresDamage() const { return bIgnoresDamage; }

	FGLOnDamaged OnDamaged;
	FGLOnDied OnDied;

private:
	FGLHealth Health;
	bool bIgnoresDamage = false;
};
