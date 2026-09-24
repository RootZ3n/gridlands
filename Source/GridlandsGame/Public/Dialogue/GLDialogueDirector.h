#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GLDialogueRules.h"
#include "Events/GLGameplayEvent.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLDialogueDirector.generated.h"

struct FGLExchangeDef;

/** One delivered line: who says what, as part of which exchange. */
struct GRIDLANDSGAME_API FGLDialogueLine
{
	FName ExchangeId;
	int32 LineIndex = 0;
	FName Speaker;
	FString Text;
	/** Seconds after the exchange started at which this line should appear. */
	double Delay = 0.0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FGLOnDialogueLine, const FGLDialogueLine&);

/**
 * NICE/Pehlichi banter (ADR-0015). Listens to every Event.* on the world's bus, applies the pure
 * GLDialogueRules, and delivers authored lines through OnLine. Gameplay code never calls this
 * class (D-4); presentation code only subscribes to OnLine.
 */
UCLASS()
class GRIDLANDSGAME_API UGLDialogueDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void SetFrequency(EGLCommentaryFrequency InFrequency) { Frequency = InFrequency; }
	EGLCommentaryFrequency GetFrequency() const { return Frequency; }
	/** Tests and replays pin the random stream so selection is reproducible (D-5). */
	void SetSeed(int32 Seed) { Random.Initialize(Seed); }
	/** Tests control time; normally the world's clock is used. */
	void SetTimeOverride(TOptional<double> Seconds) { TimeOverride = Seconds; }

	const FGLDialogueState& GetState() const { return State; }
	FGLOnDialogueLine OnLine;

	/** Show lines as on-screen debug text (off in tests). */
	bool bShowOnScreen = true;

private:
	void HandleEvent(const FGLGameplayEvent& Event);
	double Now() const;

	FGLDialogueState State;
	EGLCommentaryFrequency Frequency = EGLCommentaryFrequency::Normal;
	FRandomStream Random{ 0x9E1C };
	TOptional<double> TimeOverride;
	FDelegateHandle Subscription;
	TArray<const FGLExchangeDef*> Exchanges;
};
