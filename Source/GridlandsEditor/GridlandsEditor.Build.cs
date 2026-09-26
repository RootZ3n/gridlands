using UnrealBuildTool;

// Editor-only: the JSON -> DataAsset importer/validator lands here in M2 (ADR-0002).
public class GridlandsEditor : ModuleRules
{
	public GridlandsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "GridlandsCore", "GridlandsGame", "Json", "AssetTools", "MeshDescription", "StaticMeshDescription" });
	}
}
