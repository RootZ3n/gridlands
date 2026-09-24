#include "SpikeTerrain.h"

#include "DynamicMesh/MeshNormals.h"
#include "Misc/Compression.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	double SmoothFalloff(double Distance, double Radius)
	{
		const double T = FMath::Clamp(1.0 - Distance / Radius, 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T); // smoothstep
	}

	void AppendRecord(TArray<uint8>& Out, uint32 Index, int16 Value)
	{
		Out.Append(reinterpret_cast<const uint8*>(&Index), sizeof(Index));
		Out.Append(reinterpret_cast<const uint8*>(&Value), sizeof(Value));
	}

	void FinishMesh(FDynamicMesh3& Mesh)
	{
		Mesh.EnableAttributes();
		UE::Geometry::FMeshNormals::QuickComputeVertexNormals(Mesh);
		UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals(), true);
	}
}

void FSpikeHeightfield::Init(int32 InVertsPerSide, double InCellSize, float BaseHeight)
{
	VertsPerSide = InVertsPerSide;
	CellSize = InCellSize;
	Heights.Init(BaseHeight, VertsPerSide * VertsPerSide);
}

int32 FSpikeHeightfield::ApplyBrush(const FVector2D& Center, double Radius, double Delta)
{
	int32 Changed = 0;
	for (int32 Y = 0; Y < VertsPerSide; ++Y)
	{
		for (int32 X = 0; X < VertsPerSide; ++X)
		{
			const double Weight = SmoothFalloff(FVector2D::Distance(FVector2D(X * CellSize, Y * CellSize), Center), Radius);
			if (Weight > 0.0)
			{
				At(X, Y) += static_cast<float>(Delta * Weight);
				++Changed;
			}
		}
	}
	return Changed;
}

int32 FSpikeHeightfield::Flatten(const FVector2D& Center, double Radius, float TargetHeight)
{
	int32 Changed = 0;
	for (int32 Y = 0; Y < VertsPerSide; ++Y)
	{
		for (int32 X = 0; X < VertsPerSide; ++X)
		{
			const double Weight = SmoothFalloff(FVector2D::Distance(FVector2D(X * CellSize, Y * CellSize), Center), Radius);
			if (Weight > 0.0)
			{
				At(X, Y) = FMath::Lerp(At(X, Y), TargetHeight, static_cast<float>(Weight));
				++Changed;
			}
		}
	}
	return Changed;
}

double FSpikeHeightfield::HeightAt(const FVector2D& Local) const
{
	const double FX = FMath::Clamp(Local.X / CellSize, 0.0, VertsPerSide - 1.0);
	const double FY = FMath::Clamp(Local.Y / CellSize, 0.0, VertsPerSide - 1.0);
	const int32 X0 = FMath::Min(FMath::FloorToInt(FX), VertsPerSide - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt(FY), VertsPerSide - 2);
	const double TX = FX - X0, TY = FY - Y0;
	const double Top = FMath::Lerp<double>(At(X0, Y0), At(X0 + 1, Y0), TX);
	const double Bottom = FMath::Lerp<double>(At(X0, Y0 + 1), At(X0 + 1, Y0 + 1), TX);
	return FMath::Lerp(Top, Bottom, TY);
}

FDynamicMesh3 FSpikeHeightfield::BuildMesh() const
{
	FDynamicMesh3 Mesh;
	for (int32 Y = 0; Y < VertsPerSide; ++Y)
	{
		for (int32 X = 0; X < VertsPerSide; ++X)
		{
			Mesh.AppendVertex(FVector3d(X * CellSize, Y * CellSize, At(X, Y)));
		}
	}
	for (int32 Y = 0; Y + 1 < VertsPerSide; ++Y)
	{
		for (int32 X = 0; X + 1 < VertsPerSide; ++X)
		{
			const int32 A = Y * VertsPerSide + X, B = A + 1, C = A + VertsPerSide, D = C + 1;
			Mesh.AppendTriangle(A, C, B); // upward-facing in UE's left-handed space
			Mesh.AppendTriangle(B, C, D);
		}
	}
	FinishMesh(Mesh);
	return Mesh;
}

TArray<uint8> FSpikeHeightfield::EncodeDelta(const FSpikeHeightfield& Base) const
{
	TArray<uint8> Out;
	for (int32 Index = 0; Index < Heights.Num(); ++Index)
	{
		const int32 Centimetres = FMath::RoundToInt(Heights[Index] - Base.Heights[Index]);
		if (Centimetres != 0)
		{
			AppendRecord(Out, Index, static_cast<int16>(FMath::Clamp(Centimetres, -32768, 32767)));
		}
	}
	return Out;
}

bool FSpikeHeightfield::ApplyDelta(const TArray<uint8>& Delta)
{
	constexpr int32 Record = sizeof(uint32) + sizeof(int16);
	if (Delta.Num() % Record != 0)
	{
		return false;
	}
	for (int32 Offset = 0; Offset < Delta.Num(); Offset += Record)
	{
		uint32 Index;
		int16 Value;
		FMemory::Memcpy(&Index, Delta.GetData() + Offset, sizeof(Index));
		FMemory::Memcpy(&Value, Delta.GetData() + Offset + sizeof(Index), sizeof(Value));
		if (!Heights.IsValidIndex(Index))
		{
			return false;
		}
		Heights[Index] += Value;
	}
	return true;
}

void FSpikeVoxels::InitGround(int32 InN, double InVoxelSize, double GroundHeight)
{
	N = InN;
	VoxelSize = InVoxelSize;
	Density.SetNumUninitialized(N * N * N);
	for (int32 Z = 0; Z < N; ++Z)
	{
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				At(X, Y, Z) = static_cast<float>((GroundHeight - Z * VoxelSize) / VoxelSize); // signed distance in voxels
			}
		}
	}
}

int32 FSpikeVoxels::ApplySphere(const FVector& Center, double Radius, bool bAdd)
{
	int32 Changed = 0;
	for (int32 Z = 0; Z < N; ++Z)
	{
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const double SphereDistance = (Radius - FVector::Distance(FVector(X, Y, Z) * VoxelSize, Center)) / VoxelSize; // >0 inside
				float& D = At(X, Y, Z);
				const float Before = D;
				D = bAdd ? FMath::Max(D, static_cast<float>(SphereDistance)) : FMath::Min(D, static_cast<float>(-SphereDistance));
				Changed += (D != Before) ? 1 : 0;
			}
		}
	}
	return Changed;
}

FDynamicMesh3 FSpikeVoxels::BuildSurfaceNets() const
{
	FDynamicMesh3 Mesh;
	const int32 C = N - 1; // cells per side
	TArray<int32> CellVertex;
	CellVertex.Init(INDEX_NONE, C * C * C);
	auto CellIndex = [C](int32 X, int32 Y, int32 Z) { return (Z * C + Y) * C + X; };

	// One vertex per cell that the surface crosses, at the mean of its edge crossings.
	static const int32 Edges[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
	for (int32 Z = 0; Z < C; ++Z)
	{
		for (int32 Y = 0; Y < C; ++Y)
		{
			for (int32 X = 0; X < C; ++X)
			{
				float Corner[8];
				FVector3d CornerPos[8];
				int32 Solid = 0;
				for (int32 I = 0; I < 8; ++I)
				{
					const int32 DX = I & 1, DY = (I >> 1) & 1, DZ = (I >> 2) & 1;
					Corner[I] = At(X + DX, Y + DY, Z + DZ);
					CornerPos[I] = FVector3d(X + DX, Y + DY, Z + DZ) * VoxelSize;
					Solid += Corner[I] > 0.f ? 1 : 0;
				}
				if (Solid == 0 || Solid == 8)
				{
					continue;
				}
				FVector3d Sum = FVector3d::Zero();
				int32 Crossings = 0;
				for (const auto& E : Edges)
				{
					const float A = Corner[E[0]], B = Corner[E[1]];
					if ((A > 0.f) != (B > 0.f))
					{
						const double T = A / (A - B);
						Sum += CornerPos[E[0]] + (CornerPos[E[1]] - CornerPos[E[0]]) * T;
						++Crossings;
					}
				}
				CellVertex[CellIndex(X, Y, Z)] = Mesh.AppendVertex(Sum / Crossings);
			}
		}
	}

	// One quad per grid edge with a sign change, joining the four cells around that edge.
	auto EmitQuad = [&Mesh](int32 V0, int32 V1, int32 V2, int32 V3, bool bFlip)
	{
		if (V0 == INDEX_NONE || V1 == INDEX_NONE || V2 == INDEX_NONE || V3 == INDEX_NONE)
		{
			return;
		}
		if (bFlip)
		{
			Mesh.AppendTriangle(V0, V2, V1);
			Mesh.AppendTriangle(V0, V3, V2);
		}
		else
		{
			Mesh.AppendTriangle(V0, V1, V2);
			Mesh.AppendTriangle(V0, V2, V3);
		}
	};
	for (int32 Z = 1; Z < C; ++Z)
	{
		for (int32 Y = 1; Y < C; ++Y)
		{
			for (int32 X = 1; X < C; ++X)
			{
				const bool S = At(X, Y, Z) > 0.f;
				if (S != (At(X + 1, Y, Z) > 0.f)) // x edge: cells around it vary in y,z
				{
					EmitQuad(CellVertex[CellIndex(X, Y - 1, Z - 1)], CellVertex[CellIndex(X, Y, Z - 1)], CellVertex[CellIndex(X, Y, Z)], CellVertex[CellIndex(X, Y - 1, Z)], S);
				}
				if (S != (At(X, Y + 1, Z) > 0.f)) // y edge: cells vary in x,z
				{
					EmitQuad(CellVertex[CellIndex(X - 1, Y, Z - 1)], CellVertex[CellIndex(X - 1, Y, Z)], CellVertex[CellIndex(X, Y, Z)], CellVertex[CellIndex(X, Y, Z - 1)], S);
				}
				if (S != (At(X, Y, Z + 1) > 0.f)) // z edge: cells vary in x,y
				{
					EmitQuad(CellVertex[CellIndex(X - 1, Y - 1, Z)], CellVertex[CellIndex(X, Y - 1, Z)], CellVertex[CellIndex(X, Y, Z)], CellVertex[CellIndex(X - 1, Y, Z)], S);
				}
			}
		}
	}
	FinishMesh(Mesh);
	return Mesh;
}

TArray<uint8> FSpikeVoxels::EncodeDelta(const FSpikeVoxels& Base) const
{
	TArray<uint8> Out;
	for (int32 Index = 0; Index < Density.Num(); ++Index)
	{
		if (Density[Index] != Base.Density[Index])
		{
			AppendRecord(Out, Index, static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Density[Index] * 256.f), -32768, 32767)));
		}
	}
	return Out;
}

int32 SpikeTerrain::CompressedSize(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() == 0)
	{
		return 0;
	}
	int32 Size = FCompression::CompressMemoryBound(NAME_Zlib, Bytes.Num());
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(Size);
	return FCompression::CompressMemory(NAME_Zlib, Buffer.GetData(), Size, Bytes.GetData(), Bytes.Num()) ? Size : -1;
}
