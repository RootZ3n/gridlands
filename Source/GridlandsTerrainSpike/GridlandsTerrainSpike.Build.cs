using UnrealBuildTool;

// SPIKE S1 (throwaway): terrain technology prototypes. Lives only on branch spike/terrain.
public class GridlandsTerrainSpike : ModuleRules
{
	public GridlandsTerrainSpike(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GeometryCore", "GeometryFramework", "NavigationSystem", "Json", "UnrealEd" });
	}
}
