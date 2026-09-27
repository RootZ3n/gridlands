// Terrain collision agreement (heightfield-collision spike, after ADR-0034). What the player SEES (the
// chunk's render mesh, read back from its component) and what physics ANSWERS (line traces, small-sphere
// sweeps, overlaps: the paths movement and aiming use) must be the same surface, and both must be the
// authoritative heights (the field), through edits, seams, repetition, restore and streaming.
// Runs in whichever collision mode the process has: the canonical component trimesh, or the spike's
// -GLTerrainCollision=1 (heightfield) / =2 (worker trimesh). The same test is the control for all three.

#include "CollisionQueryParams.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Terrain/GLHeightfield.h"
#include "Terrain/GLTerrainChunk.h"
#include "Terrain/GLTerrainCollision.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLTerrainCollisionTests
{
	const FName TCollisionCell(TEXT("cell.home.origin"));

	struct FCollisionWorst
	{
		double Trace = 0.0;       // |collision (trace) - visible| cm
		double Sweep = 0.0;       // |sweep contact - visible surface at the contact| cm
		double Visible = 0.0;     // |visible - field (same triangles)| cm
		int32 Missing = 0;        // probes where physics found no ground
		int32 OverlapWrong = 0;   // embedded probe not overlapping, or clear probe overlapping
		int32 Probes = 0;
		TArray<FString> MissingWhere; // the first few misses: where, and which query
		int32 CapsuleMissing = 0;     // a character-sized capsule (34 x 88 cm) swept down found nothing
		int32 BigSphereMissing = 0;   // a 34 cm sphere swept down found nothing
	};

	struct FCollisionScene
	{
		GLTestUtils::FTestWorld Test;
		UGLTerrainSubsystem* Terrain = nullptr;

		explicit FCollisionScene(const TCHAR* Name) : Test(Name)
		{
			Terrain = Test.World->GetSubsystem<UGLTerrainSubsystem>();
			Terrain->BeginCellGround(TCollisionCell, {}, {});
			Terrain->FlushAll();
		}

		const FGLHeightfield& Field() const { return *Terrain->FieldOf(TCollisionCell); }

		/** A chunk corner well inside the cell (chunk seams run through it on both axes). */
		FVector2D Corner() const { return Field().VertexLocation(64 * 8, 64 * 8); }

		AGLTerrainChunk* ChunkAt(const FVector2D& At) const
		{
			for (TActorIterator<AGLTerrainChunk> It(Test.World); It; ++It)
			{
				if (It->State != EGLChunkState::Live || !It->IsBuilt())
				{
					continue;
				}
				const FVector2D L = At - FVector2D(It->GetActorLocation());
				const double Size = (It->GetVertsPerSide() - 1) * Field().GetSpacing();
				if (L.X >= 0.0 && L.Y >= 0.0 && L.X < Size && L.Y < Size)
				{
					return *It;
				}
			}
			return nullptr;
		}

		/** The render mesh's surface split as it is drawn: quad (x,y) is triangles (A,C,B) and (B,C,D). */
		static double TriangleZ(double ZA, double ZB, double ZC, double ZD, double FX, double FY)
		{
			if (GLTerrainCollision::RenderSplitsMainDiagonal()) // heightfield spike: split A-D, triangles (A,C,D) and (A,D,B)
			{
				return FY >= FX ? ZA + FX * (ZD - ZC) + FY * (ZC - ZA) : ZA + FX * (ZB - ZA) + FY * (ZD - ZB);
			}
			return FX + FY <= 1.0 ? ZA + FX * (ZB - ZA) + FY * (ZC - ZA) : ZD + (1.0 - FX) * (ZC - ZD) + (1.0 - FY) * (ZB - ZD);
		}

		/** What the player sees: the chunk component's own mesh (NAN if no chunk shows there). */
		double VisibleZ(const FVector2D& At) const
		{
			const AGLTerrainChunk* Chunk = ChunkAt(At);
			if (!Chunk)
			{
				return NAN;
			}
			const UE::Geometry::FDynamicMesh3* Mesh = Chunk->GetMesh()->GetMesh();
			const int32 V = Chunk->GetVertsPerSide();
			if (Mesh->VertexCount() != V * V)
			{
				return NAN;
			}
			const double S = Field().GetSpacing();
			const FVector2D L = (At - FVector2D(Chunk->GetActorLocation())) / S;
			const int32 X = FMath::Clamp(FMath::FloorToInt(L.X), 0, V - 2), Y = FMath::Clamp(FMath::FloorToInt(L.Y), 0, V - 2);
			auto Z = [Mesh, V](int32 PX, int32 PY) { return Mesh->GetVertex(PY * V + PX).Z; };
			return TriangleZ(Z(X, Y), Z(X + 1, Y), Z(X, Y + 1), Z(X + 1, Y + 1), L.X - X, L.Y - Y);
		}

		/** The authoritative heights, triangulated as drawn. */
		double FieldTriangleZ(const FVector2D& At) const
		{
			const FGLHeightfield& F = Field();
			const FVector2D L = (At - F.VertexLocation(0, 0)) / F.GetSpacing();
			const int32 X = FMath::Clamp(FMath::FloorToInt(L.X), 0, F.GetVertsX() - 2), Y = FMath::Clamp(FMath::FloorToInt(L.Y), 0, F.GetVertsY() - 2);
			return TriangleZ(F.VertexHeight(X, Y), F.VertexHeight(X + 1, Y), F.VertexHeight(X, Y + 1), F.VertexHeight(X + 1, Y + 1), L.X - X, L.Y - Y);
		}

		double TraceZ(const FVector2D& At) const
		{
			FHitResult Hit;
			return Test.World->LineTraceSingleByChannel(Hit, FVector(At, 60000.0), FVector(At, -60000.0), ECC_Visibility) ? Hit.ImpactPoint.Z : NAN;
		}

		/**
		 * A 10 cm sphere swept down (the sweep path movement uses; it skips heightfield holes). On a slope it
		 * touches beside the probe, so the answer is how far its contact is from the visible surface THERE.
		 */
		double SweepError(const FVector2D& At) const
		{
			FHitResult Hit;
			if (!Test.World->SweepSingleByChannel(Hit, FVector(At, 60000.0), FVector(At, -60000.0), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(10.0f)))
			{
				return NAN;
			}
			const double There = VisibleZ(FVector2D(Hit.ImpactPoint));
			return FMath::IsNaN(There) ? NAN : FMath::Abs(Hit.ImpactPoint.Z - There);
		}

		bool SweepFinds(const FVector2D& At, const FCollisionShape& Shape) const
		{
			FHitResult Hit;
			return Test.World->SweepSingleByChannel(Hit, FVector(At, 60000.0), FVector(At, -60000.0), FQuat::Identity, ECC_Pawn, Shape);
		}

		bool OverlapsAt(const FVector& Where) const
		{
			return Test.World->OverlapAnyTestByChannel(Where, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(4.0f));
		}

		/** Checks every probe in the square (half-size HalfCm around Centre, step 37 cm, off the vertex grid) and on seams. */
		FCollisionWorst Check(const FVector2D& Centre, double HalfCm) const
		{
			TArray<FVector2D> Probes;
			for (double Y = -HalfCm; Y <= HalfCm; Y += 37.0)
			{
				for (double X = -HalfCm; X <= HalfCm; X += 37.0)
				{
					Probes.Add(Centre + FVector2D(X, Y));
				}
			}
			// Right on and beside the seams through the corner.
			const FVector2D C = Corner();
			for (double T = -HalfCm; T <= HalfCm; T += 50.0)
			{
				for (const double Off : { -1.0, -0.01, 0.0, 0.01, 1.0 })
				{
					Probes.Add(FVector2D(C.X + Off, Centre.Y + T));
					Probes.Add(FVector2D(Centre.X + T, C.Y + Off));
				}
			}
			FCollisionWorst W;
			for (const FVector2D& P : Probes)
			{
				const double Vis = VisibleZ(P), Tri = FieldTriangleZ(P), Tr = TraceZ(P), Sw = SweepError(P);
				++W.Probes;
				W.CapsuleMissing += SweepFinds(P, FCollisionShape::MakeCapsule(34.0f, 88.0f)) ? 0 : 1;
				W.BigSphereMissing += SweepFinds(P, FCollisionShape::MakeSphere(34.0f)) ? 0 : 1;
				if (FMath::IsNaN(Vis) || FMath::IsNaN(Tr) || FMath::IsNaN(Sw))
				{
					if (W.MissingWhere.Num() < 6)
					{
						const FVector2D L = (P - Field().VertexLocation(0, 0)) / Field().GetSpacing();
						W.MissingWhere.Add(FString::Printf(TEXT("(%.2f, %.2f) vertex-space (%.3f, %.3f): visible %.1f trace %.1f sweep %.1f"), P.X, P.Y, L.X, L.Y, Vis, Tr, Sw));
					}
					++W.Missing;
					continue;
				}
				W.Visible = FMath::Max(W.Visible, FMath::Abs(Vis - Tri));
				W.Trace = FMath::Max(W.Trace, FMath::Abs(Tr - Vis));
				W.Sweep = FMath::Max(W.Sweep, Sw);
				// Collision is a surface, not a solid: a sphere straddling it must touch; one clear above must not
				// (seams, holes, stale bodies).
				if (!OverlapsAt(FVector(P, Vis - 2.0)) || OverlapsAt(FVector(P, Vis + 30.0)))
				{
					++W.OverlapWrong;
				}
			}
			return W;
		}

		void Expect(FAutomationTestBase& T, const TCHAR* Step, const FCollisionWorst& W) const
		{
			T.AddInfo(FString::Printf(TEXT("[%s] %s: %d probes; worst |trace-visible| %.2f cm, |sweep-visible| %.2f cm, |visible-field| %.2f cm; missing %d (capsule %d, 34 cm sphere %d); overlap wrong %d"),
				GLTerrainCollision::ModeName(GLTerrainCollision::GetMode()), Step, W.Probes, W.Trace, W.Sweep, W.Visible, W.Missing, W.CapsuleMissing, W.BigSphereMissing, W.OverlapWrong));
			T.TestEqual(*FString::Printf(TEXT("%s: a character capsule always lands"), Step), W.CapsuleMissing, 0);
			for (const FString& Where : W.MissingWhere)
			{
				T.AddInfo(FString::Printf(TEXT("%s: no ground at %s"), Step, *Where));
			}
			T.TestEqual(*FString::Printf(TEXT("%s: physics finds ground under every probe"), Step), W.Missing, 0);
			T.TestTrue(*FString::Printf(TEXT("%s: the trace is the visible surface (%.2f cm)"), Step, W.Trace), W.Trace <= 1.0);
			T.TestTrue(*FString::Printf(TEXT("%s: the sweep touches the visible surface (%.2f cm)"), Step, W.Sweep), W.Sweep <= 1.0);
			T.TestTrue(*FString::Printf(TEXT("%s: the visible surface is the authoritative heights (%.2f cm)"), Step, W.Visible), W.Visible <= 0.5);
			T.TestEqual(*FString::Printf(TEXT("%s: overlaps touch the surface exactly"), Step), W.OverlapWrong, 0);
		}

		void Edit(EGLTerrainOp Op, const FVector2D& At, double Radius, double Amount)
		{
			FGLTerrainEdit E;
			E.Op = Op;
			E.Centre = At;
			E.RadiusCm = Radius;
			E.AmountCm = Amount;
			Terrain->ApplyEdit(E);
		}
	};
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCollisionAgreesUnderDeformation, "Gridlands.Game.TerrainCollision.VisibleAndCollisionSurfacesAgreeUnderDeformation", GLTestUtils::Flags)
bool FGLCollisionAgreesUnderDeformation::RunTest(const FString& Parameters)
{
	GLTerrainCollisionTests::FCollisionScene S(TEXT("GLCollisionAgreeWorld"));
	const FVector2D C = S.Corner();
	S.Expect(*this, TEXT("untouched"), S.Check(C, 900.0));

	S.Edit(EGLTerrainOp::Dig, C + FVector2D(330.0, 270.0), 150.0, 20.0);
	S.Expect(*this, TEXT("small dig"), S.Check(C + FVector2D(330.0, 270.0), 300.0));

	S.Edit(EGLTerrainOp::Dig, C + FVector2D(-510.0, 430.0), 120.0, 400.0);
	S.Expect(*this, TEXT("steep dig (4 m in 1.2 m)"), S.Check(C + FVector2D(-510.0, 430.0), 300.0));

	S.Edit(EGLTerrainOp::Raise, C + FVector2D(620.0, -550.0), 100.0, 400.0);
	S.Expect(*this, TEXT("steep raise"), S.Check(C + FVector2D(620.0, -550.0), 300.0));

	for (int32 I = 0; I < 10; ++I)
	{
		S.Edit(EGLTerrainOp::Dig, C + FVector2D(-300.0, -300.0), 200.0, 30.0);
		const GLTerrainCollisionTests::FCollisionWorst Now = S.Check(C + FVector2D(-300.0, -300.0), 120.0); // right after each edit: never stale
		for (const FString& Where : Now.MissingWhere)
		{
			AddInfo(FString::Printf(TEXT("repeated dig %d: no ground at %s"), I, *Where));
		}
		TestTrue(*FString::Printf(TEXT("repeated dig %d: collision follows at once (%.2f cm, %d missing)"), I, Now.Trace, Now.Missing), Now.Trace <= 1.0 && Now.Missing == 0);
	}
	S.Expect(*this, TEXT("10 repeated digs"), S.Check(C + FVector2D(-300.0, -300.0), 400.0));

	for (int32 I = 0; I < 8; ++I)
	{
		S.Edit(EGLTerrainOp::Dig, C + FVector2D(-800.0 + I * 100.0, 800.0), 60.0, 80.0);
	}
	S.Expect(*this, TEXT("neighbouring vertices edited"), S.Check(C + FVector2D(-450.0, 800.0), 450.0));

	S.Edit(EGLTerrainOp::Raise, C + FVector2D(30.0, 2000.0), 180.0, 150.0);
	S.Expect(*this, TEXT("edit on an X seam, both sides"), S.Check(C + FVector2D(0.0, 2000.0), 400.0));

	S.Edit(EGLTerrainOp::Dig, C, 250.0, 200.0);
	S.Expect(*this, TEXT("edit on a chunk corner (four chunks)"), S.Check(C, 500.0));

	FRandomStream Rng(0x5eed);
	for (int32 I = 0; I < 30; ++I)
	{
		const FVector2D At = C + FVector2D(Rng.FRandRange(-900.0, 900.0), Rng.FRandRange(-900.0, 900.0));
		S.Edit(Rng.RandBool() ? EGLTerrainOp::Dig : EGLTerrainOp::Raise, At, Rng.FRandRange(60.0, 300.0), Rng.FRandRange(10.0, 150.0));
		const double Vis = S.VisibleZ(At), Tr = S.TraceZ(At);
		TestTrue(*FString::Printf(TEXT("rapid edit %d: no stale collision at its centre (%.1f vs %.1f)"), I, Tr, Vis), FMath::Abs(Tr - Vis) <= 1.0);
	}
	S.Expect(*this, TEXT("30 rapid edits in one frame"), S.Check(C, 1000.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCollisionSurvivesRestore, "Gridlands.Game.TerrainCollision.EditedCollisionSurvivesSaveRestoreAndStreaming", GLTestUtils::Flags)
bool FGLCollisionSurvivesRestore::RunTest(const FString& Parameters)
{
	GLTerrainCollisionTests::FCollisionScene S(TEXT("GLCollisionRestoreWorld"));
	const FVector2D C = S.Corner();
	S.Edit(EGLTerrainOp::Dig, C + FVector2D(-200.0, 150.0), 220.0, 300.0);
	S.Edit(EGLTerrainOp::Raise, C + FVector2D(250.0, -120.0), 200.0, 250.0);
	S.Edit(EGLTerrainOp::Dig, C, 150.0, 120.0);
	TArray<FVector2D> Marks;
	TArray<double> Before;
	for (int32 I = 0; I < 40; ++I)
	{
		Marks.Add(C + FVector2D(-450.0 + (I % 8) * 123.0, -450.0 + (I / 8) * 211.0));
		Before.Add(S.TraceZ(Marks.Last()));
	}
	TArray<int32> Indices, Delta;
	TestTrue(TEXT("the edits are captured"), S.Terrain->CaptureCellDelta(GLTerrainCollisionTests::TCollisionCell, Indices, Delta) && Indices.Num() > 0);

	// Restore by a flushed reload (what a save/restart does).
	S.Terrain->RemoveCell(GLTerrainCollisionTests::TCollisionCell);
	S.Terrain->FlushAll();
	TestTrue(TEXT("nothing collides where the unloaded cell was"), FMath::IsNaN(S.TraceZ(C)));
	S.Terrain->BeginCellGround(GLTerrainCollisionTests::TCollisionCell, Indices, Delta);
	S.Terrain->FlushAll();
	int32 Moved = 0;
	for (int32 I = 0; I < Marks.Num(); ++I)
	{
		Moved += FMath::Abs(S.TraceZ(Marks[I]) - Before[I]) > 1.0 ? 1 : 0;
	}
	TestEqual(TEXT("restored: collision is where it was before unloading"), Moved, 0);
	S.Expect(*this, TEXT("restored (flushed)"), S.Check(C, 600.0));

	// Restore by streaming (worker-built geometry, applied over frames), onto pooled chunks.
	S.Terrain->RemoveCell(GLTerrainCollisionTests::TCollisionCell);
	S.Terrain->FlushAll();
	S.Terrain->BeginCellGround(GLTerrainCollisionTests::TCollisionCell, Indices, Delta);
	for (int32 F = 0; F < 6000 && (!S.Terrain->HasCell(GLTerrainCollisionTests::TCollisionCell) || !S.Terrain->IsCellComplete(GLTerrainCollisionTests::TCollisionCell)); ++F)
	{
		S.Terrain->Pump(C, 0.004, 8);
		FPlatformProcess::Sleep(0.0005f);
	}
	TestTrue(TEXT("streamed back"), S.Terrain->IsCellComplete(GLTerrainCollisionTests::TCollisionCell));
	Moved = 0;
	for (int32 I = 0; I < Marks.Num(); ++I)
	{
		Moved += FMath::Abs(S.TraceZ(Marks[I]) - Before[I]) > 1.0 ? 1 : 0;
	}
	TestEqual(TEXT("streamed: collision is where it was before unloading"), Moved, 0);
	S.Expect(*this, TEXT("restored (streamed, reused chunks)"), S.Check(C, 600.0));
	return true;
}

#endif
