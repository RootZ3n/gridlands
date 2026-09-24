#pragma once

#include "Content/GLContentRegistry.h"
#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "GLContentSubsystem.generated.h"

/**
 * Owns the content registry for the running engine (ADR-0021). Loaded once at startup from the
 * project's Data/ directory; content is read-only at runtime and addressed by id.
 */
UCLASS()
class GRIDLANDSGAME_API UGLContentSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FGLContentRegistry& GetRegistry() const { return Registry; }

	template <typename TDefinition>
	const TDefinition* Find(FName Id) const { return Registry.Find<TDefinition>(Id); }

private:
	FGLContentRegistry Registry;
};
