using UnrealBuildTool;

// Pure rules and types. No world, no actors: everything here must be testable
// headless without spawning anything. See Docs/ARCHITECTURE.md section 2.
public class GridlandsCore : ModuleRules
{
	public GridlandsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });
	}
}
