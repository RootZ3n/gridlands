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

USTRUCT()
struct GRIDLANDSCORE_API FGLBuildPieceDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	/** Look and knowledge only; never power (ADR-0012, ERA-1). */
	UPROPERTY() FName Era;
	/** Physics and capability come from the material. */
	UPROPERTY() FName Material;
	UPROPERTY() TArray<FString> Sockets;
	UPROPERTY() TArray<FGLItemStackDef> Cost;
	UPROPERTY() TArray<FName> UnlockedBy;
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

USTRUCT()
struct GRIDLANDSCORE_API FGLCellDef : public FGLDefinitionBase
{
	GENERATED_BODY()

	UPROPERTY() FString DisplayName;
	UPROPERTY() FName Band;
	UPROPERTY() FGLCellCoordDef Coord;
	UPROPERTY() TArray<FGLEraWeightDef> EraComposition;
	UPROPERTY() FString Level;
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

	bool IsAnchored() const { return !Anchor.IsNone(); }
};

UENUM()
enum class EGLExchangeCategory : uint8
{
	StoryCritical,
	Contextual,
	Ambient,
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
	UPROPERTY() TArray<FGLExchangeLineDef> Lines;
};
