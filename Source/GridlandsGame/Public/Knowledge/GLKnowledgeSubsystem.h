#pragma once

#include "CoreMinimal.h"
#include "Events/GLGameplayEvent.h"
#include "Knowledge/GLKnowledge.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLKnowledgeSubsystem.generated.h"

/**
 * The world's knowledge (ADR-0019: world-save-bound). Learns from data-driven sources by
 * listening to events: items' onAcquireUnlocks (Event.Item.Acquired) and salvage's
 * onSalvageUnlocks (Event.Salvage.Completed). Emits Event.Knowledge.Unlocked.
 */
UCLASS()
class GRIDLANDSGAME_API UGLKnowledgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Learns Id (e.g. a glitch reward). Returns true if it was new. */
	bool Learn(FName Id);
	bool Knows(FName Id) const { return Knowledge.Knows(Id); }
	const FGLKnowledge& GetKnowledge() const { return Knowledge; }

private:
	void HandleItemAcquired(const FGLGameplayEvent& Event);
	void HandleSalvageCompleted(const FGLGameplayEvent& Event);

	FGLKnowledge Knowledge;
};
