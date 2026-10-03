#pragma once

#include "CoreMinimal.h"

struct FGLCreatureDef;

/** Hit points. Pure; the game's health component wraps it. */
struct GRIDLANDSCORE_API FGLHealth
{
	double Max = 100.0;
	double Current = 100.0;

	bool IsDead() const { return Current <= 0.0; }
	/** Applies damage (>= 0), never below zero. Returns what was actually taken. */
	double Damage(double Amount);
	void Heal(double Amount) { Current = FMath::Min(Max, Current + FMath::Max(0.0, Amount)); }
	void Reset() { Current = Max; }
};

enum class EGLCreatureState : uint8
{
	Idle,        // at home, watching
	Investigate, // going to Pehlichi's lure (a distraction: never an attack)
	Chase,       // has seen Zenny
	Attack,      // in reach; strikes when the cooldown allows
	Return,      // lost Zenny or went too far from home
	Defeated,
	Search,      // lost sight of Zenny: goes to where Zenny was last seen (P6)
	Patrol,      // P9: walking its data-defined loop of waypoints (calm)
	Neutralized, // P9: taken out of the encounter by a non-damage outcome (contained, disabled...): inert, alive
};

/** What the creature knows this moment. The game fills it in (positions, line of sight). */
struct GRIDLANDSCORE_API FGLCreatureFacts
{
	FVector Self = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector Home = FVector::ZeroVector;
	FVector Zenny = FVector::ZeroVector;
	bool bZennyAlive = true;
	/** Nothing solid between the creature's eyes and Zenny. */
	bool bLineOfSight = false;
	/** Seconds left on a lure the creature heard (Pehlichi's distraction; 0 = none). Overrides even a chase. */
	double LureSecondsLeft = 0.0;
	FVector Lure = FVector::ZeroVector;
	/** Seconds left investigating an ordinary noise it heard (P6; 0 = none). Never overrides seeing Zenny. */
	double NoiseSecondsLeft = 0.0;
	FVector Noise = FVector::ZeroVector;
	/** Seconds left searching where Zenny was last seen (P6; 0 = forgotten). */
	double SearchSecondsLeft = 0.0;
	FVector LastKnown = FVector::ZeroVector;
	double SecondsSinceAttack = 1e9;
	bool bDefeated = false;
	/** P9: neutralized (a non-damage outcome): no behaviour at all, and it never counts as defeated. */
	bool bNeutralized = false;
	/** P9: its patrol loop (world cm; empty = it guards its home) and the waypoint it is heading for. */
	TConstArrayView<FVector> Patrol;
	int32 PatrolIndex = 0;
};

struct GRIDLANDSCORE_API FGLCreatureDecision
{
	EGLCreatureState State = EGLCreatureState::Idle;
	bool bMove = false;
	FVector MoveTo = FVector::ZeroVector;
	/** cm/s. */
	double Speed = 0.0;
	bool bStrike = false;
	/** P9: the patrol waypoint it heads for next (unchanged unless patrolling advanced it). */
	int32 PatrolIndex = 0;
};

/**
 * Creature behaviour (M11), pure and deterministic. Threat comes from places (ADR-0014): the
 * creature guards its home and only reacts to Zenny being seen there, never to what Zenny is
 * doing elsewhere. Every non-combat option is real: out of sight is safe, walls block sight, the
 * leash ends a chase, and a lure (Pehlichi's distraction) overrides even a chase.
 */
namespace GLCreatureRules
{
	constexpr double HomeToleranceCm = 150.0;
	/**
	 * P10: every creature's collision capsule (cm). The actor is built with it and a model's impacts are tested with
	 * it, so a creature with no actor is exactly as hittable as a presented one. A model stands at its feet; the
	 * capsule centre is CapsuleHalfHeightCm above them.
	 */
	constexpr double CapsuleRadiusCm = 40.0;
	constexpr double CapsuleHalfHeightCm = 60.0;

	/** Within sight radius, inside the view cone (or already chasing), and in line of sight. */
	GRIDLANDSCORE_API bool Sees(const FGLCreatureDef& Def, const FGLCreatureFacts& Facts, bool bAlreadyChasing);

	GRIDLANDSCORE_API FGLCreatureDecision Decide(const FGLCreatureDef& Def, EGLCreatureState Previous, const FGLCreatureFacts& Facts);

	/**
	 * Does it hear a noise (P6)? Within the noise's own radius (how far that sound carries) and within
	 * the creature's hearing radius (how far it listens). Hearing is separate from sight.
	 */
	GRIDLANDSCORE_API bool Hears(const FGLCreatureDef& Def, const FVector& Self, const FVector& Noise, double NoiseRadiusCm, double AmbientMask = 0.0);

	/**
	 * P9 ambient masking: AMBIENT NOISE REDUCES THE EFFECTIVE AUDIBILITY OF OTHER NOISE AT THE LISTENER.
	 * First implementation (operator-approved, tunable): heard only within min(noise radius, hearing) x (1 - M),
	 * M the strongest active ambient mask at the listener (0..1). The same rule for every stimulus, lures included.
	 */
	GRIDLANDSCORE_API double AudibleRangeCm(double HearingCm, double NoiseRadiusCm, double AmbientMask);

	/**
	 * P9 (ADR-0029 as amended): NAVIGATION EXISTS WHERE ACTIVE GAMEPLAY REQUIRES IT. A creature needs
	 * navigation only while its behaviour may move it: patrolling, investigating, chasing, attacking,
	 * searching, returning. Idle at home, defeated and neutralized creatures never do.
	 */
	GRIDLANDSCORE_API bool NeedsNavigation(EGLCreatureState State);

	/** P9: taking part in the encounter as a hostile (neither defeated nor neutralized). */
	GRIDLANDSCORE_API bool IsActiveHostile(EGLCreatureState State);

	GRIDLANDSCORE_API const TCHAR* StateName(EGLCreatureState State);
}
