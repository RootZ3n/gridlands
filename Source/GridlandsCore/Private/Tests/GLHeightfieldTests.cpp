#include "Misc/AutomationTest.h"
#include "Terrain/GLHeightfield.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLHeightfieldTests
{
	constexpr EAutomationTestFlags TerrainFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FGLHeightfield Flat()
	{
		FGLHeightfield Field;
		Field.Init(FVector2D(-3200, -3200), 65, 65, 100.0, 0.f, 300.0, 300.0);
		return Field;
	}

	FGLTerrainEdit Edit(EGLTerrainOp Op, FVector2D At, double Radius, double Amount, double Target = 0.0)
	{
		FGLTerrainEdit E;
		E.Op = Op;
		E.Centre = At;
		E.RadiusCm = Radius;
		E.AmountCm = Amount;
		E.TargetHeightCm = Target;
		return E;
	}
}

using namespace GLHeightfieldTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLHeightfieldOps, "Gridlands.Core.Terrain.DigRaiseFlatten", TerrainFlags)
bool FGLHeightfieldOps::RunTest(const FString& Parameters)
{
	FGLHeightfield Field = Flat();
	TestEqual(TEXT("flat base"), Field.HeightAt(FVector2D(123, -456)), 0.0);

	FGLTerrainEditResult Dug = Field.Apply(Edit(EGLTerrainOp::Dig, FVector2D(0, 0), 300.0, 100.0));
	TestTrue(TEXT("dig applied"), Dug.bApplied);
	TestEqual(TEXT("full depth at the centre"), Field.HeightAt(FVector2D(0, 0)), -100.0);
	TestEqual(TEXT("untouched outside the radius"), Field.HeightAt(FVector2D(400, 0)), 0.0);
	TestTrue(TEXT("volume is negative when digging"), Dug.VolumeM3 < 0.0);
	TestTrue(TEXT("dirty rect covers the centre vertex (32,32)"), Dug.DirtyVertices.Min.X <= 32 && Dug.DirtyVertices.Max.X >= 32 && Dug.DirtyVertices.Min.Y <= 32 && Dug.DirtyVertices.Max.Y >= 32);

	FGLTerrainEditResult Raised = Field.Apply(Edit(EGLTerrainOp::Raise, FVector2D(0, 0), 300.0, 100.0));
	TestTrue(TEXT("raise applied"), Raised.bApplied);
	TestEqual(TEXT("raise undoes an equal dig exactly (whole centimetres)"), Field.HeightAt(FVector2D(0, 0)), 0.0);
	TestTrue(TEXT("volumes cancel"), FMath::IsNearlyEqual(Dug.VolumeM3, -Raised.VolumeM3, 1e-6));

	Field.Apply(Edit(EGLTerrainOp::Raise, FVector2D(1000, 1000), 400.0, 150.0));
	FGLTerrainEditResult Flattened = Field.Apply(Edit(EGLTerrainOp::Flatten, FVector2D(1000, 1000), 600.0, 0.0, 0.0));
	TestTrue(TEXT("flatten applied"), Flattened.bApplied);
	TestEqual(TEXT("flatten reaches the target at the centre"), Field.HeightAt(FVector2D(1000, 1000)), 0.0);
	TestTrue(TEXT("flatten pulls the mound down near the centre"), Field.HeightAt(FVector2D(1100, 1000)) < 20.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLHeightfieldRefusals, "Gridlands.Core.Terrain.RefusalsAreAtomic", TerrainFlags)
bool FGLHeightfieldRefusals::RunTest(const FString& Parameters)
{
	FGLHeightfield Field = Flat();
	// A structure protects the ground under it: the whole stroke is refused, nothing moves.
	const FGLTerrainEditResult Protected = Field.Apply(Edit(EGLTerrainOp::Dig, FVector2D(0, 0), 300.0, 100.0),
		[](const FVector2D& At) { return At.X >= 200.0; });
	TestFalse(TEXT("refused under a structure"), Protected.bApplied);
	TestFalse(TEXT("with a reason"), Protected.Refusal.IsEmpty());
	TestEqual(TEXT("and no vertex moved, even the unprotected ones"), Field.HeightAt(FVector2D(0, 0)), 0.0);

	// Depth limit: after the limit, a stroke that would move < half its nominal volume is refused (no free soil).
	FGLTerrainEditResult Stroke;
	int32 Strokes = 0;
	for (; Strokes < 20; ++Strokes)
	{
		Stroke = Field.Apply(Edit(EGLTerrainOp::Dig, FVector2D(0, 0), 300.0, 100.0));
		if (!Stroke.bApplied)
		{
			break;
		}
	}
	TestTrue(FString::Printf(TEXT("digging stops at the limit (after %d strokes)"), Strokes), Strokes >= 3 && Strokes < 20);
	TestEqual(TEXT("never below max depth"), Field.HeightAt(FVector2D(0, 0)), -300.0);
	TestEqual(TEXT("refused as too deep"), Stroke.Refusal, FString(TEXT("too deep to dig further")));
	const float Before = Field.VertexHeight(33, 32);
	TestFalse(TEXT("a stroke into the floor stays refused"), Field.Apply(Edit(EGLTerrainOp::Dig, FVector2D(0, 0), 300.0, 100.0)).bApplied);
	TestEqual(TEXT("and changes nothing"), Field.VertexHeight(33, 32), Before);
	TestFalse(TEXT("outside the cell is refused"), Field.Apply(Edit(EGLTerrainOp::Raise, FVector2D(90000, 0), 300.0, 100.0)).bApplied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLHeightfieldDelta, "Gridlands.Core.Terrain.SaveDeltaReproducesExactly", TerrainFlags)
bool FGLHeightfieldDelta::RunTest(const FString& Parameters)
{
	FGLHeightfield Edited = Flat();
	Edited.Apply(Edit(EGLTerrainOp::Dig, FVector2D(-500, 300), 350.0, 120.0));
	Edited.Apply(Edit(EGLTerrainOp::Raise, FVector2D(900, -800), 500.0, 200.0));
	Edited.Apply(Edit(EGLTerrainOp::Flatten, FVector2D(900, -800), 300.0, 0.0, 40.0));
	TArray<int32> Indices, Deltas;
	Edited.EncodeDelta(Indices, Deltas);
	TestTrue(TEXT("sparse: far fewer entries than vertices"), Indices.Num() > 0 && Indices.Num() < 65 * 65 / 4);

	FGLHeightfield Restored = Flat();
	TestTrue(TEXT("delta applies"), Restored.ApplyDelta(Indices, Deltas));
	bool bIdentical = true;
	for (int32 Y = 0; Y < 65; ++Y)
	{
		for (int32 X = 0; X < 65; ++X)
		{
			bIdentical &= Restored.VertexHeight(X, Y) == Edited.VertexHeight(X, Y);
		}
	}
	TestTrue(TEXT("restored heights are bit-identical"), bIdentical);

	FGLHeightfield Untouched = Flat();
	TestFalse(TEXT("an out-of-range index is refused"), Untouched.ApplyDelta(TArray<int32>{ 999999 }, TArray<int32>{ 5 }));
	TestFalse(TEXT("an absurd delta is refused"), Untouched.ApplyDelta(TArray<int32>{ 10 }, TArray<int32>{ 99999 }));
	TestFalse(TEXT("mismatched arrays are refused"), Untouched.ApplyDelta(TArray<int32>{ 10, 11 }, TArray<int32>{ 5 }));
	TestEqual(TEXT("and a refused delta changes nothing"), Untouched.VertexHeight(10, 0), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLHeightfieldAuthored, "Gridlands.Core.Terrain.AuthoredBaseIsTheReference", TerrainFlags)
bool FGLHeightfieldAuthored::RunTest(const FString& Parameters)
{
	TArray<float> Hills = GLTerrainGen::Rolling(7, 65, 65, 100.0, 1500.0);
	TestEqual(TEXT("generator is deterministic"), Hills, GLTerrainGen::Rolling(7, 65, 65, 100.0, 1500.0));
	float Low = 1e9f, High = -1e9f;
	for (const float H : Hills) { Low = FMath::Min(Low, H); High = FMath::Max(High, H); }
	TestTrue(FString::Printf(TEXT("representative relief (%.0f..%.0f cm)"), Low, High), High - Low > 800.0f);

	FGLHeightfield Field = Flat();
	TArray<float> Base = Hills;
	TestTrue(TEXT("base accepted"), Field.SetBase(MoveTemp(Base)));
	TestFalse(TEXT("a wrong-sized base is refused"), Field.SetBase(TArray<float>{ 1.f, 2.f }));
	TArray<int32> Indices, Deltas;
	Field.EncodeDelta(Indices, Deltas);
	TestEqual(TEXT("untouched authored ground saves nothing"), Indices.Num(), 0);

	const float Before = Field.VertexHeight(32, 32);
	for (int32 Stroke = 0; Stroke < 20 && Field.Apply(Edit(EGLTerrainOp::Dig, FVector2D(0, 0), 300.0, 100.0)).bApplied; ++Stroke) {}
	TestEqual(TEXT("depth limit is relative to the authored base"), Field.VertexHeight(32, 32), FMath::RoundToFloat(Before) - 300.f);
	Field.EncodeDelta(Indices, Deltas);
	FGLHeightfield Restored = Flat();
	TArray<float> Again = GLTerrainGen::Rolling(7, 65, 65, 100.0, 1500.0);
	Restored.SetBase(MoveTemp(Again));
	TestTrue(TEXT("delta restores onto the same authored base"), Restored.ApplyDelta(Indices, Deltas));
	TestEqual(TEXT("exactly"), Restored.VertexHeight(32, 32), Field.VertexHeight(32, 32));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
