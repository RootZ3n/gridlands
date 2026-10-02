#pragma once

#include "Building/GLCollapseRules.h"
#include "CoreMinimal.h"
#include "Save/GLWorldSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLStructureSubsystem.generated.h"

class AGLStructurePart;

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
	TWeakObjectPtr<AGLStructurePart> Actor;
};

struct GRIDLANDSGAME_API FGLStructureRuntime
{
	FName Placement;
	FName Def;
	FName Cell;
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
	bool SpawnStructure(FName Placement, FName Def, FName Cell, const FVector& Origin, int32 YawQuarter, bool bDeferPresentation = false);
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

	/** Terrain must not move under an intact grounded part or under debris (a digging refusal, like player pieces). */
	bool IsUnderStructure(const FVector2D& World, double MarginCm = 50.0) const;
	/**
	 * The ground footprints IsUnderStructure tests (intact grounded parts and debris, grown by MarginCm)
	 * that touch Area: a caller testing many points in one place collects them once (P7: vegetation
	 * checked every part in the world per tuft, which a dense cell made the most expensive thing in it).
	 */
	void CollectFootprints(const FBox2D& Area, double MarginCm, TArray<FBox2D>& Out) const;
	/** Player pieces must not overlap structure parts or debris. */
	bool Overlaps(const FBox& Box) const;

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
	void Collapse(FGLStructureRuntime& Structure, AActor* By, const FVector& From, bool bSilent = false);
	void Land(FGLActiveCollapse& Collapse);
	AGLStructurePart* SpawnPart(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	void MakeDebris(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	/** Makes a deferred part's actor in its current authoritative state (or nothing, if it is gone). */
	AGLStructurePart* Present(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part);
	bool IsPending(FName Placement, FName Part) const;
	double GroundAt(const FVector2D& At) const;

	TMap<FName, FGLStructureRuntime> Structures;
	/** Parts whose actors are still to be presented (placement, part). */
	TArray<TPair<FName, FName>> Pending;
	/** Retired part actors of unloaded cells, destroyed within the presentation budget. */
	TArray<TWeakObjectPtr<AGLStructurePart>> Retiring;
	TArray<FGLActiveCollapse> Active;
	TArray<FGLImpactRecord> Impacts;
	TArray<TWeakObjectPtr<AActor>> ToDestroy;
	double Clock = 0.0;
	int32 NextPieceId = 1000000; // runtime only, never saved (saves name parts)
};
