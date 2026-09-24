using UnrealBuildTool;

public class GridlandsTarget : TargetRules
{
	public GridlandsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "GridlandsCore", "GridlandsGame" });
	}
}
