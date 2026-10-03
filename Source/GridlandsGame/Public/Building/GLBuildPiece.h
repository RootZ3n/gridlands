#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLBuildPiece.generated.h"

class UStaticMeshComponent;

/**
 * A placed build piece (ADR-0024): boxes from the piece's data shapes, blocking collision that
 * navigation reads. Holds no structural state; support is derived by the structural rules.
 * A ghost is the same geometry without collision, used to preview placement.
 * P11 (ADR-0039): presentation follows the fact: a frame still waiting for its finish shows its frame (studs), a
 * finished piece shows its finish (the finish's look, or the shapes in its tint), a piece complete as built shows its
 * own look. Collision is always the piece's shapes (the authoritative envelope), whatever is shown.
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
	/** P11: the preview colour (GREEN / YELLOW / RED), from the same check the commit makes. */
	void SetGhostPreview(EGLPreview Preview);
	/** P11 removal preview: shown red while it is predicted to fall. */
	void SetRemovalHighlight(bool bOn);
	/** What it is showing now (tests read it): "frame", "finish" or "complete". */
	FName ShownPhase() const { return Shown; }

	const FGLPlacedPiece& GetPiece() const { return Piece; }

	/** P6: solid (blocks, navigation reads it) or not (while a collapse moves it; gameplay is decided elsewhere). */
	void SetSolid(bool bSolid);

private:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Boxes;
	/** P11: visual-only boxes (the frame look, or a finish's tinted shapes). */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Looks;
	FGLPlacedPiece Piece;
	bool bIsGhost = false;
	bool bHighlighted = false;
	FName Shown;
};
