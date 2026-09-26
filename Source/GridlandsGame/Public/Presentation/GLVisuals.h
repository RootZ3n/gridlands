#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FGLVisualDef;

/**
 * Attaches a visual (P7, visual.*) to an actor: the imported art mesh with its tint and outline, the
 * sparse corruption cubes and any light. Presentation only: every component is collision-free and
 * never affects navigation, so gameplay stays on the authoritative data.
 */
namespace GLVisuals
{
	constexpr const TCHAR* MeshesPath = TEXT("/Game/Gridlands/Art/Meshes");
	constexpr const TCHAR* CorruptionMaterialPath = TEXT("/Game/Gridlands/Art/Materials/M_GLCorruption.M_GLCorruption");

	GRIDLANDSGAME_API UStaticMesh* LoadMesh(const FString& Name);

	/** Attaches VisualId under Parent. Returns the main mesh component, or null if the visual is unknown. */
	GRIDLANDSGAME_API UStaticMeshComponent* Attach(AActor* Owner, USceneComponent* Parent, FName VisualId, const FTransform& Local = FTransform::Identity);

	/** Adds the dark graphic outline to a component (it writes custom depth, which the stylize post reads). */
	GRIDLANDSGAME_API void SetOutlined(UPrimitiveComponent* Component, bool bOutlined);

	/** How many corruption cubes the visual adds (for evidence and tests). */
	GRIDLANDSGAME_API int32 CorruptionCount(FName VisualId);
}
