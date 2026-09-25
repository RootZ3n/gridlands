#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLBuildPiece.generated.h"

class UStaticMeshComponent;

/**
 * A placed build piece (ADR-0024): boxes from the piece's data shapes, blocking collision that
 * navigation reads. Holds no structural state; support is derived by UGLBuildingSubsystem.
 * A ghost is the same geometry without collision, used to preview placement.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLBuildPiece : public AActor
{
	GENERATED_BODY()

public:
	AGLBuildPiece();

	/** Builds the geometry for Piece. bGhost: no collision, no navigation, preview colour. */
	bool Setup(const FGLPlacedPiece& InPiece, bool bGhost = false);
	void SetGhostValid(bool bValid);

	const FGLPlacedPiece& GetPiece() const { return Piece; }

private:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Boxes;
	FGLPlacedPiece Piece;
	bool bIsGhost = false;
};
