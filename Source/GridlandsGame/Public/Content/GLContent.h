#pragma once

#include "Content/GLContentRegistry.h"
#include "CoreMinimal.h"

/** The engine's loaded content (UGLContentSubsystem). Content is global and read-only at runtime. */
namespace GLContent
{
	GRIDLANDSGAME_API const FGLContentRegistry& Get();
}
