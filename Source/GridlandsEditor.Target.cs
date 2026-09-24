using UnrealBuildTool;

public class GridlandsEditorTarget : TargetRules
{
	public GridlandsEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "GridlandsCore", "GridlandsGame", "GridlandsEditor", "GridlandsTerrainSpike" });
	}
}
