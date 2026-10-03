#pragma once

#include "Building/GLStructureRules.h"
#include "Save/GLWorldSave.h"
#include "CoreMinimal.h"

class AActor;
class UWorld;

#if !UE_BUILD_SHIPPING

/** One check of the WINCHESTER proof (P11): what was asserted, whether it held, and the evidence. */
struct GRIDLANDSGAME_API FGLWinchesterCheck
{
	FString Name;
	bool bPass = false;
	FString Detail;
};

/**
 * DEV ONLY (P11, ADR-0039): the WINCHESTER / REAL HOUSE-0 architectural proof. A deliberately mixed two-room cabin built
 * through the real player transactions (never data placements, never shipping):
 * - a base core (claim) with two storage crates holding most of the materials, and a sawhorse;
 * - room A: modern stud frames, a doorway into a 45 degree three-sided bay (angle posts, snapped yaw), Victorian
 *   clapboard finish, a shingled gable;
 * - room B: a log-cabin wing under a shingled gable;
 * - a porch roof resting only on a Roman column and a timber post.
 * Every placement records its preview against the committed result (PREVIEW == REALITY). The automated test and the
 * real-game proof (gl.Building.Proof) run the same steps.
 */
struct GRIDLANDSGAME_API FGLWinchesterHouse
{
	FVector Anchor = FVector::ZeroVector;
	TArray<FGLWinchesterCheck> Checks;
	/** Piece ids by role in the house (for the later steps and the report). */
	TMap<FName, int32> Ids;
	TArray<int32> BayIds;
	int32 Placements = 0;
	int32 PreviewMismatches = 0;
	int32 Green = 0, Yellow = 0, Red = 0;
	/** Materials consumed from storage vs from Zenny while building. */
	TMap<FName, int32> FromStorage, FromZenny;

	/** Levels a pad for the house with ordinary flatten strokes (the ground a player would prepare). */
	bool PreparePad(UWorld* World);
	/** Base core, two crates stocked with the house's materials (most of them), a sawhorse. Zenny keeps a small share. */
	bool EstablishBase(UWorld* World, AActor* Zenny);
	/** Every FRAME of the house, through the real placement transaction (storage first). */
	bool Frame(UWorld* World, AActor* Zenny);
	/** Clapboard on room A's stud frames and the bay, shingles on every roof frame. */
	bool Finish(UWorld* World, AActor* Zenny);
	/** Saws a log into studs at the sawhorse from storage (raw log -> sawing -> studs). */
	bool Saw(UWorld* World, AActor* Zenny);
	/** Porch: post out (the roof stands, as predicted), then the column (the roof falls, as predicted). */
	bool PorchCollapse(UWorld* World, AActor* Zenny);
	/** Careful dismantle of the interior wall (intact studs) and a smash of a bay wall (degraded). */
	bool Salvage(UWorld* World, AActor* Zenny);
	/** A summary of the house as it stands (counts, layers, vocabularies, ownership): the same numbers before and after save/streaming. */
	FString Fingerprint(UWorld* World) const;

	FVector At(double X, double Y, double Z = 0.0) const { return Anchor + FVector(X, Y, Z); }
	void Note(const FString& Name, bool bPass, const FString& Detail = FString());
	bool AllPassed() const { return !Checks.ContainsByPredicate([](const FGLWinchesterCheck& C) { return !C.bPass; }); }

	/** Places one piece through the transaction, recording PREVIEW == REALITY. Returns its id (0 when refused). */
	int32 Place(UWorld* World, AActor* Zenny, FName Def, const FVector& Location, int32 YawStep, FName Role = NAME_None);
	/** Snaps then places (the bay: yaw from socket data). */
	int32 SnapPlace(UWorld* World, AActor* Zenny, FName Def, const FVector& Aim, FName Role = NAME_None);

	/** The house's materials: frames and finishes, summed per item, from the data. */
	static TMap<FName, int32> Materials();

	/**
	 * P11 density fixture: Units small player-built units (a floor, four walls on its edges, a lean-to roof; every 4th
	 * unit at 45 degrees; finishes on alternate units; a stocked storage crate in every 6th), each at its own ground
	 * height, as player-owned facts. 50 units = 309 pieces. Ids from FirstId up.
	 */
	static TArray<FGLPlacedPiece> DensePlayerBase(const FVector& Origin, int32 Units, TFunctionRef<double(const FVector2D&)> Ground, int32 FirstId, FName Cell);
	/** The same as saved facts (crates stocked), for the real restore path. */
	static TArray<FGLSavedPiece> DensePlayerBaseSaved(const FVector& Origin, int32 Units, TFunctionRef<double(const FVector2D&)> Ground, int32 FirstId, FName Cell);
};

#endif
