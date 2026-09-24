using UnrealBuildTool;

// Primary game module: actors, components and subsystems, one folder per system.
public class GridlandsGame : ModuleRules
{
	public GridlandsGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayTags", "EnhancedInput", "InputCore", "GridlandsCore" });
	}
}
