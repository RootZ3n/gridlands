#pragma once

#include "CoreMinimal.h"

class FGLContentRegistry;
class FGLInventory;
class FGLKnowledge;
struct FGLBuildPieceDef;

/** A placed build piece: the only building fact that is saved (support is derived, S-1). */
struct GRIDLANDSCORE_API FGLPlacedPiece
{
	int32 Id = 0;
	FName Def;
	/** World cm, the piece's bottom centre. */
	FVector Location = FVector::ZeroVector;
	/** Yaw in quarter turns (0..3). Pieces are axis-aligned so bounds stay exact. */
	int32 YawQuarter = 0;
	/** The Grid cell it belongs to (P3): streamed and saved with that cell. Not used by the rules. */
	FName Cell;
};

/** A socket in world space. */
struct GRIDLANDSCORE_API FGLWorldSocket
{
	FName Role; // bottom | top | side
	FVector Location = FVector::ZeroVector;
};

enum class EGLBuildRefusal : uint8
{
	None,
	UnknownPiece,
	NotKnown,       // missing knowledge
	MissingItems,
	Overlaps,
	Buried,         // pushed into the ground
	Unsupported,    // nothing holds it up (support would be <= 0)
};

struct GRIDLANDSCORE_API FGLBuildCheck
{
	EGLBuildRefusal Refusal = EGLBuildRefusal::None;
	FString Reason;
	/** Support the piece would have (material strength units; > 0 is stable). */
	double Support = 0.0;
	TArray<FName> MissingKnowledge;

	bool IsAllowed() const { return Refusal == EGLBuildRefusal::None; }
};

/**
 * Building v0 structural rules (ADR-0024). Pure: pieces, content and a ground-height function in,
 * answers out. Valheim-like support: a piece resting on the ground has its material's strength;
 * each piece further up loses strength/maxStack, and each lateral link loses
 * strength * metres / maxHorizontalSpan; a piece is stable while its support stays above zero.
 * A piece is never stronger than its own material (support is capped by it).
 */
namespace GLStructureRules
{
	using FGroundHeight = TFunctionRef<double(const FVector2D& World)>;

	constexpr double SocketToleranceCm = 5.0;
	constexpr double GroundToleranceCm = 30.0;
	constexpr double OverlapShrinkCm = 6.0;
	constexpr double MaxSnapDistanceCm = 150.0;

	GRIDLANDSCORE_API FVector ToWorld(const FGLPlacedPiece& Piece, const TArray<double>& LocalMetres);
	GRIDLANDSCORE_API FBox Bounds(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece);
	GRIDLANDSCORE_API TArray<FGLWorldSocket> Sockets(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece);

	/** True when every bottom socket of a ground-capable piece sits on the ground (within tolerance). */
	GRIDLANDSCORE_API bool RestsOnGround(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece, FGroundHeight Ground);

	/** Support of every piece (id -> value; <= 0 means unsupported). Deterministic. */
	GRIDLANDSCORE_API TMap<int32, double> ComputeSupport(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces, FGroundHeight Ground);

	/** Geometry and structure only (overlap, buried, support). */
	GRIDLANDSCORE_API FGLBuildCheck CheckPlacement(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
		const FGLPlacedPiece& Candidate, FGroundHeight Ground);
	/** Everything: knowledge, items, then CheckPlacement. */
	GRIDLANDSCORE_API FGLBuildCheck CanPlace(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
		const FGLPlacedPiece& Candidate, FGroundHeight Ground, const FGLKnowledge& Knowledge, const FGLInventory& Inventory);

	/** Pieces that would lose all support if RemovedId were taken away (not including RemovedId). */
	GRIDLANDSCORE_API TArray<int32> CollapsesAfterRemoving(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces,
		int32 RemovedId, FGroundHeight Ground);

	/**
	 * Where Def would go near Aim: aligned to the nearest compatible free socket (bottom onto top,
	 * side onto side) within MaxSnapDistanceCm that does not overlap; otherwise, for a ground-capable
	 * piece, on the ground at Aim. Returns false when neither applies.
	 */
	GRIDLANDSCORE_API bool Snap(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing, FName Def,
		const FVector& Aim, int32 YawQuarter, FGroundHeight Ground, FGLPlacedPiece& OutCandidate);

	/** Is this ground point under a piece that rests on the ground (terraforming must not move it)? */
	GRIDLANDSCORE_API bool IsUnderStructure(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces,
		const FVector2D& World, double MarginCm = 50.0);
}
