#include "Dialogue/GLDialogueDirector.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"

void UGLDialogueDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GLContent::Get().ForEachEntry([this](const FGLContentEntry& Entry)
	{
		if (const FGLExchangeDef* Exchange = Entry.Definition.GetPtr<FGLExchangeDef>())
		{
			Exchanges.Add(Exchange);
		}
	});
	UGLEventSubsystem* Bus = Collection.InitializeDependency<UGLEventSubsystem>();
	if (Bus)
	{
		Subscription = Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event")),
			FGLGameplayEventDelegate::CreateUObject(this, &UGLDialogueDirector::HandleEvent));
	}
}

void UGLDialogueDirector::Deinitialize()
{
	if (UGLEventSubsystem* Bus = GetWorld() ? GetWorld()->GetSubsystem<UGLEventSubsystem>() : nullptr)
	{
		Bus->Unsubscribe(Subscription);
	}
	Super::Deinitialize();
}

void UGLDialogueDirector::RestoreHistory(const TArray<TPair<FName, int32>>& Uses, const TArray<TPair<FName, int32>>& EventCounts)
{
	State = FGLDialogueState();
	for (const TPair<FName, int32>& Use : Uses)
	{
		State.Exchanges.Add(Use.Key).Uses = Use.Value;
	}
	for (const TPair<FName, int32>& Count : EventCounts)
	{
		State.EventCounts.Add(Count.Key, Count.Value);
	}
}

double UGLDialogueDirector::Now() const
{
	return TimeOverride.IsSet() ? TimeOverride.GetValue() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

namespace
{
	constexpr int32 MaxDeferred = 16;
}

void UGLDialogueDirector::HandleEvent(const FGLGameplayEvent& Event)
{
	const FName EventTag = Event.Tag.GetTagName();
	GLDialogueRules::RecordEvent(EventTag, State);
	if (Offer(EventTag, Event.Subject).bDeferred)
	{
		const TPair<FName, FName> Entry(EventTag, Event.Subject);
		if (!Deferred.Contains(Entry))
		{
			// When full, refuse the newest: earlier lines set up later ones (a riddle before its hints).
			if (Deferred.Num() >= MaxDeferred)
			{
				UE_LOG(LogGridlands, Warning, TEXT("Dialogue: deferred queue full; not queueing %s"), *EventTag.ToString());
			}
			else
			{
				Deferred.Add(Entry);
			}
		}
		ScheduleDeferred();
	}
}

void UGLDialogueDirector::ScheduleDeferred()
{
	UWorld* World = GetWorld();
	if (Deferred.Num() == 0 || TimeOverride.IsSet() || !World)
	{
		return;
	}
	const float Wait = FMath::Max(0.05f, static_cast<float>(State.PlayingUntil - Now()) + 0.05f);
	World->GetTimerManager().SetTimer(DeferredTimer, FTimerDelegate::CreateUObject(this, &UGLDialogueDirector::PlayDeferred), Wait, false);
}

void UGLDialogueDirector::PlayDeferred()
{
	// Oldest first; stop at the first one that is still held back (order is preserved).
	while (Deferred.Num() > 0)
	{
		const TPair<FName, FName> Next = Deferred[0];
		const FGLDialogueChoice Choice = Offer(Next.Key, Next.Value);
		if (Choice.bDeferred)
		{
			break;
		}
		Deferred.RemoveAt(0); // played, or no longer eligible (e.g. already used)
		if (Choice.Exchange)
		{
			break; // it is now the one playing; the rest wait for it
		}
	}
	ScheduleDeferred();
}

FGLDialogueChoice UGLDialogueDirector::Offer(FName EventTag, FName Subject)
{
	const double Time = Now();
	const FGLDialogueChoice Choice = GLDialogueRules::Choose(Exchanges, EventTag, Subject, Time, Frequency, State, Random);
	if (!Choice.Exchange)
	{
		UE_LOG(LogGridlands, Verbose, TEXT("Dialogue: %s -> nothing (%s)"), *EventTag.ToString(), *Choice.WhyNot);
		return Choice;
	}
	GLDialogueRules::RecordPlayed(*Choice.Exchange, Time, State);

	double Delay = 0.0;
	for (int32 Index = 0; Index < Choice.Exchange->Lines.Num(); ++Index)
	{
		const FGLExchangeLineDef& Def = Choice.Exchange->Lines[Index];
		const FGLDialogueLine Line{ Choice.Exchange->Id, Index, Def.Speaker, Def.Text, Delay, Def.Voice };
		UE_LOG(LogGridlands, Log, TEXT("Dialogue [%s] %s: %s"), *Line.ExchangeId.ToString(), *Line.Speaker.ToString(), *Line.Text);
		if (bShowOnScreen && GEngine)
		{
			const FColor Colour = Def.Speaker == TEXT("NICE") ? FColor(255, 0, 255) : FColor(0, 255, 255);
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.f + Delay, Colour, FString::Printf(TEXT("%s: %s"), *Def.Speaker.ToString(), *Def.Text));
		}
		OnLine.Broadcast(Line);
		Delay += 0.8 + 0.055 * Def.Text.Len();
	}
	return Choice;
}
