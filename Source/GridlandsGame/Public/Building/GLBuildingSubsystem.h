#pragma once

#include "Building/GLClaimRules.h"
#include "Building/GLConstructionRules.h"
#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"
#include "Inventory/GLMaterialPool.h"
#include "Save/GLWorldSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLBuildingSubsystem.generated.h"

class AGLBuildPiece;
class UGLInventoryComponent;

/**
 * Where one building or crafting operation may take materials from and put them (P11, shared base storage, ADR-0039).
 * Inside a claim, with Zenny inside it too: the claim's eligible storage pieces (player-built, intact, storage-capable)
 * ordered by distance from the operation's point, then by piece id, and then Zenny's own inventory; delivery fills
 * Zenny's inventory first, then the same storage. Elsewhere: Zenny's inventory alone. Never storage across the world.
 */
struct GRIDLANDSGAME_API FGLMaterialSources
{
	TArray<FGLInventory*> Inventories;
	TArray<int32> DeliverOrder;
	/** Storage piece ids in consumption order (evidence and tests). */
	TArray<int32> Containers;
	FName Claim;
	UGLInventoryComponent* Personal = nullptr;

	FGLMaterialPool Pool() const { return FGLMaterialPool(Inventories, DeliverOrder); }
	/** Event.Item.Acquired for what an operation delivered (the pool fills inventories directly). */
	void AnnounceAcquired(AActor* Who, const TMap<FName, int32>& Items) const;
};

/** P12 (ADR-0040): one item of a cost display: the commit's own sources and its own consumption plan. */
struct GRIDLANDSGAME_API FGLCostLine
{
	FName Item;
	int32 Needed = 0;
	/** What the eligible base storage holds, and what Zenny carries. */
	int32 InStorage = 0;
	int32 Personal = 0;
	/** What the commit would take from each (storage before Zenny inside a base). */
	int32 FromStorage = 0;
	int32 FromPersonal = 0;
};

struct GRIDLANDSGAME_API FGLCostView
{
	TArray<FGLCostLine> Lines;
	bool bAffordable = false;
	/** The claim whose storage supplies it (None: Zenny's pockets only). */
	FName Claim;
	TArray<int32> Containers;
};

/** Why a removal was refused (nothing changed). */
enum class EGLDemolishRefusal : uint8 { None, UnknownPiece, NoRoomForRefund, StorageNotEmpty };

struct GRIDLANDSGAME_API FGLDemolishResult
{
	EGLDemolishRefusal Refusal = EGLDemolishRefusal::None;
	EGLSalvagePath Path = EGLSalvagePath::Careful;
	/** The removal preview made just before committing (GLStructureRules::CollapsesAfterRemoving). */
	TArray<int32> Predicted;
	/** What actually lost support and collapsed by the canonical rules (debris; impact at impact time). */
	TArray<int32> Collapsed;
	/** What the removed piece gave back by its path (after world settings). */
	TMap<FName, int32> Recovered;

	bool IsDone() const { return Refusal == EGLDemolishRefusal::None; }
};

/**
 * Building v1 (ADR-0024 generalized by ADR-0039 in P11). Player construction lives in the canonical structural model
 * (UGLStructureSubsystem, one player structure per cell): the same support, collapse, debris and persistence as authored
 * structures. This subsystem owns the player's verbs as transactions: place (FRAME), install a finish, dismantle
 * (careful) and smash (destructive). Every preview is the commit's own rule; every cost and return is all-or-nothing
 * against the operation's material sources. Emits Event.Building.*.
 */
UCLASS()
class GRIDLANDSGAME_API UGLBuildingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Checks without changing anything (the ghost preview uses this: Preview is GREEN / YELLOW / RED). */
	FGLBuildCheck Check(const AActor* Builder, const FGLPlacedPiece& Candidate) const;
	/** Pays the cost (base storage first inside a claim) and places the FRAME, or refuses with nothing changed. */
	FGLBuildCheck Place(AActor* Builder, const FGLPlacedPiece& Candidate);
	/** The removal preview: every piece that would lose support without PieceId (the commit uses the same function). */
	TArray<int32> PreviewRemoval(int32 PieceId) const;
	/** Careful dismantle: the most intact components back; what loses support collapses by the canonical rules. */
	FGLDemolishResult Dismantle(AActor* Builder, int32 PieceId) { return Remove(Builder, PieceId, EGLSalvagePath::Careful); }
	/** Destructive smash: faster, fewer components, more scrap. */
	FGLDemolishResult Smash(AActor* Builder, int32 PieceId) { return Remove(Builder, PieceId, EGLSalvagePath::Destructive); }
	FGLDemolishResult Remove(AActor* Builder, int32 PieceId, EGLSalvagePath Path);
	/** May Builder install Layer (a finish) on PieceId now, materials included? */
	FGLInstallCheck CheckInstall(const AActor* Builder, int32 PieceId, FName Layer) const;
	/** Installs a finish layer (pays its cost, base storage first): presentation changes, support never does. */
	FGLInstallCheck InstallFinish(AActor* Builder, int32 PieceId, FName Layer);

	/** Material sources for an operation by Who at At (see FGLMaterialSources). */
	FGLMaterialSources SourcesFor(const AActor* Who, const FVector& At) const;
	/** Recognized claims (derived from intact player base cores; P11: one area of the tuned radius each). */
	TArray<FGLClaim> Claims() const;
	/** A storage piece's contents (null when it is not an intact player storage piece). */
	FGLInventory* StorageOf(int32 PieceId);
	const FGLInventory* StorageOf(int32 PieceId) const { return const_cast<UGLBuildingSubsystem*>(this)->StorageOf(PieceId); }
	/**
	 * P11: moves every carried material (not tools) from Who into an intact player storage piece, as much as fits; what
	 * does not fit stays with Who. Returns the number of items moved. Never loses or duplicates anything.
	 */
	int32 Store(AActor* Who, int32 PieceId);
	/** P11: moves a storage piece's contents to Who, as much as fits; the rest stays in the piece (InventoryFull). */
	int32 Take(AActor* Who, int32 PieceId);
	/** Player-built station tags within reach of a point (a sawhorse provides Station.Saw). */
	TArray<FName> StationsNear(const FVector& At, double ReachCm = 300.0) const;

	/** Snap helper (GLStructureRules::Snap on the live ground, over the intact player pieces); OutInfo: what it connected. */
	bool Snap(FName Def, const FVector& Aim, int32 YawStep, FGLPlacedPiece& OutCandidate, FGLSnapInfo* OutInfo = nullptr, const FVector& AimDirection = FVector::ZeroVector) const;
	/** P12: what an operation costing Cost at At would take and from where (dry run of the commit's consumption). */
	FGLCostView CostView(const AActor* Who, const FVector& At, TConstArrayView<FGLItemStackDef> Cost) const;
	/** P12: what removing PieceId by Path recovers (the commit's own yield), and whether all of it fits Who's sources. */
	TMap<FName, int32> RemovalYield(int32 PieceId, EGLSalvagePath Path) const;
	bool RemovalYieldFits(const AActor* Who, int32 PieceId, EGLSalvagePath Path) const;
	/** P12: the finishes the install rule accepts on PieceId now (role, phase, knowledge), by display name. */
	TArray<FName> FinishesFor(int32 PieceId) const;
	bool IsUnderStructure(const FVector2D& World) const;

	TMap<int32, double> Support() const;
	/** Intact player pieces in loaded cells, sorted by id. */
	TArray<FGLPlacedPiece> GetPieces() const;
	TArray<FGLPlacedPiece> PiecesOfCell(FName Cell) const;
	/** A piece's own actor, when it has one (storage, falling, debris); a quiescent piece is instanced and has none. */
	AGLBuildPiece* FindActor(int32 PieceId) const;
	int32 PieceIdOf(const AActor* Actor) const;
	/** The player piece a hit belongs to (an instanced piece through its batch's owner table, or a piece's actor). */
	int32 PieceIdAt(const FHitResult& Hit) const;
	/** "frame", "finish" or "complete" as presented now (None while not presented). */
	FName ShownPhaseOf(int32 PieceId) const;
	/** Presented now (instanced or by its actor). */
	bool IsPresented(int32 PieceId) const;
	/** The removal preview: exactly these pieces shown red. */
	void SetRemovalHighlight(const TArray<int32>& PieceIds);

	/** Streaming / load (P3, P11): adds a cell's saved player pieces silently (presented over frames). */
	void RestoreCell(FName Cell, const TArray<FGLSavedPiece>& Saved, const TArray<FGLSavedCollapse>& InFlight, TArray<FString>* OutProblems = nullptr);
	TSet<FName> CellsWithPieces() const;
	void SetNextId(int32 InNextId) { NextId = FMath::Max(NextId, InNextId); }
	/** The cell a piece at this location belongs to: the loaded ground under it, else the Grid cell. */
	FName CellFor(const FVector& Location) const;
	int32 GetNextId() const { return NextId; }

private:
	double GroundAt(const FVector2D& At) const;
	class UGLStructureSubsystem* Structures() const;
	void Emit(const TCHAR* Tag, FName Subject, AActor* Instigator, const TMap<FName, double>& Numbers = {});
	TMap<FName, int32> ScaledYield(const FGLPlacedPiece& Piece, EGLSalvagePath Path) const;

	int32 NextId = 1;
};
