#pragma once

#include "Building/GLBuildPiece.h"
#include "CoreMinimal.h"
#include "GLStructurePart.generated.h"

class UGLSalvageableComponent;

/**
 * One part of an authored structure (P6): a build piece in the shared structural language plus the
 * salvage pipeline. It holds no structural state and decides nothing: UGLStructureSubsystem owns the
 * authoritative state and moves it along the collapse plan (presentation follows the result).
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLStructurePart : public AGLBuildPiece
{
	GENERATED_BODY()

public:
	AGLStructurePart();

	UGLSalvageableComponent* GetSalvageable() const { return Salvageable; }

	FName StructurePlacement;
	FName PartName;

	/**
	 * Presentation seam for later cosmetic physics (dust, chips, boards, small rubble). Fired when the
	 * authoritative impact happens; listeners may add spectacle but can never change the outcome.
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FGLOnPresentationImpact, AGLStructurePart* /*Part*/);
	FGLOnPresentationImpact OnPresentationImpact;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLSalvageableComponent> Salvageable;
};
