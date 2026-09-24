#pragma once

// SPIKE S1 (throwaway, branch spike/terrain): two candidate runtime-deformable terrain
// representations, kept pure (no world) so they can be measured and tested headless.

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

/** (B) Chunked heightfield: one height per grid vertex. Cannot represent overhangs or caves. */
struct GRIDLANDSTERRAINSPIKE_API FSpikeHeightfield
{
	int32 VertsPerSide = 0;
	double CellSize = 100.0; // cm
	TArray<float> Heights;   // cm, row-major

	void Init(int32 InVertsPerSide, double InCellSize, float BaseHeight);
	float& At(int32 X, int32 Y) { return Heights[Y * VertsPerSide + X]; }
	float At(int32 X, int32 Y) const { return Heights[Y * VertsPerSide + X]; }

	/** Smooth circular brush; Delta < 0 digs, > 0 raises. Returns vertices changed. */
	int32 ApplyBrush(const FVector2D& Center, double Radius, double Delta);
	/** Flattens toward TargetHeight inside Radius (building foundations). Returns vertices changed. */
	int32 Flatten(const FVector2D& Center, double Radius, float TargetHeight);
	/** Bilinear height query in local cm, no physics needed. */
	double HeightAt(const FVector2D& Local) const;

	UE::Geometry::FDynamicMesh3 BuildMesh() const;

	/** Sparse save delta against Base: (uint32 index, int16 centimetre delta) per changed vertex. */
	TArray<uint8> EncodeDelta(const FSpikeHeightfield& Base) const;
	bool ApplyDelta(const TArray<uint8>& Delta);
};

/** (C) Voxel density chunk (density > 0 is solid), surface extracted with Surface Nets. Supports tunnels and overhangs. */
struct GRIDLANDSTERRAINSPIKE_API FSpikeVoxels
{
	int32 N = 0;              // voxels per side (N^3 samples)
	double VoxelSize = 100.0; // cm
	TArray<float> Density;

	void InitGround(int32 InN, double InVoxelSize, double GroundHeight);
	float& At(int32 X, int32 Y, int32 Z) { return Density[(Z * N + Y) * N + X]; }
	float At(int32 X, int32 Y, int32 Z) const { return Density[(Z * N + Y) * N + X]; }

	/** Adds (bAdd) or removes material in a sphere (local cm). Returns samples changed. */
	int32 ApplySphere(const FVector& Center, double Radius, bool bAdd);

	UE::Geometry::FDynamicMesh3 BuildSurfaceNets() const;

	/** Sparse save delta against Base: (uint32 index, int16 quantized density) per changed sample. */
	TArray<uint8> EncodeDelta(const FSpikeVoxels& Base) const;
};

namespace SpikeTerrain
{
	/** zlib size of Bytes, to estimate save cost. */
	GRIDLANDSTERRAINSPIKE_API int32 CompressedSize(const TArray<uint8>& Bytes);
}
