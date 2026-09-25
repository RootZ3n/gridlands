#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLSubtitles.generated.h"

struct FGLDialogueLine;

/** How a speaker is presented (name tag and colour). Accessibility: never colour alone; the name is shown. */
struct GRIDLANDSGAME_API FGLSpeakerStyle
{
	FString Name;
	FLinearColor Colour = FLinearColor::White;

	static FGLSpeakerStyle For(FName Speaker);
};

/** One subtitle on the timeline. */
struct GRIDLANDSGAME_API FGLSubtitle
{
	FName Exchange;
	int32 LineIndex = 0;
	FName Speaker;
	FString Text;
	double Start = 0.0;
	double End = 0.0;
};

/**
 * The subtitle timeline (P4), pure so it is testable. The dialogue director still decides what is
 * said and when (queueing, priority, cooldowns, repetition); this only presents it. When a new
 * exchange starts, lines of earlier exchanges that have not appeared yet are dropped and showing
 * ones end: the director only starts one while another is playing when it interrupts it.
 */
class GRIDLANDSGAME_API FGLSubtitleQueue
{
public:
	/** Adds a line the director delivered at Now. VoiceSeconds > 0 times it by its voice. */
	void Push(const FGLDialogueLine& Line, double Now, double VoiceSeconds = 0.0);
	/** Lines on screen at Now, oldest first, at most Max. */
	TArray<FGLSubtitle> Visible(double Now, int32 Max = 2) const;
	void Prune(double Now) { Lines.RemoveAll([Now](const FGLSubtitle& S) { return S.End < Now - 1.0; }); }
	int32 Num() const { return Lines.Num(); }

	/** Reading time: about 2.6 words a second plus a beat, 2..9 s, times ReadingScale. */
	static double ReadingSeconds(const FString& Text, double ReadingScale = 1.0);
	/** Accessibility: > 1 keeps subtitles up longer. */
	double ReadingScale = 1.0;

private:
	TArray<FGLSubtitle> Lines;
	FName CurrentExchange;
};

/**
 * Presents NICE and Pehlichi (P4): takes the director's lines, plays their voice if a line has one
 * (future assets attach through the line's data), and keeps the subtitle timeline the HUD draws.
 * Console: gl.Subtitles 0/1, gl.Subtitles.Scale (reading time), gl.Subtitles.TextScale (size).
 */
UCLASS()
class GRIDLANDSGAME_API UGLSubtitleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	TArray<FGLSubtitle> Visible() const;
	const FGLSubtitleQueue& GetQueue() const { return Queue; }
	/** Tests control time; normally the world's clock is used. */
	TOptional<double> TimeOverride;

private:
	void HandleLine(const FGLDialogueLine& Line);
	double Now() const;

	FGLSubtitleQueue Queue;
};
