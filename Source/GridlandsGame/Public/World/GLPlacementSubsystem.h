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
	 * bDeferPresentation (P7, grid streaming): structures get only their authoritative model now
	 * (UGLStructureSubsystem) and vegetation is queued; PumpPresentation instantiates both over the
	 * following frames, each in its already-resolved final state. Gameplay actors whose state lives on
	 * the actor itself (creatures, glitches, salvage nodes, puzzle sites) are always made now, so the
	 * cell's saved state can be applied to them in this same frame.
	 */
	int32 SpawnCell(FName CellId, bool bDeferPresentation = false);
	/** Instantiates deferred presentation, nearest to Where first: everything within NearCm now, the rest within BudgetSeconds (0: everything; < 0: only the near ones). */
	int32 PumpPresentation(const FVector& Where, double BudgetSeconds, double NearCm = 0.0);
	/** No presentation of CellId is still waiting. */
	bool IsCellPresented(FName CellId) const;
	/** Presentation units still waiting, all cells (evidence). */
	int32 PendingPresentation() const;

	/** Destroys everything SpawnCell made for CellId (P3 streaming). Returns how many actors. */
	int32 DespawnCell(FName CellId);
	bool IsCellSpawned(FName CellId) const { return SpawnedCells.Contains(CellId); }
	const TSet<FName>& GetSpawnedCells() const { return SpawnedCells; }
	/** Placement ids name their cell: placement.<cell short name>.* (ID-10). */
	static bool IsPlacementOfCell(FName PlacementId, FName CellId);

	AGLSalvageNode* FindSalvageNode(FName PlacementId) const;
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
	void ClearProofPlacements() { ProofPlacements.Reset(); }
#endif
	AGLCreature* FindCreature(FName PlacementId) const { const TWeakObjectPtr<AGLCreature>* Found = Creatures.Find(PlacementId); return Found ? Found->Get() : nullptr; }
	const TMap<FName, TWeakObjectPtr<AGLCreature>>& GetCreatures() const { return Creatures; }
	const TArray<FGLDiscoverySite>& GetDiscoveries() const { return Discoveries; }

private:
	bool SpawnPlacement(FName CellId, FName Id, const FGLPlacementDef& Placement, bool bDeferPresentation);
	bool SpawnScatter(const FGLPendingScatter& Scatter);

	TArray<FGLPendingScatter> PendingScatter;
	TArray<TWeakObjectPtr<AActor>> RetiringScatter;
#if !UE_BUILD_SHIPPING
	TMap<FName, TArray<TPair<FName, FGLPlacementDef>>> ProofPlacements;
#endif
	TMap<FName, TWeakObjectPtr<AGLSalvageNode>> SalvageNodes;
	TMap<FName, TWeakObjectPtr<AGLCreature>> Creatures;
	TArray<FGLDiscoverySite> Discoveries;
	TSet<FName> SpawnedCells;
	TMap<FName, TArray<TWeakObjectPtr<AActor>>> CellActors;
};
