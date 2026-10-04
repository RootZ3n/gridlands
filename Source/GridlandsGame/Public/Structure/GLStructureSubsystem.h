#pragma once

#include "Building/GLClaimRules.h"
#include "Building/GLCollapseRules.h"
#include "Building/GLConstructionRules.h"
#include "CoreMinimal.h"
#include "Inventory/GLInventory.h"
#include "Save/GLWorldSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLStructureSubsystem.generated.h"

class AGLStructurePart;
class AGLPlayerPieceBatch;
struct FHitResult;

/** One part of a live structure: its piece in world space and what has happened to it. */
struct GRIDLANDSGAME_API FGLStructurePartRuntime
{
	FName Name;
	FGLPlacedPiece Piece;
	FName Salvage;
	EGLCollapseMotion Motion = EGLCollapseMotion::Drop;
	EGLToppleDirection Direction = EGLToppleDirection::AwayFromInstigator;
	double DamageScale = 1.0;
	EGLStructurePartState State = EGLStructurePartState::Intact;
	/** Debris: its authoritative rest transform. */
	FTransform Rest;
	/** P11: a storage piece's contents (kept with the piece, intact or as debris; never lost, never duplicated). */
	FGLInventory Contents = FGLInventory(0);
	TWeakObjectPtr<AGLStructurePart> Actor;
	/** P11 scaling: presented by its structure's instanced batch (a quiescent player piece), not by an actor. */
	bool bInstanced = false;

	bool IsPlayer() const { return Piece.Origin == EGLPieceOrigin::Player; }
};

struct GRIDLANDSGAME_API FGLStructureRuntime
{
	FName Placement;
	FName Def;
	FName Cell;
	/** P11: a cell's player construction is one structure in the same model (key PlayerKey(Cell)); its pieces are facts, not data. */
	bool bPlayer = false;
	TArray<FGLStructurePartRuntime> Parts;

	FGLStructurePartRuntime* Find(FName Part) { return Parts.FindByPredicate([Part](const FGLStructurePartRuntime& P) { return P.Name == Part; }); }
};

/**
 * A collapse in progress (P10, ADR-0038: SUPPORT FAILED, DELAY, IN FLIGHT): the plan's impact still to happen, and the
 * pose to present until then. The plan fixed the trajectory, rest, impact time and volume; WHO it affects is decided
 * only at impact, from where everything is then. Its clock runs only while its cell is live and none of the cell's
 * creatures is waiting to be presented (a frozen creature must not lose time against a falling structure).
 */
struct GRIDLANDSGAME_API FGLActiveCollapse
{
	FName Placement;
	FName Part;
	FName Cell;
	FGLCollapseOutcome Outcome;
	/** Seconds since its support failed, on its own clock. */
	double Elapsed = 0.0;
	bool bImpacted = false;
	FName Material;
	/** Who physically removed the support, and who receives gameplay attribution (kills, drops): identities, not actors, so a save keeps them. */
	FName Cause;
	FName Credit;
	/** The credited actor while this session lasts (any pawn); after a reload, Credit's identity finds it again. */
	TWeakObjectPtr<AActor> CreditActor;
};

/** What one impact did (tests and evidence read it). */
struct GRIDLANDSGAME_API FGLImpactRecord
{
	FName Placement;
	FName Part;
	double Damage = 0.0;
	double Severity = 0.0;
	/** Every pawn or creature the impact volume touched (creature models included, presented or not). */
	TArray<TWeakObjectPtr<AActor>> Hit;
	/** Creature placements it pinned (Neutralize.Pinned: no damage) and creature placements it damaged. */
	TArray<FName> Pinned;
	TArray<FName> Damaged;
};

/**
 * Authored world structures (P6, ADR-0030): salvageable buildings and trees, in the shared
 * structural language (GLStructureRules). Owns the authoritative part states. Salvaging a part
 * re-derives support; whatever lost it collapses by the deterministic plan (GLCollapseRules): the
 * outcome (debris and where it rests) is committed at once, the impact damages whatever it hits
 * through the normal health system, and the pose only follows. Structures stream with their cell and
 * persist as facts per part (FGLSavedStructurePart).
 */
UCLASS()
class GRIDLANDSGAME_API UGLStructureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGLStructureSubsystem, STATGROUP_Tickables); }

	/** Advances the structure clock (Tick calls it; tests call it directly): impacts land, poses follow. */
	void Advance(double Seconds);

	/**
	 * Spawns a structure placement: every part intact. Its origin sits on the ground at Origin's XY.
	 * bDeferPresentation (P7): only the authoritative model is made; part actors are instantiated by
	 * PumpPresentation later, each in the state the model holds THEN (saved state is resolved first).
	 */
	bool SpawnStructure(FName Placement, FName Def, FName Cell, const FVector& Origin, int32 YawStep, bool bDeferPresentation = false);

	// ---- P11 (ADR-0039): player-built structures in the canonical structural model ----
	/** The structure key of a cell's player construction. */
	static FName PlayerKey(FName Cell);
	static FName PlayerPartName(int32 PieceId);
	/** Adds a player piece (intact) to its cell's player structure. bDeferPresentation: made later by the pump (restore). */
	bool AddPlayerPiece(const FGLPlacedPiece& Piece, bool bDeferPresentation);
	/** Player pieces (all loaded cells, or one cell); intact only unless bIncludeDebris. Sorted by id. */
	TArray<FGLPlacedPiece> PlayerPieces(FName Cell = NAME_None, bool bIncludeDebris = false) const;
	const FGLStructurePartRuntime* FindPlayerPiece(int32 PieceId) const;
	FGLStructurePartRuntime* FindPlayerPieceMutable(int32 PieceId);
	/** The intact pieces of the same player structure as PieceId (what its support is computed over). */
	TArray<FGLPlacedPiece> PlayerStructureOf(int32 PieceId) const;
	/**
	 * Removes an intact player piece and collapses what loses support by the canonical rules (the same as authored
	 * structures: debris, impact at impact time, persistence). Returns the ids that collapsed (sorted), or -1 entries none.
	 */
	TArray<int32> RemovePlayerPiece(int32 PieceId, AActor* By);
	/** Replaces an intact player piece's layers (a finish installed) and re-presents it. Support is unchanged by layers. */
	bool SetPlayerLayers(int32 PieceId, const TArray<FName>& Layers);
	TSet<FName> CellsWithPlayerPieces() const;

	// ---- P11 scaling: player pieces' presentation (GAMEPLAY MODEL != PRESENTATION) ----
	/**
	 * The player piece a hit belongs to (0: none): an instance of a player structure's batch (through the batch's owner
	 * table, never its renderer order) or a player piece's own actor (storage, falling, debris).
	 */
	int32 PlayerPieceAt(const FHitResult& Hit) const;
	/** "frame", "finish" or "complete" as presented now; None while it is not presented. */
	FName ShownPhaseOf(int32 PieceId) const;
	bool IsPresented(int32 PieceId) const;
	bool IsInstanced(int32 PieceId) const;
	/** The removal preview: exactly these pieces shown red (instanced or actor-presented), every other one not. */
	void SetRemovalHighlight(const TArray<int32>& PieceIds);
	/** A cell's player batch (null when it has no instanced piece yet). */
	AGLPlayerPieceBatch* BatchOf(FName Cell) const;
	int32 RetiringBatches() const { return RetiringBatchList.Num(); }
	struct FPlayerPresentation { int32 Instanced = 0; int32 Actors = 0; int32 Instances = 0; int32 Components = 0; int32 Batches = 0; };
	/** Evidence: how a cell's (or every cell's) player construction is presented now. */
	FPlayerPresentation PlayerPresentation(FName Cell = NAME_None) const;
	/** Save: a cell's player pieces (intact and debris, with layers and contents), sorted by id. */
	void CapturePlayerCell(FName Cell, TArray<FGLSavedPiece>& Out) const;
	/** Load/stream-in: a cell's saved player pieces, silently (presentation deferred); collapses in flight resume. */
	void RestorePlayerCell(FName Cell, const TArray<FGLSavedPiece>& Saved, const TArray<FGLSavedCollapse>& InFlight, TArray<FString>* OutProblems = nullptr);
	/** World renewal of an authored structure (all parts intact again). Never a player structure, never inside a claim. */
	enum class ERenewal : uint8 { Renewed, Unknown, PlayerOwned, InsideClaim };
	ERenewal Renew(FName Placement, TConstArrayView<FGLClaim> Claims);
	int32 ImpactCount() const { return Impacts.Num(); }
	/**
	 * Instantiates deferred part actors, nearest to Where first: every part within NearCm at once
	 * (what Zenny can touch), then more until BudgetSeconds is spent (0: all; < 0: only the near ones). A part is made in its
	 * current authoritative state: intact, falling, or debris at rest; removed and salvaged parts are
	 * never made. Returns how many actors were made.
	 */
	int32 PumpPresentation(const FVector& Where, double BudgetSeconds, double NearCm = 0.0);
	bool IsCellPresented(FName Cell) const;
	/** Presents one waiting part now, by the pump's own path (tests choose the order). False if it was not waiting. */
	bool PresentPart(FName Placement, FName Part);
	bool IsPartPending(FName Placement, FName Part) const { return IsPending(Placement, Part); }
	int32 PendingPresentation() const { return Pending.Num(); }
	/** Part actors of unloaded cells still waiting to be destroyed (retired: inert, hidden, no collision). */
	int32 RetiringActors() const { return Retiring.Num(); }
	/**
	 * Streaming: removes a cell's structures and their collapses in flight. The cell's record captured those first
	 * (CaptureCollapses), so they are frozen while it is dormant and resume, never replay, when it returns (P10).
	 */
	int32 RemoveCell(FName Cell);

	/** Save: every part of the cell's structures that is no longer intact, sorted. */
	void CaptureCell(FName Cell, TArray<FGLSavedStructurePart>& Out) const;
	/** P10: every collapse of the cell still in flight (support failed, impact to come), sorted. */
	void CaptureCollapses(FName Cell, TArray<FGLSavedCollapse>& Out) const;
	/**
	 * Load/stream-in: applies saved part states silently (no collapse replayed, no damage, no noise). P10: a part with a
	 * saved collapse in flight resumes it from its elapsed time (not solid, at the plan's pose); its impact is still to come.
	 */
	void RestoreCell(FName Cell, const TArray<FGLSavedStructurePart>& Saved, const TArray<FGLSavedCollapse>& InFlight = TArray<FGLSavedCollapse>(),
		TArray<FString>* OutProblems = nullptr);

	/** Terrain must not move under an intact grounded part or under debris (a digging refusal, player pieces included). */
	bool IsUnderStructure(const FVector2D& World, double MarginCm = 50.0) const;
	/**
	 * The ground footprints IsUnderStructure tests (intact grounded parts and debris, P11: oriented at their yaw) that
	 * touch Area, grown by MarginCm: a caller testing many points in one place collects them once (P7: vegetation
	 * checked every part in the world per tuft, which a dense cell made the most expensive thing in it).
	 */
	void CollectFootprints(const FBox2D& Area, double MarginCm, TArray<FGLFootprint>& Out) const;
	/** A new player piece must not overlap authored parts or any debris (intact player pieces are the rules' own check). */
	bool Overlaps(const FGLFootprint& Footprint) const;

	const FGLStructureRuntime* Find(FName Placement) const { return Structures.Find(Placement); }
	FGLStructureRuntime* FindMutable(FName Placement) { return Structures.Find(Placement); }
	AGLStructurePart* FindPart(FName Placement, FName Part) const;
	int32 ActiveCollapses() const { return Active.Num(); }
	const TArray<FGLActiveCollapse>& GetActive() const { return Active; }
	/** P10: a gameplay identity for an actor (Zenny, Pehlichi, a creature's placement), and back. */
	static FName IdentityOf(const AActor* Actor);
	AActor* ActorOf(FName Identity) const;
	const TArray<FGLImpactRecord>& GetImpacts() const { return Impacts; }
	double GetClock() const { return Clock; }

	/** The salvage pipeline completed on a part (bound to its salvageable component). */
	void HandlePartSalvaged(FName Placement, FName Part, AActor* By);

private:
	/** Returns the ids of the pieces that lost support (sorted). */
	TArray<int32> Collapse(FGLStructureRuntime& Structure, AActor* By, const FVector& From, bool bSilent = false);
	void Land(FGLActiveCollapse& Collapse);
	AGLStructurePart* SpawnPart(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	void MakeDebris(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	/** Presents a deferred part in its current authoritative state (instanced or an actor; nothing, if it is gone). */
	bool Present(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	/** A quiescent intact player piece into its structure's batch. False: it needs an actor (or is not presentable). */
	bool PresentInstanced(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	/** Out of the batch (it is about to fall, be re-presented or go): its instances only; the record is untouched. */
	void Uninstance(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	AGLPlayerPieceBatch* BatchFor(FGLStructureRuntime& Structure);
	bool IsPending(FName Placement, FName Part) const;
	double GroundAt(const FVector2D& At) const;

	TMap<FName, FGLStructureRuntime> Structures;
	/** P11: player piece id -> its cell (its structure is PlayerKey(cell)). */
	TMap<int32, FName> PlayerPieceCells;
	/** Parts whose actors are still to be presented (placement, part). */
	TArray<TPair<FName, FName>> Pending;
	/** Retired part actors of unloaded cells, destroyed within the presentation budget. */
	TArray<TWeakObjectPtr<AGLStructurePart>> Retiring;
	/** P11: player structure -> its instanced batch; and retired batches of unloaded cells (one actor per cell). */
	TMap<FName, TWeakObjectPtr<AGLPlayerPieceBatch>> Batches;
	TArray<TWeakObjectPtr<AGLPlayerPieceBatch>> RetiringBatchList;
	/** Actor-presented pieces shown red by the removal preview. */
	TArray<int32> HighlightedActors;
	TArray<FGLActiveCollapse> Active;
	TArray<FGLImpactRecord> Impacts;
	TArray<TWeakObjectPtr<AActor>> ToDestroy;
	double Clock = 0.0;
	int32 NextPieceId = 1000000; // runtime only, never saved (saves name parts)
};
