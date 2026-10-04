#pragma once

#include "CoreMinimal.h"
#include "GLContentDefinitions.generated.h"

// Typed, read-only definitions loaded from Data/**/*.json (ADR-0021).
//
// Each struct mirrors one kind's schema in Tools/gldata/schema.py field for field.
// Property names are the JSON keys with the first letter upper-cased. Two tests
// keep the languages in step: Gridlands.Core.Content.SchemaKeysMatchValidator
// (every key path agrees in both directions) and ...RoundTripKeepsEveryField.
// Tag-valued fields are FName here and resolved to FGameplayTag by consumers.
// Numbers are double so authored decimals survive a round trip exactly.

USTRUCT()
struct GRIDLANDSCORE_API FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() int32 SchemaVersion = 0;
	UPROPERTY() FName Id;
	UPROPERTY() FString Notes;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLEraDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Tag;
	UPROPERTY() FString Description;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLBandDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Tag;
	UPROPERTY() int32 Depth = 0;
	UPROPERTY() double BaselineInterference = 0.0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLYieldCategoryDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Tag;
	/** repeatable_common | repeatable_rare | repeatable_drop | unique | glitch_reward | knowledge | reward_blueprint | story | artifact */
	UPROPERTY() FName YieldClass;
	/** True only for repeatable classes (ADR-0016). */
	UPROPERTY() bool Scalable = false;
	/** World setting that scales this category; None when not scalable. */
	UPROPERTY() FName Setting;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLYieldMultipliers
{
	GENERATED_BODY()

	UPROPERTY() double ResourceYield = 1.0;
	UPROPERTY() double CreatureDrops = 1.0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLSettingsPresetDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FGLYieldMultipliers YieldMultipliers;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLMaterialSupport
{
	GENERATED_BODY()

	UPROPERTY() double Strength = 0.0;
	UPROPERTY() double MaxHorizontalSpan = 0.0;
	UPROPERTY() int32 MaxStack = 0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLMaterialDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() TArray<FName> Tags;
	UPROPERTY() double SalvageHardness = 0.0;
	/** Optional. Absent means the material is not structural (Strength == 0). */
	UPROPERTY() FGLMaterialSupport Support;
	/** Optional (P6): scales the radius of noise made by working this material (default 1). */
	UPROPERTY() double NoiseScale = 1.0;
	/** Optional (P6): scales collapse impact damage of pieces made of it (default 1). */
	UPROPERTY() double ImpactScale = 1.0;

	bool IsStructural() const { return Support.Strength > 0.0; }
};

USTRUCT()
struct GRIDLANDSCORE_API FGLItemStackDef
{
	GENERATED_BODY()

	UPROPERTY() FName Item;
	UPROPERTY() int32 Count = 0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLToolDef
{
	GENERATED_BODY()

	UPROPERTY() FName ToolClass;
	UPROPERTY() int32 Tier = 0;
};

/** Zenny can fight with this item (Pehlichi never deals damage, ADR-0017). Metres, seconds. */
USTRUCT()
struct GRIDLANDSCORE_API FGLWeaponDef
{
	GENERATED_BODY()

	UPROPERTY() double Damage = 0.0;
	UPROPERTY() double Reach = 0.0;
	UPROPERTY() double CooldownSeconds = 0.0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLItemDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	/** Inventory limit per stack (P11: slots and stacks are the limit; there is no carried weight). */
	UPROPERTY() int32 StackSize = 1;
	UPROPERTY() FName Material;
	/** Optional. Absent means the item is not a tool (ToolClass is None). */
	UPROPERTY() FGLToolDef Tool;
	/** Optional. Absent means it is not a weapon (Damage == 0). */
	UPROPERTY() FGLWeaponDef Weapon;
	UPROPERTY() bool CriticalPath = false;
	UPROPERTY() TArray<FName> Sources;
	UPROPERTY() TArray<FName> OnAcquireUnlocks;

	bool IsTool() const { return !Tool.ToolClass.IsNone(); }
};

USTRUCT()
struct GRIDLANDSCORE_API FGLKnowledgeDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Category;
	UPROPERTY() bool CriticalPath = false;
	UPROPERTY() TArray<FName> Sources;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLRecipeDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FGLItemStackDef Output;
	UPROPERTY() TArray<FGLItemStackDef> Inputs;
	UPROPERTY() FName Station;
	UPROPERTY() double CraftSeconds = 0.0;
	UPROPERTY() TArray<FName> UnlockedBy;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLSalvageYieldDef
{
	GENERATED_BODY()

	UPROPERTY() FName Item;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() FName YieldCategory;
};

/**
 * P11: salvage yields by recovery path. careful: dismantling (the most intact components); destructive: smashing
 * (fewer components, more scrap); collapse: salvaging debris (predominantly scrap). An empty path falls back to Yields
 * (trees: a felled trunk is gathered exactly as designed).
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLSalvagePathsDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<FGLSalvageYieldDef> Careful;
	UPROPERTY() TArray<FGLSalvageYieldDef> Destructive;
	UPROPERTY() TArray<FGLSalvageYieldDef> Collapse;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLToolEfficiencyDef
{
	GENERATED_BODY()

	UPROPERTY() FName ToolClass;
	UPROPERTY() int32 MinTier = 0;
	UPROPERTY() double Multiplier = 1.0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLSalvageDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() double Integrity = 0.0;
	UPROPERTY() FName Material;
	UPROPERTY() FName RequiresTool;
	/** What salvaging gives. P11: also the careful path when YieldsByPath has none. */
	UPROPERTY() TArray<FGLSalvageYieldDef> Yields;
	/** P11 (ADR-0039): how the material was recovered decides what comes back (distinct items, not stack quality). */
	UPROPERTY() FGLSalvagePathsDef YieldsByPath;
	UPROPERTY() TArray<FGLToolEfficiencyDef> ToolEfficiency;
	UPROPERTY() TArray<FName> OnSalvageUnlocks;
	/** Event.* tags emitted on completion, e.g. Event.Salvage.WireStripped. */
	UPROPERTY() TArray<FName> OnSalvageEvents;
	/** Optional (P6): the Noise.* action each hit makes (default Noise.Salvage.Hit; chopping a tree: Noise.Gather.Chop). */
	UPROPERTY() FName Noise;
};

/** A visual/collision box of a build piece, piece-local metres (ADR-0024). */
USTRUCT()
struct GRIDLANDSCORE_API FGLBuildShapeDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<double> Size;
	UPROPERTY() TArray<double> Offset;
	/** Degrees about the piece's local X axis; positive lifts +Y (roof slopes). */
	UPROPERTY() double Pitch = 0.0;
};

/** Where a piece connects: bottom meets top (resting), side meets side (lateral). */
USTRUCT()
struct GRIDLANDSCORE_API FGLBuildSocketDef
{
	GENERATED_BODY()

	UPROPERTY() FString Name;
	/** bottom | top | side */
	UPROPERTY() FName Role;
	UPROPERTY() TArray<double> Offset;
	/**
	 * P11: optional facing in degrees in the piece's frame (0 = +X), a multiple of 2.5 (SOCK-1). Two side sockets that
	 * both face must face each other to link, and a snap between them takes its yaw from the data (angled bays,
	 * octagons). Absent: GLStructureRules::NoFacing.
	 */
	UPROPERTY() double Facing = 1.0e6;
};

/** P11: a storage piece's capacity (shared base storage). */
USTRUCT()
struct GRIDLANDSCORE_API FGLStorageDef
{
	GENERATED_BODY()

	UPROPERTY() int32 Slots = 0;
};

/** One cube of NICE's corruption on a visual (P7): unnaturally precise, and sparse (VIS-2). Metres, degrees. */
USTRUCT()
struct GRIDLANDSCORE_API FGLCorruptionDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<double> Offset;
	UPROPERTY() double Size = 0.1;
	UPROPERTY() TArray<double> Rotation;
};

/** A light a visual carries (P7): signs, lamps. Metres; intensity in candela. */
USTRUCT()
struct GRIDLANDSCORE_API FGLVisualLightDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<double> Color;
	UPROPERTY() double Intensity = 0.0;
	UPROPERTY() double Radius = 0.0;
	UPROPERTY() TArray<double> Offset;
};

/** A mesh's level-of-detail budget (P8): the art pipeline builds LODs to it and every test checks it. */
USTRUCT()
struct GRIDLANDSCORE_API FGLVisualLodDef
{
	GENERATED_BODY()

	/** Maximum triangles per LOD, LOD0 first, strictly decreasing (VIS-3). */
	UPROPERTY() TArray<int32> MaxTriangles;
	/** Screen size at which each LOD starts: 1.0 for LOD0, strictly decreasing (VIS-3). */
	UPROPERTY() TArray<double> ScreenSize;
};

/**
 * How something looks (P7, the art pipeline's runtime contract): an imported mesh (Art/Source recipe
 * -> Tools/art.sh), a tint, whether it is outlined, sparse corruption cubes and an optional light.
 * Presentation only: collision, support and gameplay stay on the authoritative data (shapes, parts).
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLVisualDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	/** An imported mesh name under /Game/Gridlands/Art/Meshes (VIS-1). */
	UPROPERTY() FString Mesh;
	UPROPERTY() TArray<double> Tint;
	/** Dark graphic outline (the stylize post-process reads custom depth). Default true. */
	UPROPERTY() bool Outline = true;
	UPROPERTY() bool CastShadow = true;
	UPROPERTY() double Scale = 1.0;
	/** Mesh placement relative to its owner's origin (metres, degrees). */
	UPROPERTY() TArray<double> Offset;
	UPROPERTY() double Yaw = 0.0;
	UPROPERTY() TArray<FGLCorruptionDef> Corruption;
	UPROPERTY() FGLVisualLightDef Light;
	/** P8: presentation only; no gameplay rule may depend on a mesh or its LOD (ARCHITECTURAL-NORTH-STAR). */
	UPROPERTY() FGLVisualLodDef Lod;
	/** Metres beyond which it is not drawn (0: never culled by distance; required for instanced vegetation). */
	UPROPERTY() double CullDistance = 0.0;
};

/**
 * How a piece moves when it loses support (P6, deterministic collapse). Data, so new motions and
 * direction policies can be added without touching the structures that use them.
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLCollapseDef
{
	GENERATED_BODY()

	/** drop (falls straight down: slabs, floors) | topple (tips over about a base edge: walls, trees). Default drop. */
	UPROPERTY() FName Motion;
	/** topple only: awayFromInstigator (provisional default) | pieceForward | pieceBackward. */
	UPROPERTY() FName Direction;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLBuildPieceDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	/** Look and knowledge only; never power (ADR-0012, ERA-1). */
	UPROPERTY() FName Era;
	/** Physics and capability come from the material. */
	UPROPERTY() FName Material;
	/** foundation | wall | doorway | roof | post | beam | floor | storage | base_core | station ... */
	UPROPERTY() FName Role;
	/** May rest directly on terrain. */
	UPROPERTY() bool Grounded = false;
	/** Piece-space bounds, metres, origin at the bottom centre. */
	UPROPERTY() TArray<double> Size;
	UPROPERTY() TArray<FGLBuildShapeDef> Shapes;
	UPROPERTY() TArray<FGLBuildSocketDef> Sockets;
	UPROPERTY() TArray<FGLItemStackDef> Cost;
	UPROPERTY() TArray<FName> UnlockedBy;
	/** Optional (P6). false: a world-only piece (trees, authored structures): no cost, never offered to the player. Default true. */
	UPROPERTY() bool Buildable = true;
	/** Optional (P6): how it falls when unsupported (default: drop). */
	UPROPERTY() FGLCollapseDef Collapse;
	/** Optional (P7): how it looks (visual.*). Collision and support stay on the data shapes. */
	UPROPERTY() FName Visual;
	/**
	 * P11 (ADR-0039): the construction phases this form accepts after FRAME, in order (phase.*). Empty: complete as
	 * framed (a log wall, a Roman column: its form is its finish). A stud wall accepts [phase.electrical, phase.finish].
	 */
	UPROPERTY() TArray<FName> Layers;
	/** P11: the frame's look while it still waits for an accepted finish (studs, plates). Presentation only. */
	UPROPERTY() TArray<FGLBuildShapeDef> FrameShapes;
	/** P11: what dismantling, smashing or salvaging its debris gives back (salvage.*, by path). */
	UPROPERTY() FName Salvage;
	/** P11: a storage piece (shared base storage). */
	UPROPERTY() FGLStorageDef Storage;
	/** P11: a station it provides when built (Station.*). */
	UPROPERTY() FName Station;
	/** P11 save migration (v2 -> v3, MIG-1): layers a v0 piece of this id gets, so old saves keep their look. */
	UPROPERTY() TArray<FName> LegacyLayers;

	bool AcceptsPhase(FName Phase) const { return Layers.Contains(Phase); }
};

/**
 * P11 (ADR-0039): a construction phase after FRAME. The order is canonical: FRAME -> ELECTRICAL (optional) -> FINISH.
 * An unimplemented phase is registered so the schema never needs replacing; content for it is refused (PH-2).
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLPhaseDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() int32 Order = 0;
	UPROPERTY() bool Optional = true;
	UPROPERTY() bool Implemented = false;
};

/**
 * P11 (ADR-0039): a finish layer: real finishing material installed on a compatible frame. It changes appearance and
 * salvage, never support. Appearance = component form + material + finish; any finish fits any form whose role it
 * lists (no era-compatibility rules).
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLFinishDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	/** Look and knowledge only (ERA-1). */
	UPROPERTY() FName Era;
	UPROPERTY() FName Phase;
	UPROPERTY() TArray<FName> FitsRoles;
	UPROPERTY() TArray<FGLItemStackDef> Cost;
	UPROPERTY() TArray<FName> UnlockedBy;
	/** Optional: an imported look; otherwise the form's shapes painted with Tint. */
	UPROPERTY() FName Visual;
	UPROPERTY() TArray<double> Tint;
	/** What stripping it (careful) or losing it with its frame gives back. */
	UPROPERTY() FName Salvage;
};

/** One part of an authored structure (P6): a piece in the shared structural language, placed in structure space. */
USTRUCT()
struct GRIDLANDSCORE_API FGLStructurePartDef
{
	GENERATED_BODY()

	/** Unique within the structure; saves refer to parts by it. */
	UPROPERTY() FName Name;
	UPROPERTY() FName Piece;
	/** Structure-local metres (the structure's origin is its placement point). */
	UPROPERTY() TArray<double> Location;
	/** P11: degrees, a whole number of 2.5 degree steps (YAW-1). */
	UPROPERTY() double Yaw = 0.0;
	/** What salvaging it takes and gives (the same salvage pipeline as every other salvage). */
	UPROPERTY() FName Salvage;
	/** Optional: overrides the piece's collapse motion for this part. */
	UPROPERTY() FGLCollapseDef Collapse;
};

/**
 * An authored world structure (P6): the canonical runtime and persistence contract for salvageable
 * buildings and natural resources such as trees. Support is derived by GLStructureRules, exactly as
 * for player building. Future editor tooling produces this same data (authoring is not this file).
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLStructureDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() TArray<FGLStructurePartDef> Parts;
};

/** Provisional physical tuning (P6): collapse. Metres, seconds; never tuned by feel. */
USTRUCT()
struct GRIDLANDSCORE_API FGLCollapseTuningDef
{
	GENERATED_BODY()

	UPROPERTY() double Gravity = 9.81;
	/** Seconds between losing support and starting to move. */
	UPROPERTY() double StartDelaySeconds = 0.3;
	/** Metres the impact volume extends beyond a piece's footprint. */
	UPROPERTY() double ImpactMarginMetres = 0.3;
	/** A toppled piece's impact volume height above the ground it lands on. */
	UPROPERTY() double ImpactHeightMetres = 2.0;
	UPROPERTY() double DamageBase = 0.0;
	UPROPERTY() double DamagePerMetreFallen = 0.0;
	UPROPERTY() double DamageMax = 0.0;
	/** Starting tilt of a topple (a perfectly upright piece would never start). */
	UPROPERTY() double ToppleStartDegrees = 5.0;
	/**
	 * P10 (ADR-0038): the physical severity (metres fallen x the material's impactScale) at which an impact pins a
	 * creature whose definition lists Neutralize.Pinned. Gameplay tuning, not an architectural constant.
	 */
	UPROPERTY() double PinMinSeverity = 0.0;
};

/** Provisional building tuning (P11). Behaviour of P11, not architecture: claims may grow, connect and multiply later. */
USTRUCT()
struct GRIDLANDSCORE_API FGLBuildingTuningDef
{
	GENERATED_BODY()

	/** Radius of the area one base core claims (P11: 32 m, provisional). */
	UPROPERTY() double ClaimRadiusMetres = 32.0;
	/** Personal inventory slots (P11: 32, provisional). */
	UPROPERTY() int32 PersonalSlots = 32;
};

/** Provisional noise tuning (P6): one authoritative world-noise model. */
USTRUCT()
struct GRIDLANDSCORE_API FGLNoiseTuningDef
{
	GENERATED_BODY()

	/** Seconds a creature investigates a noise it heard. */
	UPROPERTY() double InvestigateSeconds = 0.0;
	/** Seconds a creature searches Zenny's last known position after losing sight (creature data may override). */
	UPROPERTY() double MemorySeconds = 0.0;
	/** Noise.* action -> radius in metres (before the material's noiseScale). */
	UPROPERTY() TMap<FString, double> Radius;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLTuningDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FGLCollapseTuningDef Collapse;
	UPROPERTY() FGLNoiseTuningDef Noise;
	UPROPERTY() FGLBuildingTuningDef Building;
};

/** One stroke of a terraforming tool (ADR-0022). Metres. */
USTRUCT()
struct GRIDLANDSCORE_API FGLTerraformDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	/** DIG | RAISE | FLATTEN */
	UPROPERTY() FName Op;
	UPROPERTY() double Radius = 0.0;
	UPROPERTY() double Amount = 0.0;
	UPROPERTY() FName RequiresTool;
	UPROPERTY() TArray<FGLItemStackDef> Cost;
	UPROPERTY() TArray<FGLItemStackDef> Yields;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCapabilityEffectDef
{
	GENERATED_BODY()

	/** One of the non-damaging effect kinds in schema.py CAPABILITY_EFFECTS (ADR-0017). */
	UPROPERTY() FName Kind;
	UPROPERTY() double Value = 0.0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCapabilityLevelDef
{
	GENERATED_BODY()

	UPROPERTY() int32 Level = 0;
	UPROPERTY() TArray<FGLCapabilityEffectDef> Effects;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCapabilityDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Owner;
	UPROPERTY() FName Category;
	UPROPERTY() TArray<FGLCapabilityLevelDef> Levels;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLGlitchDetectionDef
{
	GENERATED_BODY()

	UPROPERTY() FName Capability;
	UPROPERTY() int32 MinLevel = 0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLGlitchRequirementDef
{
	GENERATED_BODY()

	/** Binding key used by placements (ADR-0018). */
	UPROPERTY() FString Name;
	UPROPERTY() FName Kind;
	UPROPERTY() FName Item;
	UPROPERTY() int32 Count = 0;
	/** For Requirement.PuzzleSolved. */
	UPROPERTY() FName Puzzle;
};

UENUM()
enum class EGLInterruptPolicy : uint8
{
	KeepProgress,
	ResetProgress,
};

USTRUCT()
struct GRIDLANDSCORE_API FGLGlitchRepairDef
{
	GENERATED_BODY()

	UPROPERTY() double Seconds = 0.0;
	UPROPERTY() EGLInterruptPolicy InterruptPolicy = EGLInterruptPolicy::KeepProgress;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLGlitchRewardDef
{
	GENERATED_BODY()

	/** Pehlichi | Zenny */
	UPROPERTY() FName Recipient;
	/** Exactly one of Capability, Item, Knowledge is set. */
	UPROPERTY() FName Capability;
	UPROPERTY() int32 Delta = 0;
	UPROPERTY() FName Item;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() FName YieldCategory;
	UPROPERTY() FName Knowledge;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLGlitchDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FGLGlitchDetectionDef Detection;
	UPROPERTY() TArray<FGLGlitchRequirementDef> Requirements;
	UPROPERTY() FGLGlitchRepairDef Repair;
	UPROPERTY() double StabilityWeight = 0.0;
	/** Metres; 0 when absent (the game uses 40). */
	UPROPERTY() double InfluenceRadius = 0.0;
	UPROPERTY() TArray<FGLGlitchRewardDef> Rewards;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCellCoordDef
{
	GENERATED_BODY()

	UPROPERTY() int32 X = 0;
	UPROPERTY() int32 Y = 0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLEraWeightDef
{
	GENERATED_BODY()

	UPROPERTY() FName Era;
	UPROPERTY() double Weight = 0.0;
};

/** Authored relief for a cell's ground (GLTerrainGen), flat near the cell edges. Metres. */
USTRUCT()
struct GRIDLANDSCORE_API FGLCellReliefDef
{
	GENERATED_BODY()

	UPROPERTY() int32 Seed = 0;
	UPROPERTY() double AmplitudeMetres = 0.0;
	UPROPERTY() double EdgeBlendMetres = 1.0;
};

/** A cell's runtime heightfield ground (ADR-0022). Metres. */
USTRUCT()
struct GRIDLANDSCORE_API FGLCellTerrainDef
{
	GENERATED_BODY()

	UPROPERTY() int32 ChunkMetres = 0;
	UPROPERTY() double SpacingMetres = 1.0;
	UPROPERTY() double BaseHeight = 0.0;
	UPROPERTY() double MaxDigDepth = 0.0;
	UPROPERTY() double MaxRaiseHeight = 0.0;
	/** Absent (AmplitudeMetres == 0) means flat at BaseHeight. */
	UPROPERTY() FGLCellReliefDef Relief;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCellDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Band;
	UPROPERTY() FGLCellCoordDef Coord;
	UPROPERTY() TArray<FGLEraWeightDef> EraComposition;
	UPROPERTY() FString Level;
	/** Metres; 0 when absent (no limit). */
	UPROPERTY() double PlayableHalfExtent = 0.0;
	/** Grid pitch, metres (GRID-1: the same for every cell). */
	UPROPERTY() double SizeMetres = 0.0;
	/** Local static intensity added to the band's baseline (independent of depth and era). */
	UPROPERTY() double InterferenceOffset = 0.0;
	/** Absent (ChunkMetres == 0) means the cell has no runtime ground. */
	UPROPERTY() FGLCellTerrainDef Terrain;

	/** World-space centre of the cell (cm). */
	FVector2D CentreCm() const { return FVector2D(Coord.X, Coord.Y) * SizeMetres * 100.0; }
	/** True when a world point (cm) lies in this cell (edges belong to the lower cell). */
	bool ContainsCm(const FVector2D& P) const
	{
		const double Half = SizeMetres * 50.0;
		const FVector2D L = P - CentreCm();
		return L.X >= -Half && L.X < Half && L.Y >= -Half && L.Y < Half;
	}

	bool HasTerrain() const { return Terrain.ChunkMetres > 0; }
};

USTRUCT()
struct GRIDLANDSCORE_API FGLPlacementTransformDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<double> Location;
	UPROPERTY() double Yaw = 0.0;
};

/** P9: one point of a patrol loop (cell-local cm, like a transform's location). */
USTRUCT()
struct GRIDLANDSCORE_API FGLPlacementPointDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<double> Location;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLPlacementDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	/** glitch | salvage_node | spawn | patrol | discovery | encounter */
	UPROPERTY() FName Kind;
	UPROPERTY() FName Definition;
	/** Exactly one of Anchor or Transform (ADR-0018). */
	UPROPERTY() FName Anchor;
	UPROPERTY() TArray<double> Offset;
	UPROPERTY() FGLPlacementTransformDef Transform;
	/** Glitch requirement name -> target placement id. */
	UPROPERTY() TMap<FString, FName> Bindings;
	/** Discovery: metres within which Zenny finds it (0 = default). */
	UPROPERTY() double Radius = 0.0;
	/** scatter (P7): how many instances within Radius. */
	UPROPERTY() int32 Count = 0;
	/** spawn (P9): the patrol loop (2+ points; empty = the creature guards its home). */
	UPROPERTY() TArray<FGLPlacementPointDef> Patrol;

	bool IsAnchored() const { return !Anchor.IsNone(); }
};

USTRUCT()
struct GRIDLANDSCORE_API FGLPuzzleAnswerDef
{
	GENERATED_BODY()

	/** PRESENT | MANIPULATE | PERFORM (ADR-0023; CONSTRUCT reserved). Never a dialogue or text answer. */
	UPROPERTY() FName Mode;
	UPROPERTY() FName Item;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLPuzzleDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Family;
	UPROPERTY() FGLPuzzleAnswerDef Answer;
	UPROPERTY() FName PoseExchange;
};

UENUM()
enum class EGLExchangeCategory : uint8
{
	StoryCritical,
	Contextual,
	Ambient,
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCreaturePerceptionDef
{
	GENERATED_BODY()

	UPROPERTY() double SightRadius = 0.0;
	UPROPERTY() double ConeDegrees = 0.0;
	UPROPERTY() double HearingRadius = 0.0;
	/** Optional (P6): seconds it keeps searching Zenny's last known position after losing sight (0: tuning default). */
	UPROPERTY() double MemorySeconds = 0.0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCreatureAttackDef
{
	GENERATED_BODY()

	UPROPERTY() double Damage = 0.0;
	UPROPERTY() double Reach = 0.0;
	UPROPERTY() double CooldownSeconds = 0.0;
};

/** P9: what resolving an encounter target grants (once, whichever way it was resolved). */
USTRUCT()
struct GRIDLANDSCORE_API FGLCreatureEncounterDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<FGLSalvageYieldDef> Rewards;
};

/** A corrupted creature (M11). Exists only through placements (ADR-0014, CR-2). Metres, seconds. */
USTRUCT()
struct GRIDLANDSCORE_API FGLCreatureDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() double Health = 0.0;
	UPROPERTY() double WalkSpeed = 0.0;
	UPROPERTY() double ChaseSpeed = 0.0;
	UPROPERTY() FGLCreaturePerceptionDef Perception;
	UPROPERTY() FGLCreatureAttackDef Attack;
	UPROPERTY() double LeashRadius = 0.0;
	UPROPERTY() TArray<FGLSalvageYieldDef> Drops;
	/** Optional (P7): how it looks (visual.*). */
	UPROPERTY() FName Visual;
	/** P9: the non-damage outcomes (Neutralize.*) that can take it out of an encounter; empty = immune. */
	UPROPERTY() TArray<FName> NeutralizableBy;
	/** P9: an encounter target: resolving it (defeated or neutralized alike) grants these once. */
	UPROPERTY() FGLCreatureEncounterDef Encounter;

	bool IsEncounter() const { return Encounter.Rewards.Num() > 0; }
};

/** P9: how a mechanism is switched (one operation), by whom and how long it takes. */
USTRUCT()
struct GRIDLANDSCORE_API FGLMechanismOperateDef
{
	GENERATED_BODY()

	UPROPERTY() FString From;
	UPROPERTY() FString To;
	UPROPERTY() FName Capability;
	UPROPERTY() int32 MinLevel = 1;
	UPROPERTY() double Seconds = 0.0;
	/** Metres from the control Pehlichi must be (default 1.5). */
	UPROPERTY() double Reach = 0.0;
};

/** P9: entering InState decides, at that moment, the outcome of every susceptible creature in the box. */
USTRUCT()
struct GRIDLANDSCORE_API FGLMechanismNeutralizeDef
{
	GENERATED_BODY()

	UPROPERTY() FString InState;
	UPROPERTY() FName Tag;
	/** Placement-local metres (box centre) and half-extents. */
	UPROPERTY() TArray<double> Offset;
	UPROPERTY() TArray<double> Extent;
	/** Presentation: how long the visible drop takes (the outcome is already decided). */
	UPROPERTY() double PresentSeconds = 0.0;
};

/** P9: an ambient sound that masks other noise at listeners inside its radius (while active). */
USTRUCT()
struct GRIDLANDSCORE_API FGLMechanismAmbientDef
{
	GENERATED_BODY()

	UPROPERTY() TArray<FString> ActiveIn;
	UPROPERTY() double Radius = 0.0;
	UPROPERTY() double Mask = 0.0;
	UPROPERTY() TArray<double> Offset;
	/** Optional deterministic duty cycle on the world clock (0 = always on while active). */
	UPROPERTY() double Period = 0.0;
	UPROPERTY() double OnSeconds = 0.0;
	UPROPERTY() double Phase = 0.0;
};

/** P9: a model-first environmental mechanism (ADR-0037). Narrow on purpose: states, one operation, effects. */
USTRUCT()
struct GRIDLANDSCORE_API FGLMechanismDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() TArray<FString> States;
	UPROPERTY() FString Initial;
	UPROPERTY() FGLMechanismOperateDef Operate;
	UPROPERTY() FGLMechanismNeutralizeDef Neutralize;
	UPROPERTY() FGLMechanismAmbientDef Ambient;
	/** State name -> the Noise.* action made on entering it. */
	UPROPERTY() TMap<FString, FName> EnterNoise;

	bool CanOperate() const { return !Operate.Capability.IsNone(); }
	bool HasNeutralize() const { return !Neutralize.Tag.IsNone(); }
	bool HasAmbient() const { return Ambient.Radius > 0.0; }
};

/** P9 (ADR-0029 as amended): an authored navigation region for a bounded encounter space. */
USTRUCT()
struct GRIDLANDSCORE_API FGLNavRegionDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	/** Half-extents, metres. */
	UPROPERTY() TArray<double> Extent;
	/** Metres outside the region within which Zenny makes it relevant (0: 32 m). */
	UPROPERTY() double RelevanceMargin = 0.0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLStormTriggerDef
{
	GENERATED_BODY()

	UPROPERTY() FName EventCount;
	UPROPERTY() int32 Min = 0;
};

/** A bounded Glitch Storm NICE sets off (M11). Metres, seconds. */
USTRUCT()
struct GRIDLANDSCORE_API FGLStormDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() double DurationSeconds = 0.0;
	UPROPERTY() double Radius = 0.0;
	UPROPERTY() double SpawnPerSecond = 0.0;
	UPROPERTY() int32 MaxArtifacts = 0;
	/** cat | dog */
	UPROPERTY() TArray<FName> Artifacts;
	UPROPERTY() FGLStormTriggerDef Trigger;
	UPROPERTY() bool Harmless = true;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLExchangeLineDef
{
	GENERATED_BODY()

	/** NICE | Pehlichi. Zenny is silent (DLG-1). */
	UPROPERTY() FName Speaker;
	UPROPERTY() FString Text;
	/** Optional voice (P4 hook): a sound asset path. Its length sets the subtitle duration. */
	UPROPERTY() FString Voice;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLExchangeRequirementDef
{
	GENERATED_BODY()

	/** Event.* tag whose occurrences (including child tags) are counted. */
	UPROPERTY() FName EventCount;
	UPROPERTY() int32 Min = 0;
	/** 0 when absent: no upper bound. */
	UPROPERTY() int32 Max = 0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLExchangeDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() TArray<FName> Trigger;
	UPROPERTY() EGLExchangeCategory Category = EGLExchangeCategory::Ambient;
	UPROPERTY() int32 Priority = 0;
	UPROPERTY() double CooldownSeconds = 0.0;
	/** 0 when absent: unlimited. */
	UPROPERTY() int32 MaxUses = 0;
	UPROPERTY() double Weight = 1.0;
	/** Content id the event must be about; None matches any subject. */
	UPROPERTY() FName Subject;
	UPROPERTY() TArray<FGLExchangeRequirementDef> Requires;
	UPROPERTY() TArray<FGLExchangeLineDef> Lines;
};
