// Terrain chunk reuse (P5 pool, hardened after P7.1) and single-cook chunk collision: a pooled
// chunk is reborn clean for its new cell (heights, place, owner, collision, navigation), stale work
// never lands on it, the pool stays inside its bound, a cancelled cell never hands out a half-reset
// chunk, and a streamed chunk collides as its new cell the moment it is applied.
// Driven through the terrain subsystem directly, so reuse across cells is deterministic.

#include "Components/DynamicMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Terrain/GLHeightfield.h"
#include "Terrain/GLTerrainChunk.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLTerrainPoolTests
{
	const FName TOrigin(TEXT("cell.home.origin"));
	const FName TLots(TEXT("cell.outer.diner_lots"));

	struct FPoolScene
	{
		GLTestUtils::FTestWorld Test;
		UGLTerrainSubsystem* Terrain = nullptr;

		explicit FPoolScene(const TCHAR* Name) : Test(Name)
		{
			Terrain = Test.World->GetSubsystem<UGLTerrainSubsystem>();
		}

		void Load(FName Cell)
		{
			Terrain->BeginCellGround(Cell, {}, {});
			Terrain->FlushAll();
		}

		/** Ground by physics: where a downward trace hits (or -1e9 if nothing collides there). */
		double TraceZ(const FVector2D& At) const
		{
			FHitResult Hit;
			return Test.World->LineTraceSingleByChannel(Hit, FVector(At, 60000.0), FVector(At, -60000.0), ECC_WorldStatic) ? Hit.ImpactPoint.Z : -1.0e9;
		}

		TArray<AGLTerrainChunk*> ChunksOf(FName Cell) const
		{
			TArray<AGLTerrainChunk*> Out;
			for (TActorIterator<AGLTerrainChunk> It(Test.World); It; ++It)
			{
				if (IsValid(*It) && It->State == EGLChunkState::Live && It->OwnerCell == Cell)
				{
					Out.Add(*It);
				}
			}
			return Out;
		}

		/** Every live chunk of Cell stands where its slot is and carries that cell's heights (bounds against the field). */
		int32 CheckRebound(FAutomationTestBase& T, FName Cell) const
		{
			const FGLHeightfield* Field = Terrain->FieldOf(Cell);
			int32 Bad = 0;
			for (AGLTerrainChunk* Chunk : ChunksOf(Cell))
			{
				const FIntPoint First = Chunk->GetFirstVertex();
				const FVector2D Expect = Field->VertexLocation(First.X, First.Y);
				double Lo = 1e9, Hi = -1e9;
				for (int32 Y = First.Y; Y < First.Y + Chunk->GetVertsPerSide(); Y += 8)
				{
					for (int32 X = First.X; X < First.X + Chunk->GetVertsPerSide(); X += 8)
					{
						Lo = FMath::Min(Lo, Field->VertexHeight(X, Y));
						Hi = FMath::Max(Hi, Field->VertexHeight(X, Y));
					}
				}
				const FBox B = Chunk->GetMesh()->Bounds.GetBox();
				const bool bPlace = FVector2D(Chunk->GetActorLocation()).Equals(Expect, 1.0);
				const bool bHeights = B.Min.Z <= Lo + 5.0 && B.Max.Z >= Hi - 5.0 && B.Max.Z <= Hi + 400.0 && B.Min.Z >= Lo - 400.0;
				const bool bShown = !Chunk->IsHidden() && Chunk->GetActorEnableCollision() && Chunk->IsBuilt();
				if (!bPlace || !bHeights || !bShown)
				{
					++Bad;
					if (Bad <= 3)
					{
						T.AddError(FString::Printf(TEXT("%s chunk at %s: place %d, heights %d (bounds z %.0f..%.0f, field %.0f..%.0f), shown %d"),
							*Cell.ToString(), *First.ToString(), bPlace, bHeights, B.Min.Z, B.Max.Z, Lo, Hi, bShown));
					}
				}
			}
			return Bad;
		}

		bool Integrity(FAutomationTestBase& T, const TCHAR* When) const
		{
			TArray<FString> Problems;
			const bool bOk = Terrain->CheckChunkIntegrity(&Problems);
			for (const FString& P : Problems)
			{
				T.AddError(FString::Printf(TEXT("%s: %s"), When, *P));
			}
			return bOk;
		}
	};
}

using GLTerrainPoolTests::FPoolScene;
using GLTerrainPoolTests::TOrigin;
using GLTerrainPoolTests::TLots;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPoolRebound, "Gridlands.Game.TerrainPool.ReusedChunksAreRebornCleanInAnotherCell", GLTestUtils::Flags)
bool FGLPoolRebound::RunTest(const FString& Parameters)
{
	FPoolScene S(TEXT("GLPoolReboundWorld"));
	S.Load(TOrigin);
	// A deep pit in the origin: its chunk carries edited heights and edited collision.
	const FVector2D Pit(1000.0, -1200.0);
	const double Before = S.Terrain->HeightAt(Pit);
	for (int32 I = 0; I < 3; ++I)
	{
		FGLTerrainEdit Dig;
		Dig.Op = EGLTerrainOp::Dig;
		Dig.Centre = Pit;
		Dig.RadiusCm = 300.0;
		Dig.AmountCm = 100.0;
		S.Terrain->ApplyEdit(Dig);
	}
	const double Dug = S.Terrain->HeightAt(Pit);
	TestTrue(TEXT("the origin has a pit"), Dug < Before - 100.0);
	TestEqual(TEXT("its collision has the pit"), S.TraceZ(Pit), Dug, 5.0);
	const int32 OriginChunks = S.ChunksOf(TOrigin).Num();

	// The origin goes: its chunks are cleared into the pool.
	S.Terrain->RemoveCell(TOrigin);
	S.Terrain->FlushAll();
	TestEqual(TEXT("every origin chunk is pooled"), S.Terrain->NumPooled(), OriginChunks);
	TestTrue(TEXT("pool integrity"), S.Integrity(*this, TEXT("pooled")));
	TestTrue(TEXT("nothing collides where the origin was"), S.TraceZ(Pit) < -1.0e8);

	// The lots come: built from the origin's pooled chunks.
	const FGLTerrainStreamStats Stats0 = S.Terrain->GetStats();
	S.Load(TLots);
	const FGLTerrainStreamStats& Stats1 = S.Terrain->GetStats();
	TestEqual(TEXT("every lots chunk came from the pool"), Stats1.ChunksReused - Stats0.ChunksReused, OriginChunks);
	TestEqual(TEXT("no chunk actor was spawned"), Stats1.ChunksSpawned - Stats0.ChunksSpawned, 0);
	TestTrue(TEXT("integrity: one owner each, the lots, the right slot"), S.Integrity(*this, TEXT("reused")));
	TestEqual(TEXT("every reused chunk stands in its lots slot with the lots' heights (no previous heights)"), S.CheckRebound(*this, TLots), 0);
	// The same place inside the lots as the pit inside the origin: lots ground, never the old pit.
	const FVector2D Same = Pit + FVector2D(102400.0, 0.0);
	TestEqual(TEXT("lots collision at the pit's relative place is lots ground (no leaked edit)"), S.TraceZ(Same), S.Terrain->HeightAt(Same), 5.0);
	for (int32 I = 0; I < 24; ++I)
	{
		const FVector2D At(102400.0 - 50000.0 + I * 4100.0, -45000.0 + I * 3900.0);
		TestEqual(*FString::Printf(TEXT("lots collision is lots ground at %s"), *At.ToString()), S.TraceZ(At), S.Terrain->HeightAt(At), 5.0);
	}
	TestTrue(TEXT("still nothing where the origin was (no chunk left behind)"), S.TraceZ(Pit) < -1.0e8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPoolStale, "Gridlands.Game.TerrainPool.StaleMeshWorkNeverLandsOnAReassignedChunk", GLTestUtils::Flags)
bool FGLPoolStale::RunTest(const FString& Parameters)
{
	// Mesh jobs of one load of the lots have finished, unapplied, when the lots are reloaded with
	// different ground (a raised patch) onto the same pooled chunk actors. The new ground becomes live in
	// the same Pump that meets the old results, with the same slot versions (both start at 0), so the
	// load generation is the only thing that tells them apart: an old result must never land.
	FPoolScene S(TEXT("GLPoolStaleWorld"));
	S.Load(TOrigin);
	S.Terrain->RemoveCell(TOrigin);
	S.Terrain->FlushAll(); // a full pool
	const FVector2D Near(120000.0, 0.0);
	S.Terrain->BeginCellGround(TLots, {}, {});
	for (int32 F = 0; F < 400 && !S.Terrain->HasCell(TLots); ++F)
	{
		S.Terrain->Pump(Near);
		FPlatformProcess::Sleep(0.001f);
	}
	S.Terrain->Pump(Near, 0.0, 64); // launch many jobs, apply none
	const int32 DroppedBefore = S.Terrain->GetStats().StaleDropped;
	// Reloaded with 3 m more ground around Near (vertices 640..739 x 460..559 of the lots field).
	S.Terrain->RemoveCell(TLots);
	TArray<int32> Indices, Delta;
	for (int32 Y = 460; Y < 560; ++Y)
	{
		for (int32 X = 640; X < 740; ++X)
		{
			Indices.Add(Y * 1025 + X);
			Delta.Add(300);
		}
	}
	S.Terrain->BeginCellGround(TLots, Indices, Delta);
	// No Pump until the new field is ready (a Pump would drop the old results for lack of a ground).
	for (int32 F = 0; F < 20000 && !S.Terrain->IsCellFieldReady(TLots); ++F)
	{
		FPlatformProcess::Sleep(0.001f);
	}
	FPlatformProcess::Sleep(0.5f); // and the old mesh jobs have long finished
	TestTrue(TEXT("the reloaded field is ready before any Pump"), S.Terrain->IsCellFieldReady(TLots));
	for (int32 F = 0; F < 4000 && (!S.Terrain->HasCell(TLots) || !S.Terrain->IsCellComplete(TLots)); ++F)
	{
		S.Terrain->Pump(Near, 0.05, 16);
		FPlatformProcess::Sleep(0.0005f);
	}
	TestTrue(TEXT("the reloaded lots streamed in"), S.Terrain->IsCellComplete(TLots));
	AddInfo(FString::Printf(TEXT("stale mesh results dropped: %d"), S.Terrain->GetStats().StaleDropped - DroppedBefore));
	TestTrue(TEXT("old jobs were in flight and were dropped"), S.Terrain->GetStats().StaleDropped > DroppedBefore);
	TestTrue(TEXT("integrity"), S.Integrity(*this, TEXT("after reload")));
	int32 Wrong = 0;
	for (int32 I = 0; I < 36; ++I)
	{
		const FVector2D At = Near + FVector2D(-4500.0 + (I % 6) * 1800.0, -4500.0 + (I / 6) * 1800.0);
		const double Z = S.TraceZ(At), G = S.Terrain->HeightAt(At);
		if (FMath::Abs(Z - G) > 5.0)
		{
			++Wrong;
			AddError(FString::Printf(TEXT("at %s the chunk shows %.0f, the ground is %.0f (a stale mesh landed)"), *At.ToString(), Z, G));
		}
	}
	TestEqual(TEXT("every probe on and around the raised patch collides at the new ground"), Wrong, 0);
	TestEqual(TEXT("every chunk is rebound"), S.CheckRebound(*this, TLots), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPoolBounded, "Gridlands.Game.TerrainPool.PoolStaysInsideItsBound", GLTestUtils::Flags)
bool FGLPoolBounded::RunTest(const FString& Parameters)
{
	FPoolScene S(TEXT("GLPoolBoundWorld"));
	S.Terrain->PoolLimit = 100;
	for (int32 Round = 0; Round < 3; ++Round)
	{
		S.Load(Round % 2 ? TLots : TOrigin);
		S.Terrain->RemoveCell(Round % 2 ? TLots : TOrigin);
		S.Terrain->FlushAll();
		TestTrue(*FString::Printf(TEXT("round %d: the pool holds at most its bound (%d)"), Round, S.Terrain->NumPooled()), S.Terrain->NumPooled() <= 100);
		TestTrue(TEXT("  integrity"), S.Integrity(*this, TEXT("bounded")));
	}
	TestTrue(TEXT("chunks beyond the bound were destroyed, not kept"), S.Terrain->GetStats().PoolOverflowDestroyed >= 156);
	int32 Actors = 0;
	for (TActorIterator<AGLTerrainChunk> It(S.Test.World); It; ++It)
	{
		Actors += IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
	}
	TestTrue(*FString::Printf(TEXT("chunk actors in the world are bounded by live + pool (%d)"), Actors), Actors <= 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPoolCollision, "Gridlands.Game.TerrainPool.StreamedReusedChunksCollideAsTheirNewCell", GLTestUtils::Flags)
bool FGLPoolCollision::RunTest(const FString& Parameters)
{
	// Streamed (not flushed) onto chunks that last carried the origin's ground, including its pit: the
	// moment a lots chunk is applied, its collision is the lots' ground. Never the previous owner's
	// (stale) and never absent (one synchronous cook per apply).
	FPoolScene S(TEXT("GLPoolCollisionWorld"));
	S.Load(TOrigin);
	for (int32 I = 0; I < 3; ++I)
	{
		FGLTerrainEdit Dig;
		Dig.Op = EGLTerrainOp::Dig;
		Dig.Centre = FVector2D(1000.0, -1200.0);
		Dig.RadiusCm = 300.0;
		Dig.AmountCm = 100.0;
		S.Terrain->ApplyEdit(Dig);
	}
	S.Terrain->RemoveCell(TOrigin);
	S.Terrain->FlushAll();
	S.Terrain->BeginCellGround(TLots, {}, {});
	for (int32 F = 0; F < 4000 && (!S.Terrain->HasCell(TLots) || !S.Terrain->IsCellComplete(TLots)); ++F)
	{
		S.Terrain->Pump(FVector2D(102400.0, 0.0), 0.05, 64);
		FPlatformProcess::Sleep(0.0005f);
	}
	TestTrue(TEXT("the lots streamed in (no flush)"), S.Terrain->IsCellComplete(TLots));
	TestTrue(TEXT("onto reused chunks"), S.Terrain->GetStats().ChunksReused >= 200);
	int32 Stale = 0, Missing = 0;
	for (int32 I = 0; I < 48; ++I)
	{
		const FVector2D At(102400.0 - 50000.0 + (I % 8) * 12500.0, -50000.0 + (I / 8) * 16000.0);
		const double Z = S.TraceZ(At);
		Missing += Z < -1.0e8 ? 1 : 0;
		if (Z > -1.0e8 && FMath::Abs(Z - S.Terrain->HeightAt(At)) > 5.0)
		{
			++Stale;
			AddError(FString::Printf(TEXT("stale collision at %s: hit %.0f, ground %.0f"), *At.ToString(), Z, S.Terrain->HeightAt(At)));
		}
	}
	TestEqual(TEXT("no streamed chunk answers with its previous owner's collision"), Stale, 0);
	TestEqual(TEXT("and none is without collision"), Missing, 0);
	const FVector2D Same(1000.0 + 102400.0, -1200.0);
	TestEqual(TEXT("where the pit's chunk now lies, the lots' ground (the edit did not travel with the actor)"), S.TraceZ(Same), S.Terrain->HeightAt(Same), 5.0);
	TestEqual(TEXT("every chunk is rebound"), S.CheckRebound(*this, TLots), 0);
	TestTrue(TEXT("integrity"), S.Integrity(*this, TEXT("streamed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPoolCancel, "Gridlands.Game.TerrainPool.CancellationNeverHandsOutAHalfResetChunk", GLTestUtils::Flags)
bool FGLPoolCancel::RunTest(const FString& Parameters)
{
	FPoolScene S(TEXT("GLPoolCancelWorld"));
	S.Load(TOrigin);
	const int32 Chunks = S.ChunksOf(TOrigin).Num();
	// The origin unloads; only a couple of frames of retirement happen before the lots need chunks.
	S.Terrain->RemoveCell(TOrigin);
	S.Terrain->Pump(FVector2D(0.0, 0.0));
	S.Terrain->Pump(FVector2D(0.0, 0.0));
	TestTrue(*FString::Printf(TEXT("retirement is partial (%d retiring, %d pooled)"), S.Terrain->NumRetiring(), S.Terrain->NumPooled()), S.Terrain->NumRetiring() > 0 && S.Terrain->NumPooled() > 0);
	TestTrue(TEXT("integrity while half retired"), S.Integrity(*this, TEXT("half retired")));
	S.Load(TLots);
	TestEqual(TEXT("no half-reset chunk was ever taken"), S.Terrain->GetStats().PoolRejected, 0);
	TestTrue(TEXT("integrity after the lots took chunks"), S.Integrity(*this, TEXT("after reuse")));
	TestEqual(TEXT("the lots are whole and clean"), S.CheckRebound(*this, TLots), 0);
	TestEqual(TEXT("as many lots chunks as origin chunks"), S.ChunksOf(TLots).Num(), Chunks);
	return true;
}

#endif
