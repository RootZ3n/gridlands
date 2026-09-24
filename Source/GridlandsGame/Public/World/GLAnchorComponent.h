#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLAnchorComponent.generated.h"

/**
 * Marks a visual-world actor that gameplay placements may refer to (ADR-0018).
 * The id is anchor.<cell>.<name>; Tools/export-anchors.sh writes every anchor in a
 * cell's map to Data/anchor/<cell>.generated.json so agents can see them without the editor.
 */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLAnchorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Gridlands") FName AnchorId;
};
