#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "GLDialogueRules.generated.h"

struct FGLExchangeDef;

/** The player's optional-commentary setting (ADR-0015). Story-critical lines ignore it. */
UENUM(BlueprintType)
enum class EGLCommentaryFrequency : uint8
{
	Quiet,
	Normal,
	Chatty,
	Unhinged,
};

/** What a frequency setting means. Tuning, not identity: change the table in GLDialogueRules.cpp. */
struct GRIDLANDSCORE_API FGLCommentaryTuning
{
	/** Minimum seconds between two optional exchanges. */
	double SilenceGapSeconds = 0.0;
	/** Chance that an eligible Contextual / Ambient exchange actually plays. */
	double ContextualChance = 1.0;
	double AmbientChance = 1.0;
};

struct GRIDLANDSCORE_API FGLExchangeHistory
{
	int32 Uses = 0;
	double LastUsed = -1.0e12;
};

/** Everything the director remembers. Persisted with the world save in M9. */
struct GRIDLANDSCORE_API FGLDialogueState
{
	TMap<FName, FGLExchangeHistory> Exchanges;
	/** How often each exact Event.* tag has fired. */
	TMap<FName, int32> EventCounts;
	double LastOptionalTime = -1.0e12;
	double PlayingUntil = -1.0e12;
	bool bPlayingStoryCritical = false;
};

struct GRIDLANDSCORE_API FGLDialogueChoice
{
	const FGLExchangeDef* Exchange = nullptr;
	/** Why nothing was chosen (for logs and tests); empty when an exchange was chosen. */
	FString WhyNot;
};

/**
 * The dialogue selection rule (ADR-0015, invariants D-2, D-3, D-5). Pure and deterministic for
 * a given state and random stream, so it is unit-tested headless.
 */
namespace GLDialogueRules
{
	GRIDLANDSCORE_API FGLCommentaryTuning TuningFor(EGLCommentaryFrequency Frequency);

	/** Tag-hierarchy match on names: "Event.Salvage" matches "Event.Salvage.WireStripped". */
	GRIDLANDSCORE_API bool TagMatches(FName Tag, FName Filter);

	/** Sum of EventCounts for Tag and its children. */
	GRIDLANDSCORE_API int32 CountEvents(const FGLDialogueState& State, FName Tag);

	GRIDLANDSCORE_API bool RequirementsMet(const FGLExchangeDef& Exchange, const FGLDialogueState& State);

	/** Counts an event as a history fact. Call before Choose for the same event. */
	GRIDLANDSCORE_API void RecordEvent(FName EventTag, FGLDialogueState& State);

	/**
	 * Picks at most one exchange responding to an event:
	 *  1. triggers match, subject matches, requirements met, under maxUses, cooldown elapsed;
	 *  2. while an exchange plays, only StoryCritical may start, and never over another StoryCritical;
	 *  3. StoryCritical beats everything and ignores the setting;
	 *  4. an optional exchange needs the silence gap to have passed, then wins its category's chance;
	 *  5. highest priority wins; ties are a weighted draw that favours less-used exchanges.
	 */
	GRIDLANDSCORE_API FGLDialogueChoice Choose(TArrayView<const FGLExchangeDef* const> Exchanges, FName EventTag, FName Subject, double Now,
		EGLCommentaryFrequency Frequency, const FGLDialogueState& State, FRandomStream& Random);

	/** Seconds an exchange takes to deliver (reading-speed estimate). */
	GRIDLANDSCORE_API double EstimateDuration(const FGLExchangeDef& Exchange);

	/** Updates State after Exchange starts playing at Now. */
	GRIDLANDSCORE_API void RecordPlayed(const FGLExchangeDef& Exchange, double Now, FGLDialogueState& State);
}
