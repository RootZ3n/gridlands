#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLStationComponent.generated.h"

/** Marks its owner as a fabrication station of a kind (Station.* tag), usable within Reach. */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Gridlands") FName StationTag;
	UPROPERTY(EditAnywhere, Category = "Gridlands") float Reach = 300.f;
};
