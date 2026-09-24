#include "Glitch/GLGlitchLifecycle.h"

namespace GLGlitchLifecycle
{
	constexpr uint8 AuthorityBit(EGLGlitchAuthority Authority)
	{
		return static_cast<uint8>(1u << static_cast<uint8>(Authority));
	}

	struct FTransitionRule
	{
		EGLGlitchState From;
		EGLGlitchState To;
		uint8 AllowedAuthorities;
	};

	using S = EGLGlitchState;
	using A = EGLGlitchAuthority;

	// Mirrors the table in Docs/GLITCH-AND-PEHLICHI.md section 2. Change both together.
	// Note the absence of A::Player anywhere in this table.
	constexpr FTransitionRule Rules[] = {
		{ S::Latent,      S::Detected,    AuthorityBit(A::PehlichiScan) },
		{ S::Detected,    S::Latent,      AuthorityBit(A::Hostile) },
		{ S::Repairable,  S::Latent,      AuthorityBit(A::Hostile) },
		{ S::Detected,    S::Repairable,  AuthorityBit(A::World) },
		{ S::Repairable,  S::Detected,    AuthorityBit(A::World) },
		{ S::Repairable,  S::Repairing,   AuthorityBit(A::PehlichiRepair) },
		{ S::Repairing,   S::Interrupted, AuthorityBit(A::PehlichiRepair) | AuthorityBit(A::Hostile) | AuthorityBit(A::World) },
		{ S::Interrupted, S::Repairing,   AuthorityBit(A::PehlichiRepair) },
		{ S::Interrupted, S::Detected,    AuthorityBit(A::World) },
		{ S::Repairing,   S::Repaired,    AuthorityBit(A::PehlichiRepair) },
	};

	uint8 AllowedMask(EGLGlitchState From, EGLGlitchState To)
	{
		for (const FTransitionRule& Rule : Rules)
		{
			if (Rule.From == From && Rule.To == To)
			{
				return Rule.AllowedAuthorities;
			}
		}
		return 0;
	}
}

bool FGLGlitchLifecycle::IsTransitionAllowed(EGLGlitchState From, EGLGlitchState To, EGLGlitchAuthority By)
{
	if (From >= EGLGlitchState::Count || To >= EGLGlitchState::Count || By >= EGLGlitchAuthority::Count)
	{
		return false;
	}
	return (GLGlitchLifecycle::AllowedMask(From, To) & GLGlitchLifecycle::AuthorityBit(By)) != 0;
}

bool FGLGlitchLifecycle::IsTransitionAllowedByAnyone(EGLGlitchState From, EGLGlitchState To)
{
	return GLGlitchLifecycle::AllowedMask(From, To) != 0;
}

bool FGLGlitchLifecycle::IsTerminal(EGLGlitchState State)
{
	return State == EGLGlitchState::Repaired;
}

bool FGLGlitchLifecycle::IsVisibleToPlayer(EGLGlitchState State)
{
	return State != EGLGlitchState::Latent && State < EGLGlitchState::Count;
}

EGLGlitchState FGLGlitchLifecycle::ToPersistedState(EGLGlitchState State)
{
	return State == EGLGlitchState::Repairing ? EGLGlitchState::Interrupted : State;
}

const TCHAR* FGLGlitchLifecycle::StateName(EGLGlitchState State)
{
	switch (State)
	{
	case EGLGlitchState::Latent:      return TEXT("Latent");
	case EGLGlitchState::Detected:    return TEXT("Detected");
	case EGLGlitchState::Repairable:  return TEXT("Repairable");
	case EGLGlitchState::Repairing:   return TEXT("Repairing");
	case EGLGlitchState::Interrupted: return TEXT("Interrupted");
	case EGLGlitchState::Repaired:    return TEXT("Repaired");
	default:                          return TEXT("Invalid");
	}
}

const TCHAR* FGLGlitchLifecycle::AuthorityName(EGLGlitchAuthority Authority)
{
	switch (Authority)
	{
	case EGLGlitchAuthority::PehlichiScan:   return TEXT("PehlichiScan");
	case EGLGlitchAuthority::PehlichiRepair: return TEXT("PehlichiRepair");
	case EGLGlitchAuthority::World:          return TEXT("World");
	case EGLGlitchAuthority::Hostile:        return TEXT("Hostile");
	case EGLGlitchAuthority::Player:         return TEXT("Player");
	default:                                 return TEXT("Invalid");
	}
}
