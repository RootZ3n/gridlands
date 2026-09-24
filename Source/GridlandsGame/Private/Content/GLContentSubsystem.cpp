#include "Content/GLContentSubsystem.h"

#include "GridlandsGame.h"
#include "Misc/Paths.h"

void UGLContentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())))
	{
		UE_LOG(LogGridlands, Error, TEXT("Content loaded with %d problem(s); run Tools/data.sh validate"), Registry.GetProblems().Num());
	}
}
