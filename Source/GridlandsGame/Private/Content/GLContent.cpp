#include "Content/GLContent.h"

#include "Content/GLContentSubsystem.h"
#include "Engine/Engine.h"

namespace GLContent
{
	const FGLContentRegistry& Get()
	{
		static const FGLContentRegistry Empty;
		const UGLContentSubsystem* Content = GEngine ? GEngine->GetEngineSubsystem<UGLContentSubsystem>() : nullptr;
		return Content ? Content->GetRegistry() : Empty;
	}
}
