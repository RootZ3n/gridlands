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
#include "Building/GLBuildingSubsystem.h"
#include "Building/GLStructureRules.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLTerrainCollisionTests
{
	const FName TCollisionCell(TEXT("cell.home.origin"));

	struct FCollisionWorst
	{
		double Trace = 0.0;       // |collision (trace) - visible| cm
		double Sweep = 0.0;       // |sweep contact - visible surface at the contact| cm
		double Visible = 0.0;     // |visible - field (same triangles)| cm
		double RenderGameplay = 0.0;    // |HeightAt (what gameplay stands on) - visible| cm
		double CollisionGameplay = 0.0; // |HeightAt - collision (trace)| cm
		int32 QuadProbes = 0;           // per-quad probes (vertex, centre, both sides of and 1 mm from the diagonal)
		FString WorstSweepAt;
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

		FName Cell = TCollisionCell;

		explicit FCollisionScene(const TCHAR* Name, FName InCell = TCollisionCell) : Test(Name), Cell(InCell)
		{
			Terrain = Test.World->GetSubsystem<UGLTerrainSubsystem>();
			Terrain->BeginCellGround(Cell, {}, {});
			Terrain->FlushAll();
		}

		const FGLHeightfield& Field() const { return *Terrain->FieldOf(Cell); }

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

		/** The canonical terrain surface inside a quad (A=(x,y), B=(x+1,y), C=(x,y+1), D=(x+1,y+1)). */
		static double TriangleZ(double ZA, double ZB, double ZC, double ZD, double FX, double FY)
		{
			// An independent statement of the canonical split (A-D; triangles (A,C,D) and (A,D,B)), deliberately not
			// GLTerrainSurface: a defect there must not also blind the oracle. Chaos's own diagonal anchors both.
			return FY >= FX ? ZA + FX * (ZD - ZC) + FY * (ZC - ZA) : ZA + FX * (ZB - ZA) + FY * (ZD - ZB);
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
			// The triangles the mesh actually holds for this quad (two per quad, in build order), not an assumed
			// split: whatever the render mesh draws is what is compared.
			const double S = Field().GetSpacing();
			const FVector2D Local = At - FVector2D(Chunk->GetActorLocation());
			const int32 X = FMath::Clamp(FMath::FloorToInt(Local.X / S), 0, V - 2), Y = FMath::Clamp(FMath::FloorToInt(Local.Y / S), 0, V - 2);
			const int32 First = 2 * (Y * (V - 1) + X);
			double Best = NAN, BestOutside = 1.0e9;
			for (int32 T = First; T < First + 2; ++T)
			{
				const UE::Geometry::FIndex3i Tri = Mesh->GetTriangle(T);
				const FVector3d A = Mesh->GetVertex(Tri.A), B = Mesh->GetVertex(Tri.B), C = Mesh->GetVertex(Tri.C);
				const double Det = (B.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (B.Y - A.Y);
				if (FMath::Abs(Det) < 1e-9)
				{
					continue;
				}
				const double U = ((Local.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (Local.Y - A.Y)) / Det;
				const double W = ((B.X - A.X) * (Local.Y - A.Y) - (Local.X - A.X) * (B.Y - A.Y)) / Det;
				const double Outside = FMath::Max3(-U, -W, U + W - 1.0); // <= 0 inside the triangle
				if (Outside < BestOutside)
				{
					BestOutside = Outside;
					Best = A.Z + U * (B.Z - A.Z) + W * (C.Z - A.Z);
				}
			}
			return Best;
		}

		/** The authoritative heights, triangulated as drawn. */
		double FieldTriangleZ(const FVector2D& At) const
		{
			const FGLHeightfield& F = Field();
			const FVector2D L = (At - F.VertexLocation(0, 0)) / F.GetSpacing();
			const int32 X = FMath::Clamp(FMath::FloorToInt(L.X), 0, F.GetVertsX() - 2), Y = FMath::Clamp(FMath::FloorToInt(L.Y), 0, F.GetVertsY() - 2);
			return TriangleZ(F.VertexHeight(X, Y), F.VertexHeight(X + 1, Y), F.VertexHeight(X, Y + 1), F.VertexHeight(X + 1, Y + 1), L.X - X, L.Y - Y);
		}

		/** The colliding terrain alone (traces through anything else standing on it, e.g. a structure). */
		double TerrainZ(const FVector2D& At) const
		{
			TArray<FHitResult> Hits;
			Test.World->LineTraceMultiByObjectType(Hits, FVector(At, 60000.0), FVector(At, -60000.0), FCollisionObjectQueryParams(ECC_WorldStatic));
			for (const FHitResult& Hit : Hits)
			{
				if (Hit.GetActor() && Hit.GetActor()->IsA<AGLTerrainChunk>())
				{
					return Hit.ImpactPoint.Z;
				}
			}
			return NAN;
		}

		double TraceZ(const FVector2D& At) const
		{
			FHitResult Hit;
			return Test.World->LineTraceSingleByChannel(Hit, FVector(At, 60000.0), FVector(At, -60000.0), ECC_Visibility) ? Hit.ImpactPoint.Z : NAN;
		}

		/**
		 * A 10 cm sphere swept down (the sweep path movement uses; it skips heightfield holes). On a slope it
		 * touches beside the probe, so the answer is how far its contact is from the visible surface THERE.
		 * Swept 3 m either side of the ground, as gameplay sweeps are short: Chaos sweeps a heightfield in single
		 * precision over the whole query, so a 1.2 km sweep stops ~1 cm early on its own (measured).
		 */
		double SweepError(const FVector2D& At) const
		{
			FHitResult Hit;
			const double Z = TraceZ(At);
			if (FMath::IsNaN(Z) || !Test.World->SweepSingleByChannel(Hit, FVector(At, Z + 300.0), FVector(At, Z - 300.0), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(10.0f)))
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
			// Every quad of the square, at the points where a wrong interpolation shows: its vertex, its centre,
			// both sides of the diagonal, and 1 mm either side of the diagonal (A-D: FX = FY).
			const FGLHeightfield& F = Field();
			const FVector2D Origin = F.VertexLocation(0, 0);
			const double S = F.GetSpacing();
			const int32 X0 = FMath::CeilToInt((Centre.X - HalfCm - Origin.X) / S), X1 = FMath::FloorToInt((Centre.X + HalfCm - Origin.X) / S) - 1;
			const int32 Y0 = FMath::CeilToInt((Centre.Y - HalfCm - Origin.Y) / S), Y1 = FMath::FloorToInt((Centre.Y + HalfCm - Origin.Y) / S) - 1;
			const FVector2D InQuad[] = { {0.0, 0.0}, {0.5, 0.5}, {0.3, 0.7}, {0.7, 0.3}, {0.5 - 0.001, 0.5 + 0.001}, {0.5 + 0.001, 0.5 - 0.001}, {0.15, 0.85}, {0.85, 0.15} };
			for (int32 VY = Y0; VY <= Y1; ++VY)
			{
				for (int32 VX = X0; VX <= X1; ++VX)
				{
					for (const FVector2D& Q : InQuad)
					{
						Probes.Add(Origin + FVector2D((VX + Q.X) * S, (VY + Q.Y) * S));
					}
				}
			}
			W.QuadProbes = (X1 - X0 + 1) * (Y1 - Y0 + 1) * UE_ARRAY_COUNT(InQuad);
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
				const double Game = Terrain->HeightAt(P);
				W.RenderGameplay = FMath::Max(W.RenderGameplay, FMath::Abs(Game - Vis));
				W.CollisionGameplay = FMath::Max(W.CollisionGameplay, FMath::Abs(Game - Tr));
				W.Trace = FMath::Max(W.Trace, FMath::Abs(Tr - Vis));
				if (Sw > W.Sweep)
				{
					W.Sweep = Sw;
					const FVector2D L = (P - Field().VertexLocation(0, 0)) / Field().GetSpacing();
					FHitResult Hit;
					Test.World->SweepSingleByChannel(Hit, FVector(P, Tr + 300.0), FVector(P, Tr - 300.0), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(10.0f));
					const FVector2D CL = (FVector2D(Hit.ImpactPoint) - Field().VertexLocation(0, 0)) / Field().GetSpacing();
					W.WorstSweepAt = FString::Printf(TEXT("probe vertex-space (%.3f, %.3f); contact (%.3f, %.3f) z %.3f; visible there %.3f, trace there %.3f; sphere centre z %.3f, normal %s"),
						L.X, L.Y, CL.X, CL.Y, Hit.ImpactPoint.Z, VisibleZ(FVector2D(Hit.ImpactPoint)), TraceZ(FVector2D(Hit.ImpactPoint)), Hit.Location.Z, *Hit.ImpactNormal.ToString());
				}
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
			T.AddInfo(FString::Printf(TEXT("[%s] %s: %d probes; worst |trace-visible| %.2f cm, |sweep-visible| %.2f cm, |visible-field| %.2f cm; missing %d (capsule %d, 34 cm sphere %d); overlap wrong %d; GAMEPLAY (HeightAt) vs visible %.3f cm, vs collision %.3f cm (%d per-quad probes)"),
				GLTerrainCollision::ModeName(GLTerrainCollision::GetMode()), Step, W.Probes, W.Trace, W.Sweep, W.Visible, W.Missing, W.CapsuleMissing, W.BigSphereMissing, W.OverlapWrong, W.RenderGameplay, W.CollisionGameplay, W.QuadProbes));
			T.TestTrue(*FString::Printf(TEXT("%s: gameplay ground is the visible ground (%.3f cm)"), Step, W.RenderGameplay), W.RenderGameplay <= 0.05);
			T.TestTrue(*FString::Printf(TEXT("%s: gameplay ground is the colliding ground (%.3f cm)"), Step, W.CollisionGameplay), W.CollisionGameplay <= 0.1);
			T.TestEqual(*FString::Printf(TEXT("%s: a character capsule always lands"), Step), W.CapsuleMissing, 0);
			for (const FString& Where : W.MissingWhere)
			{
				T.AddInfo(FString::Printf(TEXT("%s: no ground at %s"), Step, *Where));
			}
			T.TestEqual(*FString::Printf(TEXT("%s: physics finds ground under every probe"), Step), W.Missing, 0);
			T.TestTrue(*FString::Printf(TEXT("%s: the trace is the visible surface (%.2f cm)"), Step, W.Trace), W.Trace <= 1.0);
			T.TestTrue(*FString::Printf(TEXT("%s: the sweep touches the visible surface (%.3f cm, worst at %s)"), Step, W.Sweep, *W.WorstSweepAt), W.Sweep <= 1.0);
			T.TestTrue(*FString::Printf(TEXT("%s: the visible surface is the authoritative heights (%.2f cm)"), Step, W.Visible), W.Visible <= 0.5);
			T.TestEqual(*FString::Printf(TEXT("%s: overlaps touch the surface exactly"), Step), W.OverlapWrong, 0);
		}

		void Edit(EGLTerrainOp Op, const FVector2D& At, double Radius, double Amount, double Target = 0.0)
		{
			FGLTerrainEdit E;
			E.Op = Op;
			E.Centre = At;
			E.RadiusCm = Radius;
			E.AmountCm = Amount;
			E.TargetHeightCm = Target;
			Terrain->ApplyEdit(E);
		}

		/** The interpolation HeightAt used before 2026-09-27 (bilinear), kept only to show where it differed. */
		double OldBilinearZ(const FVector2D& At) const
		{
			const FGLHeightfield& F = Field();
			const FVector2D L = (At - F.VertexLocation(0, 0)) / F.GetSpacing();
			const int32 X = FMath::Clamp(FMath::FloorToInt(L.X), 0, F.GetVertsX() - 2), Y = FMath::Clamp(FMath::FloorToInt(L.Y), 0, F.GetVertsY() - 2);
			const double TX = L.X - X, TY = L.Y - Y;
			return FMath::Lerp(FMath::Lerp<double>(F.VertexHeight(X, Y), F.VertexHeight(X + 1, Y), TX), FMath::Lerp<double>(F.VertexHeight(X, Y + 1), F.VertexHeight(X + 1, Y + 1), TX), TY);
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

	// Flatten a steep patch toward a level (dig and raise at once), and the 4 m dig limit reached and held.
	S.Edit(EGLTerrainOp::Raise, C + FVector2D(-600.0, -800.0), 250.0, 200.0);
	S.Edit(EGLTerrainOp::Flatten, C + FVector2D(-520.0, -760.0), 180.0, 0.0, 60.0);
	S.Expect(*this, TEXT("flatten"), S.Check(C + FVector2D(-560.0, -780.0), 350.0));
	for (int32 I = 0; I < 4; ++I)
	{
		S.Edit(EGLTerrainOp::Dig, C + FVector2D(700.0, 700.0), 200.0, 300.0); // asks for 12 m: held at the 4 m limit
	}
	TestTrue(TEXT("the dig limit held (4 m)"), S.Field().VertexHeight(512 + 7, 512 + 7) >= -400.5);
	S.Expect(*this, TEXT("4 m dig limit"), S.Check(C + FVector2D(700.0, 700.0), 350.0));
	S.Edit(EGLTerrainOp::Raise, C + FVector2D(-6400.0 + 50.0, 3200.0), 320.0, 260.0); // across the next X seam west
	S.Edit(EGLTerrainOp::Dig, C + FVector2D(3200.0, 6400.0 - 40.0), 320.0, 260.0);     // across the next Y seam north
	S.Expect(*this, TEXT("edit crossing the west X seam"), S.Check(C + FVector2D(-6400.0, 3200.0), 450.0));
	S.Expect(*this, TEXT("edit crossing the north Y seam"), S.Check(C + FVector2D(3200.0, 6400.0), 450.0));

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

	// Restore in place, as a save loaded into the running world does (RestoreCellDelta): back to the base, then the edits.
	TestTrue(TEXT("restored in place to the base"), S.Terrain->RestoreCellDelta(GLTerrainCollisionTests::TCollisionCell, {}, {}));
	S.Expect(*this, TEXT("restored in place to the base"), S.Check(C, 600.0));
	TestTrue(TEXT("restored in place with the edits"), S.Terrain->RestoreCellDelta(GLTerrainCollisionTests::TCollisionCell, Indices, Delta));
	Moved = 0;
	for (int32 I = 0; I < Marks.Num(); ++I)
	{
		Moved += FMath::Abs(S.TraceZ(Marks[I]) - Before[I]) > 1.0 ? 1 : 0;
	}
	TestEqual(TEXT("restored in place: collision is where it was"), Moved, 0);
	S.Expect(*this, TEXT("restored in place with the edits"), S.Check(C, 600.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNaturalTerrainIsOneSurface, "Gridlands.Game.TerrainCollision.NaturalTerrainIsOneSurface", GLTestUtils::Flags)
bool FGLNaturalTerrainIsOneSurface::RunTest(const FString& Parameters)
{
	// The lots' authored rolling relief (6 m): ordinary ground and the steepest natural slope in the playable middle.
	GLTerrainCollisionTests::FCollisionScene S(TEXT("GLNaturalSurfaceWorld"), TEXT("cell.outer.diner_lots"));
	const FGLHeightfield& F = S.Field();
	double Best = -1.0;
	FIntPoint Steep(512, 512);
	for (int32 Y = 120; Y < 900; Y += 2)
	{
		for (int32 X = 120; X < 900; X += 2)
		{
			const double GX = (F.VertexHeight(X + 1, Y) - F.VertexHeight(X - 1, Y)) / (2.0 * F.GetSpacing());
			const double GY = (F.VertexHeight(X, Y + 1) - F.VertexHeight(X, Y - 1)) / (2.0 * F.GetSpacing());
			if (GX * GX + GY * GY > Best)
			{
				Best = GX * GX + GY * GY;
				Steep = FIntPoint(X, Y);
			}
		}
	}
	AddInfo(FString::Printf(TEXT("steepest natural slope %.1f deg at vertex (%d, %d)"), FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Best))), Steep.X, Steep.Y));
	S.Expect(*this, TEXT("rolling natural terrain"), S.Check(F.VertexLocation(300, 700), 900.0));
	S.Expect(*this, TEXT("steepest natural slope"), S.Check(F.VertexLocation(Steep.X, Steep.Y), 900.0));

	// How much the change moved the ground authored content stands on (reported, not asserted): the whole
	// natural cell, and within 5 m of every authored structure's origin (placements are cell-local).
	auto Report = [this](GLTerrainCollisionTests::FCollisionScene& Scene, const FVector2D& CellCentre, const TCHAR* Prefix)
	{
		const FGLHeightfield& G = Scene.Field();
		double CellMax = 0.0;
		for (double Y = 0.0; Y < (G.GetVertsY() - 1) * G.GetSpacing(); Y += 37.0)
		{
			for (double X = 0.0; X < (G.GetVertsX() - 1) * G.GetSpacing(); X += 37.0)
			{
				const FVector2D P = G.GetOrigin() + FVector2D(X, Y);
				CellMax = FMath::Max(CellMax, FMath::Abs(Scene.OldBilinearZ(P) - Scene.Terrain->HeightAt(P)));
			}
		}
		AddInfo(FString::Printf(TEXT("%s: the whole natural cell moved by at most %.3f cm (old bilinear vs the one surface)"), Prefix, CellMax));
		GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
		{
			const FGLPlacementDef* Placement = GLContent::Get().Find<FGLPlacementDef>(Entry.Id);
			if (!Placement || Placement->Kind != TEXT("structure") || !Entry.Id.ToString().StartsWith(Prefix) || Placement->Transform.Location.Num() < 2)
			{
				return;
			}
			const FVector2D At = CellCentre + FVector2D(Placement->Transform.Location[0], Placement->Transform.Location[1]);
			double Max = 0.0;
			for (double Y = -500.0; Y <= 500.0; Y += 25.0)
			{
				for (double X = -500.0; X <= 500.0; X += 25.0)
				{
					Max = FMath::Max(Max, FMath::Abs(Scene.OldBilinearZ(At + FVector2D(X, Y)) - Scene.Terrain->HeightAt(At + FVector2D(X, Y))));
				}
			}
			const double AtOrigin = Scene.Terrain->HeightAt(At) - Scene.OldBilinearZ(At);
			AddInfo(FString::Printf(TEXT("authored %s: its base (the ground at its origin) moved %+.3f cm; ground within 5 m moved by at most %.3f cm"), *Entry.Id.ToString(), AtOrigin, Max));
		});
	};
	Report(S, FVector2D(102400.0, 0.0), TEXT("placement.diner_lots."));
	GLTerrainCollisionTests::FCollisionScene Home(TEXT("GLNaturalSurfaceHome"));
	Report(Home, FVector2D::ZeroVector, TEXT("placement.origin."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructuresStandOnTheSurface, "Gridlands.Game.TerrainCollision.StructuresAndBuildingStandOnTheVisibleGround", GLTestUtils::Flags)
bool FGLStructuresStandOnTheSurface::RunTest(const FString& Parameters)
{
	// Steep edited ground where the old bilinear HeightAt left the drawn, colliding triangles by tens of cm:
	// building placement, the placement rules (resting / buried) and authored structures must use the
	// visible, colliding ground there, deterministically, before and after save/restore.
	const FName SurfaceFloor(TEXT("buildpiece.modern.timber_foundation"));
	const FName SurfaceCarport(TEXT("structure.modern.carport"));
	auto Stage = [](GLTerrainCollisionTests::FCollisionScene& S)
	{
		const FVector2D C = S.Corner() + FVector2D(-2000.0, 1500.0);
		for (int32 I = 0; I < 8; ++I)
		{
			S.Edit(I % 2 ? EGLTerrainOp::Raise : EGLTerrainOp::Dig, C + FVector2D(-350.0 + I * 100.0, (I % 3) * 70.0), 70.0, 150.0);
		}
		S.Edit(EGLTerrainOp::Dig, C + FVector2D(120.0, -260.0), 110.0, 380.0);
		return C;
	};
	GLTerrainCollisionTests::FCollisionScene S(TEXT("GLSurfaceBuildWorld"));
	const FVector2D C = Stage(S);
	UGLBuildingSubsystem* Building = S.Test.World->GetSubsystem<UGLBuildingSubsystem>();
	UGLStructureSubsystem* Structures = S.Test.World->GetSubsystem<UGLStructureSubsystem>();
	const FGLContentRegistry& Content = GLContent::Get();
	// A builder who knows timber framing and carries planks (the subsystem's check needs both before the ground).
	AActor* SurfaceBuilder = S.Test.World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(SurfaceBuilder);
	SurfaceBuilder->SetRootComponent(Root);
	Root->RegisterComponent();
	UGLInventoryComponent* Inventory = NewObject<UGLInventoryComponent>(SurfaceBuilder);
	Inventory->RegisterComponent();
	Inventory->AddItem(TEXT("item.material.timber_plank"), 500);
	S.Test.World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(TEXT("knowledge.style.modern_timber_frame"));

	// Where the old interpolation was furthest from the surface (10 cm scan of the edited area).
	TArray<TPair<double, FVector2D>> Worst;
	for (double Y = -500.0; Y <= 500.0; Y += 10.0)
	{
		for (double X = -600.0; X <= 600.0; X += 10.0)
		{
			const FVector2D P = C + FVector2D(X, Y);
			Worst.Add({ FMath::Abs(S.OldBilinearZ(P) - S.Terrain->HeightAt(P)), P });
		}
	}
	Worst.Sort([](const TPair<double, FVector2D>& A, const TPair<double, FVector2D>& B) { return A.Key > B.Key; });
	TestTrue(*FString::Printf(TEXT("the case matters: the old interpolation was %.1f cm off the surface here"), Worst[0].Key), Worst[0].Key >= 30.0);

	// 1. Placement: a grounded piece snapped with nothing to snap to sits at the ground under the aim.
	double SnapVis = 0.0, SnapTrace = 0.0, SnapOld = 0.0;
	for (int32 I = 0; I < 24; ++I)
	{
		const FVector2D P = Worst[I * 7].Value;
		FGLPlacedPiece Candidate;
		if (!TestTrue(TEXT("a floor snaps to the ground"), Building->Snap(SurfaceFloor, FVector(P, 0.0), 0, Candidate)))
		{
			return false;
		}
		SnapVis = FMath::Max(SnapVis, FMath::Abs(Candidate.Location.Z - S.VisibleZ(P)));
		SnapTrace = FMath::Max(SnapTrace, FMath::Abs(Candidate.Location.Z - S.TerrainZ(P)));
		SnapOld = FMath::Max(SnapOld, FMath::Abs(S.OldBilinearZ(P) - S.TerrainZ(P)));
	}
	AddInfo(FString::Printf(TEXT("placement: a snapped floor vs visible %.3f cm, vs collision %.3f cm (the old interpolation: up to %.1f cm)"), SnapVis, SnapTrace, SnapOld));
	TestTrue(TEXT("a snapped floor sits on the visible ground"), SnapVis <= 0.05);
	TestTrue(TEXT("a snapped floor sits on the colliding ground"), SnapTrace <= 0.1);

	// 2. The placement rules (resting on the ground, buried) through the building subsystem, against the same
	//    rules on the visible, colliding ground, for floors set level at many heights over the steep area.
	auto Decide = [&](TFunctionRef<double(const FVector2D&)> Ground)
	{
		TArray<int32> Out;
		for (double Y = -400.0; Y <= 400.0; Y += 80.0)
		{
			for (double X = -500.0; X <= 500.0; X += 80.0)
			{
				const FVector2D P = C + FVector2D(X, Y);
				for (const double Up : { -60.0, -35.0, -20.0, 0.0, 20.0, 35.0 })
				{
					const FGLPlacedPiece Candidate{ 0, SurfaceFloor, FVector(P, S.TerrainZ(P) + Up), 0 };
					Out.Add(static_cast<int32>(GLStructureRules::CheckPlacement(Content, Building->GetPieces(), Candidate, Ground).Refusal) * 2
						+ (GLStructureRules::RestsOnGround(*Content.Find<FGLBuildPieceDef>(SurfaceFloor), Candidate, Ground) ? 1 : 0));
				}
			}
		}
		return Out;
	};
	const TArray<int32> Visible = Decide([&S](const FVector2D& At) { return S.TerrainZ(At); });
	const TArray<int32> Gameplay = Decide([&S](const FVector2D& At) { return S.Terrain->HeightAt(At); });
	const TArray<int32> Old = Decide([&S](const FVector2D& At) { return S.OldBilinearZ(At); });
	int32 Differ = 0, OldDiffer = 0;
	for (int32 I = 0; I < Visible.Num(); ++I)
	{
		Differ += Gameplay[I] != Visible[I] ? 1 : 0;
		OldDiffer += Old[I] != Visible[I] ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("placement rules on %d candidates: %d decisions differ from the visible ground (the old interpolation: %d)"), Visible.Num(), Differ, OldDiffer));
	TestEqual(TEXT("every resting/buried decision is the one the visible, colliding ground gives"), Differ, 0);
	TestTrue(TEXT("the old interpolation got some of them wrong (the proof has teeth)"), OldDiffer > 0);
	// And through the building subsystem's own check (its own ground query), for the same candidates at rest height.
	int32 CheckDiffer = 0, Checked = 0;
	for (double Y = -400.0; Y <= 400.0; Y += 80.0)
	{
		for (double X = -500.0; X <= 500.0; X += 80.0)
		{
			const FVector2D P = C + FVector2D(X, Y);
			for (const double Up : { -60.0, -35.0, 0.0, 35.0 })
			{
				const FGLPlacedPiece Candidate{ 0, SurfaceFloor, FVector(P, S.TerrainZ(P) + Up), 0 };
				const bool bBuriedBySubsystem = Building->Check(SurfaceBuilder, Candidate).Refusal == EGLBuildRefusal::Buried;
				const bool bBuriedOnVisible = GLStructureRules::CheckPlacement(Content, Building->GetPieces(), Candidate, [&S](const FVector2D& At) { return S.TerrainZ(At); }).Refusal == EGLBuildRefusal::Buried;
				CheckDiffer += bBuriedBySubsystem != bBuriedOnVisible ? 1 : 0;
				++Checked;
			}
		}
	}
	TestEqual(*FString::Printf(TEXT("the building subsystem's own check calls a floor buried exactly where the visible ground does (%d candidates)"), Checked), CheckDiffer, 0);

	// 3. An authored structure spawned at the worst point stands on the visible, colliding ground.
	const FVector2D P0 = Worst[0].Value;
	TestTrue(TEXT("the carport spawns"), Structures->SpawnStructure(TEXT("placement.test.surface_carport"), SurfaceCarport, S.Cell, FVector(P0, 0.0), 0));
	AGLStructurePart* Post = Structures->FindPart(TEXT("placement.test.surface_carport"), TEXT("post_south"));
	const double Base = Post ? Post->GetActorLocation().Z : 1.0e9;
	AddInfo(FString::Printf(TEXT("authored structure base at the worst point: vs visible %.3f cm, vs collision %.3f cm (the old interpolation: %.1f cm off)"),
		FMath::Abs(Base - S.VisibleZ(P0)), FMath::Abs(Base - S.TerrainZ(P0)), FMath::Abs(S.OldBilinearZ(P0) - S.TerrainZ(P0))));
	TestTrue(TEXT("the structure stands on the visible ground"), FMath::Abs(Base - S.VisibleZ(P0)) <= 0.05);
	TestTrue(TEXT("the structure stands on the colliding ground"), FMath::Abs(Base - S.TerrainZ(P0)) <= 0.1);

	// 4. Deterministic: the same edits in a fresh world give the same structure and the same decisions.
	{
		GLTerrainCollisionTests::FCollisionScene R(TEXT("GLSurfaceBuildWorld2"));
		Stage(R);
		UGLStructureSubsystem* Structures2 = R.Test.World->GetSubsystem<UGLStructureSubsystem>();
		Structures2->SpawnStructure(TEXT("placement.test.surface_carport"), SurfaceCarport, R.Cell, FVector(P0, 0.0), 0);
		AGLStructurePart* Post2 = Structures2->FindPart(TEXT("placement.test.surface_carport"), TEXT("post_south"));
		TestTrue(TEXT("deterministic: the same structure base in a fresh world"), Post2 && FMath::Abs(Post2->GetActorLocation().Z - Base) < 0.001);
		const TArray<int32> Again = Decide([&R](const FVector2D& At) { return R.Terrain->HeightAt(At); });
		TestTrue(TEXT("deterministic: the same placement decisions in a fresh world"), Again == Gameplay);
	}

	// 5. Save/restore: the restored ground gives the same placement and the same decisions.
	TArray<int32> Indices, Delta;
	TestTrue(TEXT("the edits are captured"), S.Terrain->CaptureCellDelta(S.Cell, Indices, Delta) && Indices.Num() > 0);
	FGLPlacedPiece Before;
	Building->Snap(SurfaceFloor, FVector(Worst[0].Value, 0.0), 0, Before);
	S.Terrain->RemoveCell(S.Cell);
	S.Terrain->FlushAll();
	S.Terrain->BeginCellGround(S.Cell, Indices, Delta);
	S.Terrain->FlushAll();
	FGLPlacedPiece After;
	Building->Snap(SurfaceFloor, FVector(Worst[0].Value, 0.0), 0, After);
	TestTrue(TEXT("restored: a floor snaps to the same height"), FMath::Abs(After.Location.Z - Before.Location.Z) < 0.001);
	TestTrue(TEXT("restored: the same placement decisions"), Decide([&S](const FVector2D& At) { return S.Terrain->HeightAt(At); }) == Gameplay);
	return true;
}

#endif
