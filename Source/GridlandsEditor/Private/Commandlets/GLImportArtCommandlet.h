#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "GLImportArtCommandlet.generated.h"

/**
 * P7 art pipeline, step 2 (after Art/Source/build_assets.py): writes the master materials, imports
 * every FBX in the Blender manifest to /Game/Gridlands/Art/Meshes, binds material slots by name
 * (GL_Painted, GL_Glow, GL_Glass) and validates each mesh against the manifest (bounds, pivot,
 * triangles, slots, vertex colours). Any violation fails the commandlet. Tools/art.sh runs it.
 * Args: -Manifest=<path to manifest.json> [-MaterialsOnly]
 */
UCLASS()
class UGLImportArtCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGLImportArtCommandlet() { IsClient = false; IsEditor = true; IsServer = false; LogToConsole = true; }
	virtual int32 Main(const FString& Params) override;
};
