#pragma once

#include "CoreMinimal.h"
#include "GLGlitchLifecycle.generated.h"

/**
 * Lifecycle of a simulation glitch. See Docs/GLITCH-AND-PEHLICHI.md section 2.
 * Access, resources, guards and jamming are requirements, not states.
 */
UENUM(BlueprintType)
enum class EGLGlitchState : uint8
{
	/** Exists in the simulation; unknown and invisible to the player. */
	Latent,
	/** Revealed by a Pehlichi scan; at least one requirement unmet. */
	Detected,
	/** Detected and every requirement currently met. */
	Repairable,
	/** Pehlichi is actively repairing. Never persisted as-is. */
	Repairing,
	/** A repair stopped part-way; progress kept per the definition's policy. */
	Interrupted,
	/** Terminal. Persistent world change applied, reward granted. */
	Repaired,

	Count UMETA(Hidden)
};

/** Who is asking for a transition. Every transition in the table names one. */
UENUM(BlueprintType)
enum class EGLGlitchAuthority : uint8
{
	/** Pehlichi's scan system. */
	PehlichiScan,
	/** Pehlichi's repair system: the only authority that can repair. */
	PehlichiRepair,
	/** Requirement evaluation by the glitch subsystem. */
	World,
	/** Guards, attacks and hostile-AI jamming. */
	Hostile,
	/** The player. Has no legal transition: the player acts through Pehlichi and the world. */
	Player,

	Count UMETA(Hidden)
};

/**
 * The single source of truth for which glitch transitions are legal and by whom.
 * Game code asks this; it never encodes transitions of its own (ADR-0005).
 */
struct GRIDLANDSCORE_API FGLGlitchLifecycle
{
	static constexpr int32 NumStates = static_cast<int32>(EGLGlitchState::Count);
	static constexpr int32 NumAuthorities = static_cast<int32>(EGLGlitchAuthority::Count);

	static bool IsTransitionAllowed(EGLGlitchState From, EGLGlitchState To, EGLGlitchAuthority By);

	/** True if any authority may move From -> To. */
	static bool IsTransitionAllowedByAnyone(EGLGlitchState From, EGLGlitchState To);

	static bool IsTerminal(EGLGlitchState State);

	/** Latent glitches are invisible until Pehlichi reveals them. */
	static bool IsVisibleToPlayer(EGLGlitchState State);

	/** The state written to a save. A load never resumes a repair nobody is performing. */
	static EGLGlitchState ToPersistedState(EGLGlitchState State);

	static const TCHAR* StateName(EGLGlitchState State);
	static const TCHAR* AuthorityName(EGLGlitchAuthority Authority);
};
