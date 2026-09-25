#include "Dialogue/GLDialogueRules.h"

#include "Content/GLContentDefinitions.h"

namespace GLDialogueRules
{
	FGLCommentaryTuning TuningFor(EGLCommentaryFrequency Frequency)
	{
		switch (Frequency)
		{
		case EGLCommentaryFrequency::Quiet:    return { 300.0, 0.35, 0.10 };
		case EGLCommentaryFrequency::Chatty:   return { 60.0, 1.00, 0.60 };
		case EGLCommentaryFrequency::Unhinged: return { 20.0, 1.00, 1.00 };
		case EGLCommentaryFrequency::Normal:
		default:                               return { 120.0, 0.80, 0.35 };
		}
	}

	bool TagMatches(FName Tag, FName Filter)
	{
		if (Filter.IsNone())
		{
			return false;
		}
		const FString T = Tag.ToString(), F = Filter.ToString();
		return T == F || (T.Len() > F.Len() && T.StartsWith(F, ESearchCase::CaseSensitive) && T[F.Len()] == TEXT('.'));
	}

	int32 CountEvents(const FGLDialogueState& State, FName Tag)
	{
		int32 Total = 0;
		for (const TPair<FName, int32>& Pair : State.EventCounts)
		{
			Total += TagMatches(Pair.Key, Tag) ? Pair.Value : 0;
		}
		return Total;
	}

	bool RequirementsMet(const FGLExchangeDef& Exchange, const FGLDialogueState& State)
	{
		for (const FGLExchangeRequirementDef& Requirement : Exchange.Requires)
		{
			const int32 Count = CountEvents(State, Requirement.EventCount);
			if (Count < Requirement.Min || (Requirement.Max > 0 && Count > Requirement.Max))
			{
				return false;
			}
		}
		return true;
	}

	void RecordEvent(FName EventTag, FGLDialogueState& State)
	{
		++State.EventCounts.FindOrAdd(EventTag);
	}

	FGLDialogueChoice Choose(TArrayView<const FGLExchangeDef* const> Exchanges, FName EventTag, FName Subject, double Now,
		EGLCommentaryFrequency Frequency, const FGLDialogueState& State, FRandomStream& Random)
	{
		const bool bBusy = Now < State.PlayingUntil;
		bool bDeferredCritical = false;
		TArray<const FGLExchangeDef*> Eligible;
		for (const FGLExchangeDef* Exchange : Exchanges)
		{
			if (!Exchange || !Exchange->Trigger.ContainsByPredicate([EventTag](FName Trigger) { return TagMatches(EventTag, Trigger); }))
			{
				continue;
			}
			if (!Exchange->Subject.IsNone() && Exchange->Subject != Subject)
			{
				continue;
			}
			const FGLExchangeHistory* History = State.Exchanges.Find(Exchange->Id);
			const int32 Uses = History ? History->Uses : 0;
			const double LastUsed = History ? History->LastUsed : -1.0e12;
			if ((Exchange->MaxUses > 0 && Uses >= Exchange->MaxUses) || Now - LastUsed < Exchange->CooldownSeconds || !RequirementsMet(*Exchange, State))
			{
				continue;
			}
			const bool bCritical = Exchange->Category == EGLExchangeCategory::StoryCritical;
			if (bBusy && (!bCritical || State.bPlayingStoryCritical))
			{
				bDeferredCritical |= bCritical;
				continue;
			}
			Eligible.Add(Exchange);
		}
		if (Eligible.Num() == 0)
		{
			if (bDeferredCritical)
			{
				return { nullptr, TEXT("deferred: a story-critical exchange is playing"), true };
			}
			return { nullptr, bBusy ? TEXT("busy: an exchange is playing") : TEXT("no eligible exchange") };
		}

		// Story-critical first; then by priority.
		Eligible.Sort([](const FGLExchangeDef& A, const FGLExchangeDef& B)
		{
			const bool ACrit = A.Category == EGLExchangeCategory::StoryCritical, BCrit = B.Category == EGLExchangeCategory::StoryCritical;
			return ACrit != BCrit ? ACrit : A.Priority > B.Priority;
		});
		const bool bCritical = Eligible[0]->Category == EGLExchangeCategory::StoryCritical;
		TArray<const FGLExchangeDef*> Top = Eligible.FilterByPredicate([&](const FGLExchangeDef* E)
		{
			return (E->Category == EGLExchangeCategory::StoryCritical) == bCritical && E->Priority == Eligible[0]->Priority;
		});

		// Weighted draw among equals, favouring less-used exchanges (repetition protection).
		double TotalWeight = 0.0;
		TArray<double> Weights;
		for (const FGLExchangeDef* E : Top)
		{
			const FGLExchangeHistory* History = State.Exchanges.Find(E->Id);
			Weights.Add(E->Weight / (1.0 + (History ? History->Uses : 0)));
			TotalWeight += Weights.Last();
		}
		double Roll = Random.FRand() * TotalWeight;
		const FGLExchangeDef* Picked = Top.Last();
		for (int32 Index = 0; Index < Top.Num(); ++Index)
		{
			if (Roll < Weights[Index])
			{
				Picked = Top[Index];
				break;
			}
			Roll -= Weights[Index];
		}

		if (!bCritical)
		{
			const FGLCommentaryTuning Tuning = TuningFor(Frequency);
			if (Now - State.LastOptionalTime < Tuning.SilenceGapSeconds)
			{
				return { nullptr, TEXT("silence gap") };
			}
			const double Chance = Picked->Category == EGLExchangeCategory::Ambient ? Tuning.AmbientChance : Tuning.ContextualChance;
			if (Random.FRand() >= Chance)
			{
				return { nullptr, TEXT("frequency roll") };
			}
		}
		return { Picked, FString() };
	}

	double EstimateDuration(const FGLExchangeDef& Exchange)
	{
		double Seconds = 0.0;
		for (const FGLExchangeLineDef& Line : Exchange.Lines)
		{
			Seconds += 0.8 + 0.055 * Line.Text.Len();
		}
		return Seconds;
	}

	void RecordPlayed(const FGLExchangeDef& Exchange, double Now, FGLDialogueState& State)
	{
		FGLExchangeHistory& History = State.Exchanges.FindOrAdd(Exchange.Id);
		++History.Uses;
		History.LastUsed = Now;
		const bool bCritical = Exchange.Category == EGLExchangeCategory::StoryCritical;
		if (!bCritical)
		{
			State.LastOptionalTime = Now;
		}
		State.PlayingUntil = Now + EstimateDuration(Exchange);
		State.bPlayingStoryCritical = bCritical;
	}
}
