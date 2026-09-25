#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLPuzzleSubsystem.generated.h"

class AGLPehlichi;

/**
 * Puzzles as world state (ADR-0023). NICE poses a glitch's puzzle when Pehlichi reveals it
 * (Event.Puzzle.Posed). Pehlichi gives escalating hints on request, capped by his analysis
 * capability (Event.Puzzle.Hint.Tier1..3 / .Exhausted); the dialogue director speaks them.
 * Only gameplay solves a puzzle (a puzzle site's PRESENT interaction), never dialogue.
 */
UCLASS()
class GRIDLANDSGAME_API UGLPuzzleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsSolved(FName PuzzleId) const { return Solved.Contains(PuzzleId); }
	bool IsPosed(FName PuzzleId) const { return Posed.Contains(PuzzleId); }
	int32 HintLevel(FName PuzzleId) const { return HintLevels.FindRef(PuzzleId); }
	/** The most recently posed unsolved puzzle (what "give me a hint" is about), or None. */
	FName ActivePuzzle() const;

	/** Called by gameplay (a puzzle site) when Zenny performs the answer. Emits Event.Puzzle.Solved. */
	bool Solve(FName PuzzleId);
	/** Zenny asks Pehlichi for help. Returns the tier given (0 when Pehlichi can go no further). */
	int32 RequestHint(const AGLPehlichi* Pehlichi);

	void Restore(const TArray<FName>& InSolved, const TArray<FName>& InPosed, const TArray<TPair<FName, int32>>& InHints);
	const TSet<FName>& GetSolved() const { return Solved; }
	const TArray<FName>& GetPosed() const { return Posed; }
	const TMap<FName, int32>& GetHintLevels() const { return HintLevels; }

private:
	void HandleGlitchDetected(const struct FGLGameplayEvent& Event);
	void Emit(const TCHAR* Tag, FName PuzzleId);

	TSet<FName> Solved;
	TArray<FName> Posed;
	TMap<FName, int32> HintLevels;
};
