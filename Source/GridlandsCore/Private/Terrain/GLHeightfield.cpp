#include "Terrain/GLHeightfield.h"

void FGLHeightfield::Init(const FVector2D& InOrigin, int32 InVertsX, int32 InVertsY, double InSpacingCm, float InBaseHeightCm,
	double InMaxDigDepthCm, double InMaxRaiseHeightCm)
{
	Origin = InOrigin;
	VertsX = FMath::Max(2, InVertsX);
	VertsY = FMath::Max(2, InVertsY);
	Spacing = InSpacingCm;
	BaseHeight = InBaseHeightCm;
	MaxDig = InMaxDigDepthCm;
	MaxRaise = InMaxRaiseHeightCm;
	Heights.Init(BaseHeight, VertsX * VertsY);
}

bool FGLHeightfield::Contains(const FVector2D& World) const
{
	const FVector2D Local = World - Origin;
	return Local.X >= 0.0 && Local.Y >= 0.0 && Local.X <= (VertsX - 1) * Spacing && Local.Y <= (VertsY - 1) * Spacing;
}

double FGLHeightfield::Falloff(double DistanceCm, double RadiusCm)
{
	const double T = FMath::Clamp(1.0 - DistanceCm / RadiusCm, 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

double FGLHeightfield::HeightAt(const FVector2D& World) const
{
	const FVector2D Local = World - Origin;
	const double FX = FMath::Clamp(Local.X / Spacing, 0.0, VertsX - 1.0);
	const double FY = FMath::Clamp(Local.Y / Spacing, 0.0, VertsY - 1.0);
	const int32 X0 = FMath::Min(FMath::FloorToInt(FX), VertsX - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt(FY), VertsY - 2);
	const double TX = FX - X0, TY = FY - Y0;
	const double Top = FMath::Lerp<double>(VertexHeight(X0, Y0), VertexHeight(X0 + 1, Y0), TX);
	const double Bottom = FMath::Lerp<double>(VertexHeight(X0, Y0 + 1), VertexHeight(X0 + 1, Y0 + 1), TX);
	return FMath::Lerp(Top, Bottom, TY);
}

FGLTerrainEditResult FGLHeightfield::Apply(const FGLTerrainEdit& Edit, TFunctionRef<bool(const FVector2D& World)> IsProtected)
{
	FGLTerrainEditResult Result;
	if (Edit.RadiusCm <= 0.0 || (Edit.Op != EGLTerrainOp::Flatten && Edit.AmountCm <= 0.0))
	{
		Result.Refusal = TEXT("empty edit");
		return Result;
	}
	const FVector2D Local = Edit.Centre - Origin;
	const int32 MinX = FMath::Max(0, FMath::FloorToInt((Local.X - Edit.RadiusCm) / Spacing));
	const int32 MaxX = FMath::Min(VertsX - 1, FMath::CeilToInt((Local.X + Edit.RadiusCm) / Spacing));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt((Local.Y - Edit.RadiusCm) / Spacing));
	const int32 MaxY = FMath::Min(VertsY - 1, FMath::CeilToInt((Local.Y + Edit.RadiusCm) / Spacing));
	if (MinX > MaxX || MinY > MaxY)
	{
		Result.Refusal = TEXT("outside this cell's ground");
		return Result;
	}

	// Plan every change first; commit only if the whole edit is allowed (atomic).
	struct FChange { int32 Index; float NewHeight; };
	TArray<FChange> Changes;
	double Nominal = 0.0, Actual = 0.0;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const FVector2D At = VertexLocation(X, Y);
			const double Weight = Falloff(FVector2D::Distance(At, Edit.Centre), Edit.RadiusCm);
			if (Weight <= 0.0)
			{
				continue;
			}
			const int32 Index = Y * VertsX + X;
			const double Old = Heights[Index];
			double New = Old;
			switch (Edit.Op)
			{
			case EGLTerrainOp::Dig:
				New = FMath::Max(Old - Edit.AmountCm * Weight, BaseHeight - MaxDig);
				Nominal += Edit.AmountCm * Weight;
				break;
			case EGLTerrainOp::Raise:
				New = FMath::Min(Old + Edit.AmountCm * Weight, BaseHeight + MaxRaise);
				Nominal += Edit.AmountCm * Weight;
				break;
			case EGLTerrainOp::Flatten:
				New = FMath::Lerp(Old, FMath::Clamp(Edit.TargetHeightCm, BaseHeight - MaxDig, BaseHeight + MaxRaise), Weight);
				Nominal += FMath::Abs(New - Old);
				break;
			}
			// Quantise to whole centimetres so saved deltas reproduce heights exactly.
			New = FMath::RoundToDouble(New);
			if (New == Old)
			{
				continue;
			}
			if (IsProtected(At))
			{
				Result.Refusal = TEXT("the ground here is holding up a structure");
				return Result;
			}
			Actual += FMath::Abs(New - Old);
			Changes.Add({ Index, static_cast<float>(New) });
		}
	}
	if (Changes.Num() == 0)
	{
		Result.Refusal = TEXT("nothing to change");
		return Result;
	}
	if (Edit.Op != EGLTerrainOp::Flatten && Actual < 0.5 * Nominal)
	{
		Result.Refusal = Edit.Op == EGLTerrainOp::Dig ? TEXT("too deep to dig further") : TEXT("too high to raise further");
		return Result;
	}

	const double CellAreaM2 = (Spacing / 100.0) * (Spacing / 100.0);
	Result.DirtyVertices = FIntRect(MaxX, MaxY, MinX, MinY);
	for (const FChange& Change : Changes)
	{
		Result.VolumeM3 += (Change.NewHeight - Heights[Change.Index]) / 100.0 * CellAreaM2;
		Heights[Change.Index] = Change.NewHeight;
		const int32 X = Change.Index % VertsX, Y = Change.Index / VertsX;
		Result.DirtyVertices.Min.X = FMath::Min(Result.DirtyVertices.Min.X, X);
		Result.DirtyVertices.Min.Y = FMath::Min(Result.DirtyVertices.Min.Y, Y);
		Result.DirtyVertices.Max.X = FMath::Max(Result.DirtyVertices.Max.X, X);
		Result.DirtyVertices.Max.Y = FMath::Max(Result.DirtyVertices.Max.Y, Y);
	}
	Result.bApplied = true;
	Result.VerticesChanged = Changes.Num();
	return Result;
}

void FGLHeightfield::EncodeDelta(TArray<int32>& OutIndices, TArray<int32>& OutDeltaCm) const
{
	OutIndices.Reset();
	OutDeltaCm.Reset();
	for (int32 Index = 0; Index < Heights.Num(); ++Index)
	{
		const int32 Delta = FMath::RoundToInt(Heights[Index] - BaseHeight);
		if (Delta != 0)
		{
			OutIndices.Add(Index);
			OutDeltaCm.Add(Delta);
		}
	}
}

bool FGLHeightfield::ApplyDelta(TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm)
{
	if (Indices.Num() != DeltaCm.Num())
	{
		return false;
	}
	for (int32 I = 0; I < Indices.Num(); ++I)
	{
		if (!Heights.IsValidIndex(Indices[I]) || FMath::Abs(DeltaCm[I]) > FMath::Max(MaxDig, MaxRaise) + 1.0)
		{
			return false;
		}
	}
	for (int32 I = 0; I < Indices.Num(); ++I)
	{
		Heights[Indices[I]] = BaseHeight + DeltaCm[I];
	}
	return true;
}
