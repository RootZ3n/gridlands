#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLBuildingSubsystem.generated.h"

class AGLBuildPiece;
class UGLInventoryComponent;

/** Why a demolition was refused (nothing changed). */
enum class EGLDemolishRefusal : uint8 { None, UnknownPiece, NoRoomForRefund };

struct GRIDLANDSGAME_API FGLDemolishResult
{
	EGLDemolishRefusal Refusal = EGLDemolishRefusal::None;
	/** Removed pieces: the target first, then any that collapsed without it. */
	TArray<int32> Removed;
	/** Items returned to the builder (full cost of every removed piece: nothing is lost). */
	TMap<FName, int32> Refunded;

	bool IsDone() const { return Refusal == EGLDemolishRefusal::None; }
};

/**
 * Building v0 (ADR-0024). Placement and demolition are transactions against the builder's
 * inventory and knowledge: either everything happens or nothing does. Pieces are the saved facts;
 * support is recomputed from them (S-1). Emits Event.Building.*.
 */
UCLASS()
class GRIDLANDSGAME_API UGLBuildingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Checks without changing anything (the ghost preview uses this). */
	FGLBuildCheck Check(const AActor* Builder, const FGLPlacedPiece& Candidate) const;
	/** Pays the cost and places the piece, or refuses with nothing changed. */
	FGLBuildCheck Place(AActor* Builder, const FGLPlacedPiece& Candidate);
	/** Removes a piece and anything that falls without it, refunding their full cost. */
	FGLDemolishResult Demolish(AActor* Builder, int32 PieceId);

	/** Snap helper (GLStructureRules::Snap on the live ground). */
	bool Snap(FName Def, const FVector& Aim, int32 YawQuarter, FGLPlacedPiece& OutCandidate) const;
	bool IsUnderStructure(const FVector2D& World) const;

	TMap<int32, double> Support() const;
	const TArray<FGLPlacedPiece>& GetPieces() const { return Pieces; }
	AGLBuildPiece* FindActor(int32 PieceId) const;
	int32 PieceIdOf(const AActor* Actor) const;

	/** Save support: replaces all pieces silently (no events, no costs). */
	void Restore(const TArray<FGLPlacedPiece>& InPieces, int32 InNextId);
	/** Streaming (P3): adds a cell's saved pieces silently. */
	void RestoreCell(FName Cell, const TArray<FGLPlacedPiece>& InPieces);
	/** Streaming (P3): removes a cell's pieces and their actors, returning them (for its record). */
	TArray<FGLPlacedPiece> RemoveCell(FName Cell);
	TArray<FGLPlacedPiece> PiecesOfCell(FName Cell) const { return Pieces.FilterByPredicate([Cell](const FGLPlacedPiece& P) { return P.Cell == Cell; }); }
	TSet<FName> CellsWithPieces() const { TSet<FName> Out; for (const FGLPlacedPiece& P : Pieces) { Out.Add(P.Cell); } return Out; }
	void SetNextId(int32 InNextId) { NextId = FMath::Max(NextId, InNextId); }
	/** The cell a piece at this location belongs to: the loaded ground under it, else the Grid cell. */
	FName CellFor(const FVector& Location) const;
	int32 GetNextId() const { return NextId; }

private:
	double GroundAt(const FVector2D& At) const;
	AGLBuildPiece* SpawnPiece(const FGLPlacedPiece& Piece);
	void Emit(const TCHAR* Tag, FName Subject, AActor* Instigator, const TMap<FName, double>& Numbers = {});

	TArray<FGLPlacedPiece> Pieces;
	UPROPERTY() TMap<int32, TObjectPtr<AGLBuildPiece>> Actors;
	int32 NextId = 1;
};
