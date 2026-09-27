#pragma once

#include "CoreMinimal.h"

/** What a terrain operation does (ADR-0022). Player terraforming is a heightfield: no caves. */
enum class EGLTerrainOp : uint8
{
	Dig,     // lower by Amount (cm) at the centre, smooth falloff to Radius
	Raise,   // raise by Amount (cm)
	Flatten, // move toward TargetHeight (cm)
};

/**
 * THE terrain surface (ADR-0035): the stored vertex heights plus one triangle diagonal define one surface,
 * and rendering, collision and gameplay queries all represent it.
 *
 * Quad (x, y) has corners A = (x, y), B = (x+1, y), C = (x, y+1), D = (x+1, y+1). It is split along A-D,
 * the diagonal a Chaos heightfield uses (its collision cannot be told otherwise): triangle (A, C, D) where
 * FY >= FX, triangle (A, D, B) where FY < FX. Inside each triangle the height is that triangle's plane.
 * FX, FY are the position inside the quad in [0, 1].
 */
namespace GLTerrainSurface
{
	/** The surface height inside a quad from its corner heights (A, B, C, D as above). */
	inline double Height(double HA, double HB, double HC, double HD, double FX, double FY)
	{
		return FY >= FX ? HA + FX * (HD - HC) + FY * (HC - HA)   // triangle (A, C, D)
		                : HA + FX * (HB - HA) + FY * (HD - HB);  // triangle (A, D, B)
	}

	/** The quad's two triangles as vertex indices, upward-facing in UE's left-handed space: (A, C, D), (A, D, B). */
	inline void Triangles(int32 A, int32 B, int32 C, int32 D, int32 Out[6])
	{
		Out[0] = A; Out[1] = C; Out[2] = D;
		Out[3] = A; Out[4] = D; Out[5] = B;
	}
}

struct GRIDLANDSCORE_API FGLTerrainEdit
{
	EGLTerrainOp Op = EGLTerrainOp::Dig;
	FVector2D Centre = FVector2D::ZeroVector; // world cm
	double RadiusCm = 200.0;
	double AmountCm = 50.0;       // Dig / Raise
	double TargetHeightCm = 0.0;  // Flatten
};

struct GRIDLANDSCORE_API FGLTerrainEditResult
{
	bool bApplied = false;
	/** Why nothing changed (empty when applied). */
	FString Refusal;
	/** Inclusive vertex rectangle that changed (for chunk rebuilds); empty when nothing changed. */
	FIntRect DirtyVertices;
	int32 VerticesChanged = 0;
	/** Signed volume moved, m^3 (negative when digging). */
	double VolumeM3 = 0.0;
};

/**
 * One Grid cell's ground as a regular height grid (ADR-0022). Pure and deterministic: the same
 * edits give the same heights, bit for bit, so saves store only sparse deltas from the base.
 * Rendering, collision and navigation live in GridlandsGame and follow DirtyVertices.
 */
class GRIDLANDSCORE_API FGLHeightfield
{
public:
	/** Flat base: VertsX x VertsY vertices, Spacing cm apart, vertex (0,0) at Origin (world cm). */
	void Init(const FVector2D& InOrigin, int32 InVertsX, int32 InVertsY, double InSpacingCm, float InBaseHeightCm,
		double InMaxDigDepthCm, double InMaxRaiseHeightCm);

	/**
	 * Authored ground: Base holds one height per vertex (cm, row-major, VertsX * VertsY). Edit
	 * limits and saved deltas are relative to each vertex's base. False (unchanged) on a size mismatch.
	 */
	bool SetBase(TArray<float>&& InBase);
	bool HasAuthoredBase() const { return Base.Num() > 0; }
	float VertexBase(int32 Index) const { return Base.Num() > 0 ? Base[Index] : BaseHeight; }
	SIZE_T GetAllocatedBytes() const { return Heights.GetAllocatedSize() + Base.GetAllocatedSize(); }

	int32 GetVertsX() const { return VertsX; }
	int32 GetVertsY() const { return VertsY; }
	double GetSpacing() const { return Spacing; }
	FVector2D GetOrigin() const { return Origin; }
	float GetBaseHeight() const { return BaseHeight; }
	float VertexHeight(int32 X, int32 Y) const { return Heights[Y * VertsX + X]; }
	FVector2D VertexLocation(int32 X, int32 Y) const { return Origin + FVector2D(X * Spacing, Y * Spacing); }
	bool Contains(const FVector2D& World) const;

	/**
	 * Ground height at a world point (cm): THE terrain surface (GLTerrainSurface), the same one that is drawn
	 * and that collides. Exactly the stored height at every vertex; clamps to the edge outside. No physics.
	 */
	double HeightAt(const FVector2D& World) const;
	/** How far the nearest vertex has been moved from its base (cm, >= 0): dug or raised ground (P7). */
	double EditedAt(const FVector2D& World) const;

	/**
	 * Applies an edit atomically. Refused, changing nothing, when:
	 *  - IsProtected returns true for any vertex the edit would move (e.g. under a structure);
	 *  - the depth/height limits leave less than half the nominal change (no free strokes).
	 */
	FGLTerrainEditResult Apply(const FGLTerrainEdit& Edit, TFunctionRef<bool(const FVector2D& World)> IsProtected, bool bDryRun = false);
	FGLTerrainEditResult Apply(const FGLTerrainEdit& Edit) { return Apply(Edit, [](const FVector2D&) { return false; }); }

	/** Sparse difference from the flat base: vertex index -> whole centimetres. Deterministic order. */
	void EncodeDelta(TArray<int32>& OutIndices, TArray<int32>& OutDeltaCm) const;
	/** Every vertex back to the base height (limits and layout unchanged). */
	void ResetToBase() { for (int32 I = 0; I < Heights.Num(); ++I) { Heights[I] = VertexBase(I); } }
	/** Restores a saved delta onto a freshly initialised base. False (and no change) if malformed. */
	bool ApplyDelta(TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm);

	/** Smoothstep falloff used by every brush: 1 at the centre, 0 at Radius. */
	static double Falloff(double DistanceCm, double RadiusCm);

private:
	FVector2D Origin = FVector2D::ZeroVector;
	int32 VertsX = 0;
	int32 VertsY = 0;
	double Spacing = 100.0;
	float BaseHeight = 0.f;
	double MaxDig = 300.0;
	double MaxRaise = 300.0;
	TArray<float> Heights;
	TArray<float> Base; // empty: flat at BaseHeight
};

/** Deterministic authored-ground generators (tests, tools, performance harnesses). */
namespace GLTerrainGen
{
	/**
	 * Rolling hills: value noise, four octaves, about +/- AmplitudeCm, with a few steeper ridges.
	 * The same seed gives the same heights on every machine.
	 */
	GRIDLANDSCORE_API TArray<float> Rolling(int32 Seed, int32 VertsX, int32 VertsY, double SpacingCm, double AmplitudeCm);
}
