#include "Combat/GLCreatureRules.h"

#include "Content/GLContentDefinitions.h"

double FGLHealth::Damage(double Amount)
{
	const double Taken = FMath::Min(Current, FMath::Max(0.0, Amount));
	Current -= Taken;
	return Taken;
}

bool GLCreatureRules::Sees(const FGLCreatureDef& Def, const FGLCreatureFacts& Facts, bool bAlreadyChasing)
{
	if (!Facts.bZennyAlive || !Facts.bLineOfSight)
	{
		return false;
	}
	const FVector ToZenny = Facts.Zenny - Facts.Self;
	// Once chasing, it keeps track a little further and all around (it knows where Zenny went).
	const double Radius = Def.Perception.SightRadius * 100.0 * (bAlreadyChasing ? 1.25 : 1.0);
	if (ToZenny.Size2D() > Radius)
	{
		return false;
	}
	if (bAlreadyChasing)
	{
		return true;
	}
	const FVector2D Facing = FVector2D(Facts.Forward).GetSafeNormal();
	const FVector2D Direction = FVector2D(ToZenny).GetSafeNormal();
	const double CosHalf = FMath::Cos(FMath::DegreesToRadians(Def.Perception.ConeDegrees * 0.5));
	return FVector2D::DotProduct(Facing, Direction) >= CosHalf;
}

FGLCreatureDecision GLCreatureRules::Decide(const FGLCreatureDef& Def, EGLCreatureState Previous, const FGLCreatureFacts& Facts)
{
	FGLCreatureDecision D;
	if (Facts.bDefeated)
	{
		D.State = EGLCreatureState::Defeated;
		return D;
	}
	// A lure wins over everything else, even a chase: that is what makes distraction a real option.
	if (Facts.LureSecondsLeft > 0.0)
	{
		D.State = EGLCreatureState::Investigate;
		D.bMove = true;
		D.MoveTo = Facts.Lure;
		D.Speed = Def.WalkSpeed * 100.0;
		return D;
	}
	const bool bWasHunting = Previous == EGLCreatureState::Chase || Previous == EGLCreatureState::Attack || Previous == EGLCreatureState::Search;
	const bool bZennyInTerritory = FVector::Dist2D(Facts.Zenny, Facts.Home) <= Def.LeashRadius * 100.0;
	if (bZennyInTerritory && Sees(Def, Facts, bWasHunting))
	{
		const double Distance = FVector::Dist2D(Facts.Zenny, Facts.Self);
		if (Distance <= Def.Attack.Reach * 100.0)
		{
			D.State = EGLCreatureState::Attack;
			D.bStrike = Facts.SecondsSinceAttack >= Def.Attack.CooldownSeconds;
			return D;
		}
		D.State = EGLCreatureState::Chase;
		D.bMove = true;
		D.MoveTo = Facts.Zenny;
		D.Speed = Def.ChaseSpeed * 100.0;
		return D;
	}
	const double Leash = Def.LeashRadius * 100.0;
	// Lost sight: it still knows where Zenny was, and goes to look (sight lost is not knowledge lost).
	if (bWasHunting && Facts.SearchSecondsLeft > 0.0 && FVector::Dist2D(Facts.LastKnown, Facts.Home) <= Leash)
	{
		D.State = EGLCreatureState::Search;
		D.bMove = FVector::Dist2D(Facts.Self, Facts.LastKnown) > HomeToleranceCm;
		D.MoveTo = Facts.LastKnown;
		D.Speed = Def.WalkSpeed * 100.0;
		return D;
	}
	// A noise it heard (within its territory): go and look.
	if (Facts.NoiseSecondsLeft > 0.0 && FVector::Dist2D(Facts.Noise, Facts.Home) <= Leash)
	{
		D.State = EGLCreatureState::Investigate;
		D.bMove = FVector::Dist2D(Facts.Self, Facts.Noise) > HomeToleranceCm;
		D.MoveTo = Facts.Noise;
		D.Speed = Def.WalkSpeed * 100.0;
		return D;
	}
	if (FVector::Dist2D(Facts.Self, Facts.Home) > HomeToleranceCm)
	{
		D.State = EGLCreatureState::Return;
		D.bMove = true;
		D.MoveTo = Facts.Home;
		D.Speed = Def.WalkSpeed * 100.0;
		return D;
	}
	D.State = EGLCreatureState::Idle;
	return D;
}

bool GLCreatureRules::Hears(const FGLCreatureDef& Def, const FVector& Self, const FVector& Noise, double NoiseRadiusCm)
{
	return FVector::Dist(Self, Noise) <= FMath::Min(NoiseRadiusCm, Def.Perception.HearingRadius * 100.0);
}

const TCHAR* GLCreatureRules::StateName(EGLCreatureState State)
{
	switch (State)
	{
	case EGLCreatureState::Idle: return TEXT("Idle");
	case EGLCreatureState::Investigate: return TEXT("Investigate");
	case EGLCreatureState::Chase: return TEXT("Chase");
	case EGLCreatureState::Attack: return TEXT("Attack");
	case EGLCreatureState::Return: return TEXT("Return");
	case EGLCreatureState::Defeated: return TEXT("Defeated");
	case EGLCreatureState::Search: return TEXT("Search");
	}
	return TEXT("?");
}
