#include "Puzzle/GLPuzzleSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLCapabilityRules.h"
#include "Pehlichi/GLPehlichi.h"

namespace
{
	const FName AnalysisCapability(TEXT("capability.pehlichi.analysis"));
	constexpr int32 MaxHintTier = 3;
}

void UGLPuzzleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGLEventSubsystem* Bus = Collection.InitializeDependency<UGLEventSubsystem>())
	{
		Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Glitch.Detected")),
			FGLGameplayEventDelegate::CreateUObject(this, &UGLPuzzleSubsystem::HandleGlitchDetected));
	}
}

void UGLPuzzleSubsystem::Emit(const TCHAR* Tag, FName PuzzleId)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
	Event.Subject = PuzzleId;
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
}

void UGLPuzzleSubsystem::HandleGlitchDetected(const FGLGameplayEvent& Event)
{
	const FGLGlitchDef* Glitch = GLContent::Get().Find<FGLGlitchDef>(Event.Subject);
	if (!Glitch)
	{
		return;
	}
	for (const FGLGlitchRequirementDef& Requirement : Glitch->Requirements)
	{
		if (Requirement.Kind == TEXT("Requirement.PuzzleSolved") && !Requirement.Puzzle.IsNone()
			&& !Solved.Contains(Requirement.Puzzle) && !Posed.Contains(Requirement.Puzzle))
		{
			Posed.Add(Requirement.Puzzle);
			Emit(TEXT("Event.Puzzle.Posed"), Requirement.Puzzle); // NICE poses it (dialogue data)
		}
	}
}

FName UGLPuzzleSubsystem::ActivePuzzle() const
{
	for (int32 Index = Posed.Num() - 1; Index >= 0; --Index)
	{
		if (!Solved.Contains(Posed[Index]))
		{
			return Posed[Index];
		}
	}
	return NAME_None;
}

bool UGLPuzzleSubsystem::Solve(FName PuzzleId)
{
	if (!GLContent::Get().Find<FGLPuzzleDef>(PuzzleId) || Solved.Contains(PuzzleId))
	{
		return false;
	}
	Solved.Add(PuzzleId);
	Emit(TEXT("Event.Puzzle.Solved"), PuzzleId);
	return true;
}

int32 UGLPuzzleSubsystem::RequestHint(const AGLPehlichi* Pehlichi)
{
	const FName Puzzle = ActivePuzzle();
	if (Puzzle.IsNone())
	{
		return 0;
	}
	// Pehlichi's analysis sets how far he can read NICE's puzzles (ADR-0023).
	int32 Cap = 1;
	const FGLCapabilityDef* Analysis = GLContent::Get().Find<FGLCapabilityDef>(AnalysisCapability);
	if (Analysis && Pehlichi)
	{
		Cap = FMath::Clamp(FMath::RoundToInt(GLCapabilityRules::EffectValue(*Analysis, Pehlichi->GetCapabilities()->Level(AnalysisCapability), TEXT("puzzle_insight"))), 1, MaxHintTier);
	}
	const int32 Next = HintLevel(Puzzle) + 1;
	if (Next > Cap)
	{
		Emit(TEXT("Event.Puzzle.Hint.Exhausted"), Puzzle);
		return 0;
	}
	HintLevels.Add(Puzzle, Next);
	Emit(*FString::Printf(TEXT("Event.Puzzle.Hint.Tier%d"), Next), Puzzle);
	return Next;
}

void UGLPuzzleSubsystem::Restore(const TArray<FName>& InSolved, const TArray<FName>& InPosed, const TArray<TPair<FName, int32>>& InHints)
{
	Solved = TSet<FName>(InSolved);
	Posed = InPosed;
	HintLevels.Reset();
	for (const TPair<FName, int32>& Hint : InHints)
	{
		HintLevels.Add(Hint.Key, Hint.Value);
	}
}
