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
	UPROPERTY() int32 StackSize = 1;
	UPROPERTY() double Weight = 0.0;
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
	UPROPERTY() TArray<FGLSalvageYieldDef> Yields;
	UPROPERTY() TArray<FGLToolEfficiencyDef> ToolEfficiency;
	UPROPERTY() TArray<FName> OnSalvageUnlocks;
	/** Event.* tags emitted on completion, e.g. Event.Salvage.WireStripped. */
	UPROPERTY() TArray<FName> OnSalvageEvents;
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
	/** foundation | wall | doorway | roof | post | beam */
	UPROPERTY() FName Role;
	/** May rest directly on terrain. */
	UPROPERTY() bool Grounded = false;
	/** Piece-space bounds, metres, origin at the bottom centre. */
	UPROPERTY() TArray<double> Size;
	UPROPERTY() TArray<FGLBuildShapeDef> Shapes;
	UPROPERTY() TArray<FGLBuildSocketDef> Sockets;
	UPROPERTY() TArray<FGLItemStackDef> Cost;
	UPROPERTY() TArray<FName> UnlockedBy;
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
};

USTRUCT()
struct GRIDLANDSCORE_API FGLCreatureAttackDef
{
	GENERATED_BODY()

	UPROPERTY() double Damage = 0.0;
	UPROPERTY() double Reach = 0.0;
	UPROPERTY() double CooldownSeconds = 0.0;
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
