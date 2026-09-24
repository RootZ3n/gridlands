#include "Dialogue/GLDialogueDirector.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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

double UGLDialogueDirector::Now() const
{
	return TimeOverride.IsSet() ? TimeOverride.GetValue() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void UGLDialogueDirector::HandleEvent(const FGLGameplayEvent& Event)
{
	const FName EventTag = Event.Tag.GetTagName();
	GLDialogueRules::RecordEvent(EventTag, State);
	const double Time = Now();
	const FGLDialogueChoice Choice = GLDialogueRules::Choose(Exchanges, EventTag, Event.Subject, Time, Frequency, State, Random);
	if (!Choice.Exchange)
	{
		UE_LOG(LogGridlands, Verbose, TEXT("Dialogue: %s -> nothing (%s)"), *EventTag.ToString(), *Choice.WhyNot);
		return;
	}
	GLDialogueRules::RecordPlayed(*Choice.Exchange, Time, State);

	double Delay = 0.0;
	for (int32 Index = 0; Index < Choice.Exchange->Lines.Num(); ++Index)
	{
		const FGLExchangeLineDef& Def = Choice.Exchange->Lines[Index];
		const FGLDialogueLine Line{ Choice.Exchange->Id, Index, Def.Speaker, Def.Text, Delay };
		UE_LOG(LogGridlands, Log, TEXT("Dialogue [%s] %s: %s"), *Line.ExchangeId.ToString(), *Line.Speaker.ToString(), *Line.Text);
		if (bShowOnScreen && GEngine)
		{
			const FColor Colour = Def.Speaker == TEXT("NICE") ? FColor(255, 0, 255) : FColor(0, 255, 255);
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.f + Delay, Colour, FString::Printf(TEXT("%s: %s"), *Def.Speaker.ToString(), *Def.Text));
		}
		OnLine.Broadcast(Line);
		Delay += 0.8 + 0.055 * Def.Text.Len();
	}
}
