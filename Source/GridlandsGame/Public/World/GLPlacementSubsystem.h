#pragma once

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

	/** Spawns every supported placement of CellId. Returns how many actors were spawned. */
	int32 SpawnCell(FName CellId);

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
	AGLCreature* SpawnProofCreature(FName Def, const FVector& Location, double Yaw, FName Cell);
#endif
	AGLCreature* FindCreature(FName PlacementId) const { const TWeakObjectPtr<AGLCreature>* Found = Creatures.Find(PlacementId); return Found ? Found->Get() : nullptr; }
	const TMap<FName, TWeakObjectPtr<AGLCreature>>& GetCreatures() const { return Creatures; }
	const TArray<FGLDiscoverySite>& GetDiscoveries() const { return Discoveries; }

private:
	TMap<FName, TWeakObjectPtr<AGLSalvageNode>> SalvageNodes;
	TMap<FName, TWeakObjectPtr<AGLCreature>> Creatures;
	TArray<FGLDiscoverySite> Discoveries;
	TSet<FName> SpawnedCells;
	TMap<FName, TArray<TWeakObjectPtr<AActor>>> CellActors;
};
