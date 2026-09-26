#include "Content/GLContent.h"

#include "Content/GLContentDefinitions.h"
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

	const FGLTuningDef& Tuning()
	{
		static const FGLTuningDef Missing; // zeros: no damage, no noise, nothing investigated
		const FGLTuningDef* Found = Get().Find<FGLTuningDef>(TEXT("tuning.world.physical"));
		return Found ? *Found : Missing;
	}
}
