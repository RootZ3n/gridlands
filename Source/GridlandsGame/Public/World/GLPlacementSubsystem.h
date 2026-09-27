#pragma once

#include "Content/GLContentDefinitions.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLPlacementSubsystem.generated.h"

class AGLCreature;
class AGLGlitch;
class AGLSalvageNode;

/** A place Zenny can find (discovery placement): knowledge learned on arrival. */
struct FGLDiscoverySite
{
	FName Placement;
	FName Knowledge;
	FVector Location = FVector::ZeroVector;
	double RadiusCm = 800.0;
	FName Cell;
};

/**
 * P8: the authoritative model of an actor-state placement: a salvage node or a creature (a glitch's
 * lives in UGLGlitchSubsystem). Made with its cell's gameplay layer, before any actor, and it holds
 * what saves read and restore. The actor is presentation made from it: never made salvaged or defeated.
 */
struct FGLActorPlacement
{
	FName Placement;
	FName Kind;       // salvage_node | spawn
	FName Definition; // salvage.* | creature.*
	FName Cell;
	FVector Location = FVector::ZeroVector;
	double Yaw = 0.0;
	/** The authored level actor an anchored salvage node stands for (a fence, a wall): hidden once salvaged. */
	TWeakObjectPtr<AActor> LinkedVisual;
	bool bSalvaged = false;
	bool bDefeated = false;
	TWeakObjectPtr<AActor> Actor;
};

/** A gameplay actor waiting to be made from its model (P8): a glitch, salvage node or creature. */
struct FGLPendingActor
{
	FName Cell;
	FName Placement;
	FName Kind;
	FVector Location = FVector::ZeroVector;
};

/** Vegetation waiting to be presented (P7 multi-frame presentation). */
struct FGLPendingScatter
{
	FName Cell;
	FName Placement;
	FName Visual;
	FVector Location = FVector::ZeroVector;
	double RadiusCm = 0.0;
	int32 Count = 0;
};

/**
 * Spawns a cell's gameplay layer from Data/placement/<cell>/ (ADR-0018). Anchored placements
 * sit at their anchor (from the map's actor if loaded, else the exported anchor record) plus offset.
 * Spawns salvage nodes (M4), glitches (M7), puzzle sites, creatures (M11; the only way creatures
 * enter the world, ADR-0014) and records discovery sites (M11).
 */
UCLASS()
class GRIDLANDSGAME_API UGLPlacementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * Spawns every supported placement of CellId. Returns how many placements were made.
	 * Every placement's authoritative model is made now: structures (UGLStructureSubsystem), glitches
	 * (UGLGlitchSubsystem records), salvage nodes and creatures (FGLActorPlacement, P8), discovery sites.
	 * bDeferPresentation (P7/P8, grid streaming): no presentation actor is made now. Structure parts,
	 * glitches, salvage nodes, creatures and vegetation are queued, and PumpPresentation makes each over
	 * the following frames from its model as it is then (the cell's saved state is applied to the models
	 * first, in the same frame). Puzzle sites (stateless) are always made now.
	 */
	int32 SpawnCell(FName CellId, bool bDeferPresentation = false);
	/** Instantiates deferred presentation, nearest to Where first: everything within NearCm now, the rest within BudgetSeconds (0: everything; < 0: only the near ones). */
	int32 PumpPresentation(const FVector& Where, double BudgetSeconds, double NearCm = 0.0);
	/** No presentation of CellId is still waiting. */
	bool IsCellPresented(FName CellId) const;
	/** Presentation units still waiting, all cells (evidence). */
	int32 PendingPresentation() const;

	/**
	 * Removes everything SpawnCell made for CellId (P3 streaming): the models go at once; gameplay actors
	 * are made inert at once (out of every query, no collision, hidden) and destroyed within the
	 * presentation budget (P8, as structure parts and vegetation). Returns how many actors.
	 */
	int32 DespawnCell(FName CellId);
	bool IsCellSpawned(FName CellId) const { return SpawnedCells.Contains(CellId); }
	const TSet<FName>& GetSpawnedCells() const { return SpawnedCells; }
	/** Placement ids name their cell: placement.<cell short name>.* (ID-10). */
	static bool IsPlacementOfCell(FName PlacementId, FName CellId);

	/** The presented node of a placement (null while it waits, once salvaged away, or when unknown). */
	AGLSalvageNode* FindSalvageNode(FName PlacementId) const;
	/** P8 model queries: true only for a loaded placement whose model says so. */
	bool IsSalvaged(FName PlacementId) const { const FGLActorPlacement* M = ActorModels.Find(PlacementId); return M && M->bSalvaged; }
	bool IsCreatureDefeated(FName PlacementId) const { const FGLActorPlacement* M = ActorModels.Find(PlacementId); return M && M->bDefeated; }
	const FGLActorPlacement* FindActorModel(FName PlacementId) const { return ActorModels.Find(PlacementId); }
	const TMap<FName, FGLActorPlacement>& GetActorModels() const { return ActorModels; }
	/** A saved fact onto the model (and its actor, if presented), silently. False if the placement is not loaded. */
	bool RestoreSalvaged(FName PlacementId);
	bool RestoreDefeated(FName PlacementId);
	/** A presented creature was defeated (its model records it). */
	void MarkDefeated(FName PlacementId);
	/** Makes a waiting gameplay actor now by the pump's own path (tests choose the order). False if it was not waiting. */
	bool PresentActor(FName PlacementId);
	bool IsActorPending(FName PlacementId) const { return PendingActors.ContainsByPredicate([PlacementId](const FGLPendingActor& P) { return P.Placement == PlacementId; }); }
	/** Gameplay actors of unloaded cells still waiting to be destroyed (retired: inert, hidden). */
	int32 RetiringActorCount() const { return RetiringActors.Num(); }
#if !UE_BUILD_SHIPPING
	/**
	 * DEV ONLY (P6 real-game proofs): a creature of Def at Location, set up exactly like a placed one
	 * but not saved (it belongs to no placement) and removed with its cell. Never gameplay: threat
	 * still comes only from placements (ADR-0014).
	 */
	AGLCreature* SpawnProofCreature(FName Def, const FVector& Location, double Yaw, FName Cell, FName VisualOverride = NAME_None, bool bPosed = false);
	/**
	 * DEV ONLY (P7 dense-spawn proof): an in-memory placement of CellId, spawned (and saved, by its id)
	 * exactly like an authored one while registered. Never shipping content. Register before the cell
	 * streams in, and register the same set on every launch that loads a save made with it.
	 */
	void AddProofPlacement(FName CellId, FName Id, const FGLPlacementDef& Placement);
	/** The dense authored stress fixture (GLDenseProof.h) in the diner lots; -GLDenseProof enables it. Returns its placement count. */
	int32 AddDenseProof();
	/** The P8 production-density town block (GLTownBlock.cpp) on the crossing route in the lots; -GLTownBlock enables it. Returns its placement count. */
	int32 AddTownBlock();
	/** The id of the town block's Index-th placement (its layout order, GLTownBlockSites.inl). */
	static FName TownBlockId(int32 Index);
	void ClearProofPlacements() { ProofPlacements.Reset(); }
#endif
	/** The presented creature of a placement (null while it waits, once defeated before it was made, or when unknown). */
	AGLCreature* FindCreature(FName PlacementId) const;
	const TArray<FGLDiscoverySite>& GetDiscoveries() const { return Discoveries; }

private:
	bool SpawnPlacement(FName CellId, FName Id, const FGLPlacementDef& Placement, bool bDeferPresentation);
	bool SpawnScatter(const FGLPendingScatter& Scatter);
	/** Makes a gameplay actor from its model now (P8). False if there is nothing to make (salvaged, defeated, gone, or already made). */
	bool MakeActor(const FGLPendingActor& Pending);
	void Retire(AActor* Actor);

	TArray<FGLPendingScatter> PendingScatter;
	TArray<TWeakObjectPtr<AActor>> RetiringScatter;
#if !UE_BUILD_SHIPPING
	TMap<FName, TArray<TPair<FName, FGLPlacementDef>>> ProofPlacements;
#endif
	TMap<FName, FGLActorPlacement> ActorModels;
	TArray<FGLPendingActor> PendingActors;
	TArray<TWeakObjectPtr<AActor>> RetiringActors;
	TArray<FGLDiscoverySite> Discoveries;
	TSet<FName> SpawnedCells;
	TMap<FName, TArray<TWeakObjectPtr<AActor>>> CellActors;
};
