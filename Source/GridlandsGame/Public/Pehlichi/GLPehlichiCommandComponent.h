#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLPehlichiCommandComponent.generated.h"

/** Why Pehlichi refused a command (spoken about by the dialogue director, never by Zenny). */
UENUM()
enum class EGLCommandRejection : uint8
{
	None,
	UnknownCommand,
	Busy,
	NothingRevealedNearby,
	RequirementsUnmet,
	AlreadyRepaired,
	OutOfRange,
};

/**
 * The player's side of the partnership: Zenny commands, Pehlichi decides. Commands are
 * Command.Pehlichi.* tags. Emits Event.Pehlichi.CommandAccepted or
 * Event.Pehlichi.CommandRejected.<Reason> (subject: the command). Reason names match EGLCommandRejection.
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLPehlichiCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	EGLCommandRejection Issue(FName Command, AActor* Commander);

	/** How far from the commander Pehlichi looks for a glitch to repair. */
	UPROPERTY(EditAnywhere, Category = "Gridlands") float RepairSearchRadius = 2000.f;
	/** Seconds between distractions (provisional). */
	UPROPERTY(EditAnywhere, Category = "Gridlands") float DistractCooldownSeconds = 10.f;

private:
	double DistractReadyAt = -1e9;

public:
};
