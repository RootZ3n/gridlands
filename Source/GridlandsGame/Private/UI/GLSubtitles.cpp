#include "UI/GLSubtitles.h"

#include "Dialogue/GLDialogueDirector.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSubtitles(TEXT("gl.Subtitles"), 1, TEXT("Show NICE and Pehlichi subtitles (1) or not (0)."));
	TAutoConsoleVariable<float> CVarReadingScale(TEXT("gl.Subtitles.Scale"), 1.f, TEXT("Subtitle reading time multiplier (accessibility)."));
	TAutoConsoleVariable<float> CVarTextScale(TEXT("gl.Subtitles.TextScale"), 1.f, TEXT("Subtitle text size multiplier (accessibility)."));
}

FGLSpeakerStyle FGLSpeakerStyle::For(FName Speaker)
{
	if (Speaker == TEXT("NICE"))
	{
		return { TEXT("NICE"), FLinearColor(1.f, 0.35f, 0.95f) };
	}
	if (Speaker == TEXT("Pehlichi"))
	{
		return { TEXT("PEHLICHI"), FLinearColor(0.3f, 0.95f, 1.f) };
	}
	return { Speaker.ToString().ToUpper(), FLinearColor(0.9f, 0.9f, 0.9f) };
}

double FGLSubtitleQueue::ReadingSeconds(const FString& Text, double Scale)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	return FMath::Clamp(1.2 + Words.Num() / 2.6, 2.0, 9.0) * FMath::Max(0.25, Scale);
}

void FGLSubtitleQueue::Push(const FGLDialogueLine& Line, double Now, double VoiceSeconds)
{
	if (Line.ExchangeId != CurrentExchange)
	{
		// A new exchange: anything left of the previous one was interrupted (the director decided).
		Lines.RemoveAll([Now](const FGLSubtitle& S) { return S.Start > Now; });
		for (FGLSubtitle& S : Lines)
		{
			S.End = FMath::Min(S.End, Now);
		}
		CurrentExchange = Line.ExchangeId;
	}
	FGLSubtitle& S = Lines.AddDefaulted_GetRef();
	S.Exchange = Line.ExchangeId;
	S.LineIndex = Line.LineIndex;
	S.Speaker = Line.Speaker;
	S.Text = Line.Text;
	S.Start = Now + Line.Delay;
	S.End = S.Start + (VoiceSeconds > 0.0 ? VoiceSeconds + 0.3 : ReadingSeconds(Line.Text, ReadingScale));
}

TArray<FGLSubtitle> FGLSubtitleQueue::Visible(double Now, int32 Max) const
{
	TArray<FGLSubtitle> Out;
	for (const FGLSubtitle& S : Lines)
	{
		if (S.Start <= Now && Now < S.End)
		{
			Out.Add(S);
		}
	}
	Out.Sort([](const FGLSubtitle& First, const FGLSubtitle& Second) { return First.Start < Second.Start; });
	while (Out.Num() > Max)
	{
		Out.RemoveAt(0);
	}
	return Out;
}

void UGLSubtitleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGLDialogueDirector* Director = Collection.InitializeDependency<UGLDialogueDirector>())
	{
		Director->OnLine.AddUObject(this, &UGLSubtitleSubsystem::HandleLine);
	}
}

double UGLSubtitleSubsystem::Now() const
{
	return TimeOverride.IsSet() ? TimeOverride.GetValue() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void UGLSubtitleSubsystem::HandleLine(const FGLDialogueLine& Line)
{
	Queue.ReadingScale = CVarReadingScale.GetValueOnGameThread();
	double VoiceSeconds = 0.0;
	if (!Line.Voice.IsEmpty())
	{
		if (USoundBase* Voice = LoadObject<USoundBase>(nullptr, *Line.Voice))
		{
			VoiceSeconds = Voice->GetDuration();
			// The voice starts with its subtitle.
			TWeakObjectPtr<UGLSubtitleSubsystem> Self(this);
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Voice]()
			{
				UGameplayStatics::PlaySound2D(this, Voice);
			}), FMath::Max(0.01, Line.Delay), false);
		}
	}
	Queue.Prune(Now());
	Queue.Push(Line, Now(), VoiceSeconds);
}

TArray<FGLSubtitle> UGLSubtitleSubsystem::Visible() const
{
	return CVarSubtitles.GetValueOnGameThread() != 0 ? Queue.Visible(Now()) : TArray<FGLSubtitle>();
}
