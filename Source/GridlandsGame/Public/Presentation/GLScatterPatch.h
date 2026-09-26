#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLScatterPatch.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
struct FGLGameplayEvent;

/**
 * Scattered vegetation (P7, placement kind "scatter"): Count instances of a visual within Radius,
 * deterministic from the placement id, sitting on the runtime ground. Presentation only, but it
 * follows the systems: nothing grows on dug or raised earth or under structures and debris, and it
 * re-plants when the terrain or a structure changes nearby.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLScatterPatch : public AActor
{
	GENERATED_BODY()

public:
	AGLScatterPatch();
	bool Setup(FName InPlacementId, FName InVisual, double InRadiusCm, int32 InCount);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Re-plants every instance against the current ground and structures. */
	void Rebuild();
	int32 GetInstanceCount() const;

private:
	void HandleChange(const FGLGameplayEvent& Event);

	UPROPERTY() TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
	FName PlacementId;
	FName Visual;
	double RadiusCm = 0.0;
	int32 Count = 0;
	TArray<FDelegateHandle> Subscriptions;
};
