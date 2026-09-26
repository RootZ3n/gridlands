#pragma once

#include "Content/GLContentRegistry.h"
#include "CoreMinimal.h"

/** The engine's loaded content (UGLContentSubsystem). Content is global and read-only at runtime. */
struct FGLTuningDef;

namespace GLContent
{
	GRIDLANDSGAME_API const FGLContentRegistry& Get();
	/** P6 provisional physical and noise tuning (tuning.world.physical; TUN-1 guarantees exactly one). */
	GRIDLANDSGAME_API const FGLTuningDef& Tuning();
}
