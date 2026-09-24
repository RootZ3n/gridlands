#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLSalvageNode.generated.h"

class UGLSalvageableComponent;
class UStaticMeshComponent;

/** A gameplay salvage node spawned from a salvage_node placement (ADR-0018). Placeholder mesh. */
UCLASS()
class GRIDLANDSGAME_API AGLSalvageNode : public AActor
{
	GENERATED_BODY()

public:
	AGLSalvageNode();

	UGLSalvageableComponent* GetSalvageable() const { return Salvageable; }
	/** The placement id this node was spawned from (its persistence key). */
	UPROPERTY(VisibleAnywhere, Category = "Gridlands") FName PlacementId;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLSalvageableComponent> Salvageable;
};
