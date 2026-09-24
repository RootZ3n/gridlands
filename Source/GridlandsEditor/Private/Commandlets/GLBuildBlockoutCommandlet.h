#pragma once

#include "Commandlets/Commandlet.h"
#include "GLBuildBlockoutCommandlet.generated.h"

/**
 * Generates the origin cell's blockout map, /Game/Gridlands/Maps/L_Origin (operator decision E8):
 * ~250 m of recognizable modern-day suburbia with one conspicuous 1950s fragment, one smaller
 * Roman fragment and one storm-drain entrance.
 *
 * Until art takes the map over, this file is the map's source: change the layout here and
 * run Tools/build-blockout.sh, then Tools/export-anchors.sh. After hand-off the .umap becomes
 * the source and this generator is retired.
 */
UCLASS()
class UGLBuildBlockoutCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
