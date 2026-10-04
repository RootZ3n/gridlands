#pragma once

#include "CoreMinimal.h"

class FGLContentRegistry;
class FGLInventory;
class FGLKnowledge;
struct FGLBuildPieceDef;

/** Who made a placed piece (P11, ADR-0039). Explicit in every saved fact; never inferred from where it is stored. */
enum class EGLPieceOrigin : uint8
{
	Authored = 0, // world content (authored structures, trees): renewable by world rules
	Player = 1,   // built by the player: durable owned state, never renewed away
};

/**
 * A placed build piece: the structural fact that is saved (support is derived, S-1). P11: yaw is an integer in
 * 2.5 degree steps (YawStep, 0..143), so every rotation is exact and comparable; installed construction layers (a
 * finish, later electrical) are part of the fact, in phase order.
 */
struct GRIDLANDSCORE_API FGLPlacedPiece
{
	int32 Id = 0;
	FName Def;
	/** World cm, the piece's bottom centre. */
	FVector Location = FVector::ZeroVector;
	/** Yaw in GLStructureRules::YawStepDegrees steps (0..YawSteps-1). Quarter turns are multiples of 36. */
	int32 YawStep = 0;
	/** The Grid cell it belongs to (P3): streamed and saved with that cell. Not used by the rules. */
	FName Cell;
	EGLPieceOrigin Origin = EGLPieceOrigin::Authored;
	/** Installed layers (finish.*, later electrical.*), in phase order. Empty: frame only. Never changes support. */
	TArray<FName> Layers;
};

/** A socket in world space. */
struct GRIDLANDSCORE_API FGLWorldSocket
{
	FName Name;
	FName Role; // bottom | top | side
	FVector Location = FVector::ZeroVector;
	/** World facing in degrees (0 = +X), when the socket declares one. */
	bool bHasFacing = false;
	double Facing = 0.0;
};

/**
 * A piece's oriented footprint (P11): a rectangle in the XY plane at its yaw, and a Z interval. Every shape-based
 * world question (overlap, terrain protection, landing surfaces, impact volumes) uses it, so any yaw is exact.
 */
struct GRIDLANDSCORE_API FGLFootprint
{
	FVector2D Centre = FVector2D::ZeroVector;
	FVector2D AxisX = FVector2D(1.0, 0.0);
	FVector2D AxisY = FVector2D(0.0, 1.0);
	FVector2D Half = FVector2D::ZeroVector;
	double ZMin = 0.0;
	double ZMax = 0.0;

	/** Is the XY point inside (grown by MarginCm on every side)? */
	bool ContainsXY(const FVector2D& Point, double MarginCm = 0.0) const;
	/** Do the two oriented rectangles overlap in XY (separating axes), each shrunk by ShrinkCm? */
	bool OverlapsXY(const FGLFootprint& Other, double ShrinkCm = 0.0) const;
	/** XY overlap and Z overlap, both shrunk by ShrinkCm. */
	bool Overlaps(const FGLFootprint& Other, double ShrinkCm) const;
	/** Corners, edge midpoints and centre (9 points), for sampling the ground under it. */
	TArray<FVector2D> SamplePoints() const;
	/** The world box enclosing it. */
	FBox Enclosing() const;
	/** How far the rectangle reaches along a unit XY direction from its centre. */
	double ReachAlong(const FVector2D& Direction) const;
	/** An axis-aligned footprint from a world box (debris lying in any pose is held conservatively by its box). */
	static FGLFootprint FromBox(const FBox& Box);
};

/** Placement preview (P11, operator-approved semantics): the same rules as the commit, never a parallel estimate. */
enum class EGLPreview : uint8
{
	Red,    // refused: structurally invalid (or a non-structural refusal, reported with its own reason)
	Yellow, // accepted, at its material's limit: support <= one more vertical step of its own material
	Green,  // accepted with margin
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
	OutsideClaim,   // P11: a base core may not overlap another claim
};

/** P12 (ADR-0040): what a snap connected (the snap marker shows exactly this; the commit places the same candidate). */
struct GRIDLANDSCORE_API FGLSnapInfo
{
	/** Snapped to another piece's socket (false: on the ground at the aim point). */
	bool bSnapped = false;
	int32 TargetPieceId = INT32_MIN;
	FName TargetSocket;
	FVector TargetLocation = FVector::ZeroVector;
	/** The candidate's own socket that meets it. */
	FName OwnSocket;
	/** The yaw came from the two sockets' facings (data), not from the requested yaw. */
	bool bYawFromData = false;
};

struct GRIDLANDSCORE_API FGLBuildCheck
{
	EGLBuildRefusal Refusal = EGLBuildRefusal::None;
	FString Reason;
	/** Support the piece would have (material strength units; > 0 is stable). */
	double Support = 0.0;
	/** One vertical step of the piece's own material (strength / maxStack): the YELLOW threshold. */
	double VerticalStep = 0.0;
	EGLPreview Preview = EGLPreview::Red;
	TArray<FName> MissingKnowledge;
	/** P12 (ADR-0040): machine-readable detail for the refusal (the UI explains these; it never re-derives them). */
	/** Overlaps: the existing piece in the way (its id; INT32_MIN when it is not a known piece). */
	int32 BlockingPieceId = INT32_MIN;
	/** MissingItems: the first item short, how many the piece needs and how many the sources hold. */
	FName MissingItem;
	int32 MissingNeeded = 0;
	int32 MissingHave = 0;
	/** The piece's material (YELLOW: "at <material>'s limit"). */
	FName Material;

	bool IsAllowed() const { return Refusal == EGLBuildRefusal::None; }
};

/**
 * Structural rules (ADR-0024, generalized by ADR-0039 in P11). Pure: pieces, content and a ground-height function in,
 * answers out. Valheim-like support: a piece resting on the ground has its material's strength; each piece further up
 * loses strength/maxStack, and each lateral link loses strength * metres / maxHorizontalSpan; a piece is stable while its
 * support stays above zero. A piece is never stronger than its own material (support is capped by it). Links are made
 * by coinciding sockets; yaw is any multiple of 2.5 degrees.
 */
namespace GLStructureRules
{
	using FGroundHeight = TFunctionRef<double(const FVector2D& World)>;

	constexpr double SocketToleranceCm = 5.0;
	constexpr double GroundToleranceCm = 30.0;
	constexpr double OverlapShrinkCm = 6.0;
	constexpr double MaxSnapDistanceCm = 150.0;
	/** P11: the canonical yaw unit. 144 steps per turn: 90 = 36, 60 = 24, 45 = 18, 22.5 = 9, 15 = 6, 7.5 = 3. */
	constexpr int32 YawSteps = 144;
	constexpr double YawStepDegrees = 2.5;
	constexpr int32 QuarterTurnSteps = 36;
	/** A socket's facing in data is absent when it equals this. */
	constexpr double NoFacing = 1.0e6;
	/** Facings oppose when they differ from 180 degrees by at most half a step. */
	constexpr double FacingToleranceDegrees = YawStepDegrees * 0.5;

	GRIDLANDSCORE_API int32 NormalizeYawStep(int32 Step);
	/** The nearest yaw step to Degrees. */
	GRIDLANDSCORE_API int32 YawStepFromDegrees(double Degrees);
	GRIDLANDSCORE_API double YawDegrees(int32 Step);
	/** True when Degrees is a whole number of yaw steps (data validation uses the same rule). */
	GRIDLANDSCORE_API bool IsWholeYawStep(double Degrees);
	/** Rotates an XY vector by a yaw step; exact for quarter turns. */
	GRIDLANDSCORE_API FVector2D RotateXY(const FVector2D& V, int32 Step);

	GRIDLANDSCORE_API FVector ToWorld(const FGLPlacedPiece& Piece, const TArray<double>& LocalMetres);
	GRIDLANDSCORE_API FGLFootprint Footprint(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece);
	/** The world box enclosing the piece's oriented footprint. */
	GRIDLANDSCORE_API FBox Bounds(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece);
	GRIDLANDSCORE_API TArray<FGLWorldSocket> Sockets(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece);

	/** True when every bottom socket of a ground-capable piece sits on the ground (within tolerance). */
	GRIDLANDSCORE_API bool RestsOnGround(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece, FGroundHeight Ground);

	/** Support of every piece (id -> value; <= 0 means unsupported). Deterministic. */
	GRIDLANDSCORE_API TMap<int32, double> ComputeSupport(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces, FGroundHeight Ground);

	/** Geometry and structure only (overlap, buried, support), with the preview colour of the result. */
	GRIDLANDSCORE_API FGLBuildCheck CheckPlacement(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
		const FGLPlacedPiece& Candidate, FGroundHeight Ground);
	/** Everything: knowledge, items (counted in Available), then CheckPlacement. */
	GRIDLANDSCORE_API FGLBuildCheck CanPlace(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
		const FGLPlacedPiece& Candidate, FGroundHeight Ground, const FGLKnowledge& Knowledge, TFunctionRef<int32(FName Item)> Available);
	GRIDLANDSCORE_API FGLBuildCheck CanPlace(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
		const FGLPlacedPiece& Candidate, FGroundHeight Ground, const FGLKnowledge& Knowledge, const FGLInventory& Inventory);
	/** The preview colour of a check (RED when refused; YELLOW at the material's limit; GREEN otherwise). */
	GRIDLANDSCORE_API EGLPreview PreviewOf(const FGLBuildCheck& Check);

	/**
	 * Pieces that would lose all support if RemovedId were taken away (not including RemovedId), sorted. The removal
	 * preview and the committed removal both call this: the prediction is the result.
	 */
	GRIDLANDSCORE_API TArray<int32> CollapsesAfterRemoving(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces,
		int32 RemovedId, FGroundHeight Ground);

	/**
	 * Where Def would go near Aim: aligned to the nearest compatible free socket (bottom onto top, side onto side) within
	 * MaxSnapDistanceCm that does not overlap; otherwise, for a ground-capable piece, on the ground at Aim. When both side
	 * sockets declare a facing, the candidate's yaw comes from the data (they must face each other), not from YawStep.
	 * Returns false when neither applies.
	 */
	GRIDLANDSCORE_API bool Snap(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing, FName Def,
		const FVector& Aim, int32 YawStep, FGroundHeight Ground, FGLPlacedPiece& OutCandidate, struct FGLSnapInfo* OutInfo = nullptr);

	/** Is this ground point under a piece that rests on the ground (terraforming must not move it)? Oriented. */
	GRIDLANDSCORE_API bool IsUnderStructure(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces,
		const FVector2D& World, double MarginCm = 50.0);
}
