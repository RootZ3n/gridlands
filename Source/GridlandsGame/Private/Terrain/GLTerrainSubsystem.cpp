#include "Terrain/GLTerrainSubsystem.h"

#include "Noise/GLNoiseSubsystem.h"
#include "Structure/GLStructureSubsystem.h"

#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Tasks/Task.h"
#include "Terrain/GLCellNavBounds.h"
#include "Terrain/GLTerrainChunk.h"
#include "World/GLGridCells.h"

namespace
{
	// Vertical room for navigation above and below the base: the edit limits plus headroom for pieces.
	constexpr double NavHeadroomCm = 1500.0;
	// Chunk actors of an unloaded ground cleared per frame (the rest wait, hidden, without collision).
	constexpr int32 RetirePerFrame = 16;
	// Cleared chunk actors kept for reuse: two cells' worth (ADR-0027: 256 per cell). Beyond, destroyed.
	constexpr int32 MaxPooled = 512;
}

/** A cell's field built off the game thread (P5). */
struct FGLPendingGround
{
	int32 Generation = 0;
	FGLGroundParams Params;
	UE::Tasks::TTask<TSharedPtr<FGLHeightfield>> Task;
};

/** A chunk mesh being built off the game thread (P5). */
struct FGLMeshJob
{
	FName Cell;
	int32 Generation = 0;
	int32 Slot = 0;
	int32 Version = 0;
	double DistanceSq = 0.0;
	UE::Tasks::TTask<TSharedPtr<UE::Geometry::FDynamicMesh3>> Task;
};

UGLTerrainSubsystem::UGLTerrainSubsystem() = default;
UGLTerrainSubsystem::~UGLTerrainSubsystem() = default;

void UGLTerrainSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// A map that is itself a cell's level gets that cell's ground (legacy single-cell maps). The
	// Grid map (P3) has no cell of its own: UGLGridSubsystem streams grounds in and out.
	const FString MapPackage = InWorld.GetOutermost()->GetName();
	GLContent::Get().ForEachEntry([this, &MapPackage](const FGLContentEntry& Entry)
	{
		const FGLCellDef* Cell = Entry.Definition.GetPtr<FGLCellDef>();
		if (Cell && Entry.Kind == TEXT("cell") && !Cell->Level.IsEmpty() && MapPackage.EndsWith(FPaths::GetBaseFilename(Cell->Level)))
		{
			SetupCell(Entry.Id);
		}
	});
}

bool UGLTerrainSubsystem::GroundParamsFor(FName CellId, FGLGroundParams& Out)
{
	const FGLCellDef* Cell = GLContent::Get().Find<FGLCellDef>(CellId);
	if (!Cell || !Cell->HasTerrain())
	{
		return false;
	}
	const FGLCellTerrainDef& T = Cell->Terrain;
	// The ground covers the whole cell (GRID-3, ADR-0027: 1024 m, 1 m, 64 m chunks).
	const double SizeM = Cell->SizeMetres > 0.0 ? Cell->SizeMetres : 2.0 * FMath::Max(Cell->PlayableHalfExtent, T.ChunkMetres * 0.5);
	Out.ChunksX = Out.ChunksY = FMath::Max(1, FMath::CeilToInt(SizeM / T.ChunkMetres));
	Out.ChunkVerts = FMath::RoundToInt(T.ChunkMetres / T.SpacingMetres) + 1;
	const double Half = Out.ChunksX * T.ChunkMetres * 50.0;
	Out.Origin = Cell->CentreCm() - FVector2D(Half, Half);
	Out.SpacingCm = T.SpacingMetres * 100.0;
	Out.BaseHeightCm = T.BaseHeight * 100.0;
	Out.MaxDigCm = T.MaxDigDepth * 100.0;
	Out.MaxRaiseCm = T.MaxRaiseHeight * 100.0;
	Out.ReliefSeed = T.Relief.Seed;
	Out.ReliefAmplitudeCm = T.Relief.AmplitudeMetres * 100.0;
	Out.ReliefEdgeBlendVerts = T.Relief.EdgeBlendMetres / T.SpacingMetres;
	return true;
}

TSharedPtr<FGLHeightfield> UGLTerrainSubsystem::MakeField(const FGLGroundParams& P, TConstArrayView<int32> DeltaIndices, TConstArrayView<int32> DeltaCm)
{
	TSharedPtr<FGLHeightfield> Field = MakeShared<FGLHeightfield>();
	const int32 Verts = P.ChunksX * (P.ChunkVerts - 1) + 1;
	Field->Init(P.Origin, Verts, P.ChunksY * (P.ChunkVerts - 1) + 1, P.SpacingCm, P.BaseHeightCm, P.MaxDigCm, P.MaxRaiseCm);
	if (P.ReliefAmplitudeCm > 0.0)
	{
		TArray<float> Relief = GLTerrainGen::Rolling(P.ReliefSeed, Verts, Verts, P.SpacingCm, P.ReliefAmplitudeCm);
		// Flat to the base height near every edge: neighbouring cells always meet without seams.
		for (int32 Y = 0; Y < Verts; ++Y)
		{
			for (int32 X = 0; X < Verts; ++X)
			{
				const double Edge = FMath::Min(FMath::Min(X, Verts - 1 - X), FMath::Min(Y, Verts - 1 - Y));
				const double W = FMath::SmoothStep(0.0, 1.0, FMath::Clamp(Edge / FMath::Max(1.0, P.ReliefEdgeBlendVerts), 0.0, 1.0));
				float& H = Relief[Y * Verts + X];
				H = static_cast<float>(P.BaseHeightCm + W * H);
			}
		}
		Field->SetBase(MoveTemp(Relief));
	}
	// Saved edits go in before the first build: each chunk is built once, already edited.
	if (DeltaIndices.Num() > 0 && !Field->ApplyDelta(DeltaIndices, DeltaCm))
	{
		UE_LOG(LogGridlands, Warning, TEXT("Terrain: saved edits do not fit this ground; ground left as authored"));
	}
	return Field;
}

bool UGLTerrainSubsystem::SetupCell(FName CellId, TConstArrayView<int32> DeltaIndices, TConstArrayView<int32> DeltaCm)
{
	FGLGroundParams P;
	if (!GroundParamsFor(CellId, P))
	{
		return false;
	}
	CancelPending(CellId);
	TSharedPtr<FGLHeightfield> Field = MakeField(P, DeltaIndices, DeltaCm);
	FinishGround(CellId, MoveTemp(*Field), P, ++GenerationCounter, true);
	UE_LOG(LogGridlands, Log, TEXT("Terrain: %s ground %dx%d chunks built at once (%d edited vertices)"), *CellId.ToString(), P.ChunksX, P.ChunksY, DeltaIndices.Num());
	return true;
}

bool UGLTerrainSubsystem::BeginCellGround(FName CellId, TConstArrayView<int32> DeltaIndices, TConstArrayView<int32> DeltaCm)
{
	FGLGroundParams P;
	if (!GroundParamsFor(CellId, P) || Grounds.Contains(CellId) || Pending.Contains(CellId))
	{
		return false;
	}
	TSharedPtr<FGLPendingGround> Job = MakeShared<FGLPendingGround>();
	Job->Generation = ++GenerationCounter;
	Job->Params = P;
	TArray<int32> Indices(DeltaIndices.GetData(), DeltaIndices.Num()), Deltas(DeltaCm.GetData(), DeltaCm.Num());
	// The field (relief + saved edits) is built on a worker; nothing here touches the world.
	Job->Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [P, Indices = MoveTemp(Indices), Deltas = MoveTemp(Deltas)]()
	{
		return MakeField(P, Indices, Deltas);
	});
	Pending.Add(CellId, Job);
	return true;
}

void UGLTerrainSubsystem::CancelPending(FName CellId)
{
	Pending.Remove(CellId); // the task finishes on its own; its result is simply never used
}

void UGLTerrainSubsystem::Setup(const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm, float BaseHeightCm,
	double MaxDigCm, double MaxRaiseCm, TArray<float>* AuthoredBase)
{
	FGLGroundParams P;
	P.Origin = Origin;
	P.ChunksX = ChunksX;
	P.ChunksY = ChunksY;
	P.ChunkVerts = ChunkVerts;
	P.SpacingCm = SpacingCm;
	P.BaseHeightCm = BaseHeightCm;
	P.MaxDigCm = MaxDigCm;
	P.MaxRaiseCm = MaxRaiseCm;
	FGLHeightfield Field;
	Field.Init(Origin, ChunksX * (ChunkVerts - 1) + 1, ChunksY * (ChunkVerts - 1) + 1, SpacingCm, BaseHeightCm, MaxDigCm, MaxRaiseCm);
	if (AuthoredBase && !Field.SetBase(MoveTemp(*AuthoredBase)))
	{
		UE_LOG(LogGridlands, Error, TEXT("Terrain: authored base has the wrong size; using flat ground"));
	}
	FinishGround(NAME_None, MoveTemp(Field), P, ++GenerationCounter, true);
}

void UGLTerrainSubsystem::FinishGround(FName CellId, FGLHeightfield&& Field, const FGLGroundParams& P, int32 Generation, bool bBuildNow)
{
	RemoveCell(CellId);
	UWorld* World = GetWorld();
	FGLCellGround& Ground = Grounds.Add(CellId);
	Ground.Generation = Generation;
	Ground.VertsPerChunk = P.ChunkVerts;
	Ground.Field = MoveTemp(Field);
	for (int32 CY = 0; CY < P.ChunksY; ++CY)
	{
		for (int32 CX = 0; CX < P.ChunksX; ++CX)
		{
			FGLChunkSlot& Slot = Ground.Slots.AddDefaulted_GetRef();
			Slot.First = FIntPoint(CX * (P.ChunkVerts - 1), CY * (P.ChunkVerts - 1));
			if (bBuildNow)
			{
				BuildSlotNow(Ground, Slot, false);
			}
		}
	}
	// The cell declares its navigable volume; navigation builds inside it (around invokers, ADR-0029).
	const FVector2D Size((Ground.Field.GetVertsX() - 1) * P.SpacingCm, (Ground.Field.GetVertsY() - 1) * P.SpacingCm);
	const FVector Centre(P.Origin.X + Size.X * 0.5, P.Origin.Y + Size.Y * 0.5, P.BaseHeightCm);
	Ground.NavBounds = World->SpawnActor<AGLCellNavBounds>(Centre, FRotator::ZeroRotator);
	if (Ground.NavBounds)
	{
		// Components register during spawn, so the volume first reports its default box; resize,
		// then tell navigation the bounds changed.
		Ground.NavBounds->SetExtent(FVector(Size.X * 0.5, Size.Y * 0.5, FMath::Max(P.MaxDigCm, P.MaxRaiseCm) + NavHeadroomCm + 2000.0));
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			Nav->OnNavigationBoundsUpdated(Ground.NavBounds);
		}
	}
}

AGLTerrainChunk* UGLTerrainSubsystem::AcquireChunk(const FGLCellGround& Ground, const FGLChunkSlot& Slot)
{
	const FVector2D At = Ground.Field.VertexLocation(Slot.First.X, Slot.First.Y);
	AGLTerrainChunk* Chunk = nullptr;
	while (!Chunk && Pool.Num() > 0)
	{
		Chunk = Pool.Pop().Get();
	}
	if (Chunk)
	{
		Chunk->SetActorLocation(FVector(At.X, At.Y, 0.0));
		Chunk->SetActorHiddenInGame(false);
		Chunk->SetActorEnableCollision(true);
		++Stats.ChunksReused;
	}
	else
	{
		Chunk = GetWorld()->SpawnActor<AGLTerrainChunk>(FVector(At.X, At.Y, 0.0), FRotator::ZeroRotator);
	}
	Chunk->Setup(Slot.First, Ground.VertsPerChunk);
	return Chunk;
}

void UGLTerrainSubsystem::BuildSlotNow(FGLCellGround& Ground, FGLChunkSlot& Slot, bool bAsyncCollision)
{
	if (!Slot.Actor)
	{
		Slot.Actor = AcquireChunk(Ground, Slot);
	}
	Slot.Actor->ApplyMesh(AGLTerrainChunk::BuildMesh(AGLTerrainChunk::MakeSnapshot(Ground.Field, Slot.First, Ground.VertsPerChunk)), true, bAsyncCollision);
	Slot.BuiltVersion = Slot.Version;
}

void UGLTerrainSubsystem::Pump(const FVector2D& Near, double BudgetSeconds, int32 MaxInFlight)
{
	const double Start = FPlatformTime::Seconds();
	// 1. Fields that finished on workers become grounds (cheap: nav bounds and empty slots).
	for (auto It = Pending.CreateIterator(); It; ++It)
	{
		if (It.Value()->Task.IsCompleted())
		{
			const TSharedPtr<FGLPendingGround> Job = It.Value();
			const FName Cell = It.Key();
			It.RemoveCurrent();
			TSharedPtr<FGLHeightfield> Field = Job->Task.GetResult();
			FinishGround(Cell, MoveTemp(*Field), Job->Params, Job->Generation, false);
			break; // one per frame
		}
	}
	// 2. Finished meshes, nearest first, while the budget lasts. Stale results are dropped.
	Jobs.Sort([](const TSharedPtr<FGLMeshJob>& A, const TSharedPtr<FGLMeshJob>& B) { return A->DistanceSq < B->DistanceSq; });
	for (int32 I = 0; I < Jobs.Num(); )
	{
		const TSharedPtr<FGLMeshJob> Job = Jobs[I];
		FGLCellGround* Ground = Grounds.Find(Job->Cell);
		const bool bStale = !Ground || Ground->Generation != Job->Generation || !Ground->Slots.IsValidIndex(Job->Slot)
			|| Ground->Slots[Job->Slot].Version != Job->Version;
		if (bStale)
		{
			if (Ground && Ground->Generation == Job->Generation && Ground->Slots.IsValidIndex(Job->Slot))
			{
				Ground->Slots[Job->Slot].InFlightVersion = -1;
			}
			++Stats.StaleDropped;
			Jobs.RemoveAt(I);
			continue;
		}
		if (!Job->Task.IsCompleted() || FPlatformTime::Seconds() - Start > BudgetSeconds)
		{
			++I;
			continue;
		}
		FGLChunkSlot& Slot = Ground->Slots[Job->Slot];
		if (!Slot.Actor)
		{
			Slot.Actor = AcquireChunk(*Ground, Slot);
		}
		Slot.Actor->ApplyMesh(MoveTemp(*Job->Task.GetResult()), true, true);
		Slot.BuiltVersion = Job->Version;
		Slot.InFlightVersion = -1;
		++Stats.ChunksApplied;
		Jobs.RemoveAt(I);
	}
	// 3. Start new mesh builds on workers for the nearest chunks that need one.
	struct FWant { FName Cell; int32 Slot; double DistanceSq; };
	TArray<FWant> Wanted;
	for (TPair<FName, FGLCellGround>& Entry : Grounds)
	{
		FGLCellGround& Ground = Entry.Value;
		for (int32 S = 0; S < Ground.Slots.Num(); ++S)
		{
			const FGLChunkSlot& Slot = Ground.Slots[S];
			if (Slot.BuiltVersion != Slot.Version && Slot.InFlightVersion != Slot.Version)
			{
				const double Half = (Ground.VertsPerChunk - 1) * Ground.Field.GetSpacing() * 0.5;
				const FVector2D Centre = Ground.Field.VertexLocation(Slot.First.X, Slot.First.Y) + FVector2D(Half, Half);
				Wanted.Add({ Entry.Key, S, FVector2D::DistSquared(Centre, Near) });
			}
		}
	}
	Wanted.Sort([](const FWant& A, const FWant& B) { return A.DistanceSq < B.DistanceSq; });
	for (const FWant& Want : Wanted)
	{
		if (Jobs.Num() >= MaxInFlight)
		{
			break;
		}
		FGLCellGround& Ground = Grounds[Want.Cell];
		FGLChunkSlot& Slot = Ground.Slots[Want.Slot];
		TSharedPtr<FGLMeshJob> Job = MakeShared<FGLMeshJob>();
		Job->Cell = Want.Cell;
		Job->Generation = Ground.Generation;
		Job->Slot = Want.Slot;
		Job->Version = Slot.Version;
		Job->DistanceSq = Want.DistanceSq;
		FGLChunkSnapshot Snapshot = AGLTerrainChunk::MakeSnapshot(Ground.Field, Slot.First, Ground.VertsPerChunk);
		Job->Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Snapshot = MoveTemp(Snapshot)]()
		{
			return TSharedPtr<UE::Geometry::FDynamicMesh3>(MakeShared<UE::Geometry::FDynamicMesh3>(AGLTerrainChunk::BuildMesh(Snapshot)));
		});
		Slot.InFlightVersion = Slot.Version;
		Jobs.Add(Job);
	}
	// 4. Retire an unloaded ground's chunk actors a few at a time: clear (frees mesh and collision now), pool.
	for (int32 N = 0; N < RetirePerFrame && Retiring.Num() > 0; ++N)
	{
		RetireOne();
	}
}

bool UGLTerrainSubsystem::EnsureReadyAt(const FVector2D& World, double RadiusCm)
{
	// A pending field under this point is finished now (waiting on its worker if needed).
	for (auto It = Pending.CreateIterator(); It; ++It)
	{
		const FGLGroundParams& P = It.Value()->Params;
		const FVector2D Size(P.ChunksX * (P.ChunkVerts - 1) * P.SpacingCm, P.ChunksY * (P.ChunkVerts - 1) * P.SpacingCm);
		if (World.X >= P.Origin.X && World.Y >= P.Origin.Y && World.X <= P.Origin.X + Size.X && World.Y <= P.Origin.Y + Size.Y)
		{
			const TSharedPtr<FGLPendingGround> Job = It.Value();
			const FName Cell = It.Key();
			It.RemoveCurrent();
			TSharedPtr<FGLHeightfield> Field = Job->Task.GetResult(); // waits
			FinishGround(Cell, MoveTemp(*Field), Job->Params, Job->Generation, false);
			++Stats.EmergencyFields;
			break;
		}
	}
	bool bAny = false;
	for (TPair<FName, FGLCellGround>& Entry : Grounds)
	{
		FGLCellGround& Ground = Entry.Value;
		const double ChunkCm = (Ground.VertsPerChunk - 1) * Ground.Field.GetSpacing();
		for (FGLChunkSlot& Slot : Ground.Slots)
		{
			const FVector2D Min = Ground.Field.VertexLocation(Slot.First.X, Slot.First.Y);
			const bool bNear = World.X >= Min.X - RadiusCm && World.X <= Min.X + ChunkCm + RadiusCm && World.Y >= Min.Y - RadiusCm && World.Y <= Min.Y + ChunkCm + RadiusCm;
			if (!bNear)
			{
				continue;
			}
			bAny = true;
			if (Slot.BuiltVersion != Slot.Version || !Slot.Actor)
			{
				// Never let Zenny stand over missing ground: build it here, with collision at once.
				BuildSlotNow(Ground, Slot, false);
				++Stats.EmergencyChunks;
			}
		}
	}
	return bAny;
}

bool UGLTerrainSubsystem::RemoveCell(FName CellId)
{
	CancelPending(CellId);
	FGLCellGround Ground;
	if (!Grounds.RemoveAndCopyValue(CellId, Ground))
	{
		return false;
	}
	for (const FGLChunkSlot& Slot : Ground.Slots)
	{
		if (AGLTerrainChunk* Chunk = Slot.Actor.Get())
		{
			// Gone from the game at once (no collision, not drawn); destroyed a few per frame.
			Chunk->SetActorHiddenInGame(true);
			Chunk->SetActorEnableCollision(false);
			Retiring.Add(Chunk);
		}
	}
	if (Ground.NavBounds)
	{
		Ground.NavBounds->Destroy();
	}
	return true;
}

bool UGLTerrainSubsystem::IsCellComplete(FName CellId) const
{
	const FGLCellGround* Ground = Grounds.Find(CellId);
	return Ground && !Ground->Slots.ContainsByPredicate([](const FGLChunkSlot& S) { return S.BuiltVersion != S.Version || !S.Actor; });
}

int32 UGLTerrainSubsystem::NumChunks() const
{
	int32 N = 0;
	for (const TPair<FName, FGLCellGround>& G : Grounds)
	{
		N += G.Value.Slots.FilterByPredicate([](const FGLChunkSlot& S) { return S.Actor != nullptr; }).Num();
	}
	return N;
}

void UGLTerrainSubsystem::FlushAll()
{
	for (auto It = Pending.CreateIterator(); It; ++It)
	{
		const TSharedPtr<FGLPendingGround> Job = It.Value();
		const FName Cell = It.Key();
		It.RemoveCurrent();
		TSharedPtr<FGLHeightfield> Field = Job->Task.GetResult();
		FinishGround(Cell, MoveTemp(*Field), Job->Params, Job->Generation, false);
	}
	Jobs.Reset();
	for (TPair<FName, FGLCellGround>& Entry : Grounds)
	{
		for (FGLChunkSlot& Slot : Entry.Value.Slots)
		{
			if (Slot.BuiltVersion != Slot.Version || !Slot.Actor)
			{
				BuildSlotNow(Entry.Value, Slot, false);
			}
			Slot.InFlightVersion = -1;
		}
	}
	while (Retiring.Num() > 0)
	{
		RetireOne();
	}
}

void UGLTerrainSubsystem::RetireOne()
{
	AGLTerrainChunk* Chunk = Retiring.Pop().Get();
	if (!Chunk)
	{
		return;
	}
	if (Pool.Num() < MaxPooled)
	{
		Chunk->ClearForPool();
		Pool.Add(Chunk);
	}
	else
	{
		Chunk->Destroy();
	}
}

const FGLCellGround* UGLTerrainSubsystem::GroundAt(const FVector2D& World) const
{
	for (const TPair<FName, FGLCellGround>& Entry : Grounds)
	{
		if (Entry.Value.Field.Contains(World))
		{
			return &Entry.Value;
		}
	}
	return nullptr;
}

bool UGLTerrainSubsystem::GroundCellAt(const FVector2D& World, FName& OutCell) const
{
	for (const TPair<FName, FGLCellGround>& Entry : Grounds)
	{
		if (Entry.Value.Field.Contains(World))
		{
			OutCell = Entry.Key;
			return true;
		}
	}
	return false;
}

double UGLTerrainSubsystem::HeightAt(const FVector2D& World) const
{
	const FGLCellGround* Ground = GroundAt(World);
	return Ground ? Ground->Field.HeightAt(World) : (Grounds.Num() == 1 ? Grounds.CreateConstIterator()->Value.Field.HeightAt(World) : 0.0);
}

double UGLTerrainSubsystem::EditedAt(const FVector2D& World) const
{
	const FGLCellGround* Ground = GroundAt(World);
	return Ground ? Ground->Field.EditedAt(World) : 0.0;
}

const FGLHeightfield& UGLTerrainSubsystem::GetField() const
{
	static const FGLHeightfield Empty;
	if (const FGLCellGround* None = Grounds.Find(NAME_None))
	{
		return None->Field;
	}
	return Grounds.Num() > 0 ? Grounds.CreateConstIterator()->Value.Field : Empty;
}

TArray<FName> UGLTerrainSubsystem::GetGroundCells() const
{
	TArray<FName> Cells;
	Grounds.GetKeys(Cells);
	return Cells;
}

FGLTerrainEditResult UGLTerrainSubsystem::ApplyEdit(const FGLTerrainEdit& Edit, TFunctionRef<bool(const FVector2D&)> IsProtected)
{
	FGLTerrainEditResult Result;
	if (Grounds.Num() == 0)
	{
		Result.Refusal = TEXT("no ground here");
		return Result;
	}
	// A brush that reaches into a Grid cell whose ground is not loaded is refused: the two cells
	// would disagree at their shared edge (a seam) when it loads. (Explicit tool ground, id None,
	// is not part of the Grid.)
	if (!Grounds.Contains(NAME_None))
	{
		for (const FName& Cell : GLGridCells::AllCells())
		{
			const FGLCellDef* Def = GLContent::Get().Find<FGLCellDef>(Cell);
			if (Def && Def->HasTerrain() && !Grounds.Contains(Cell) && GLGridCells::DistanceToCell(*Def, Edit.Centre) < Edit.RadiusCm)
			{
				Result.Refusal = TEXT("the ground beyond is not loaded");
				return Result;
			}
		}
	}
	// Dry-run every ground the brush touches; commit only if none refuses (atomic across cells,
	// without copying whole fields).
	TArray<FGLCellGround*> Touched;
	const FBox2D Brush(Edit.Centre - FVector2D(Edit.RadiusCm), Edit.Centre + FVector2D(Edit.RadiusCm));
	for (TPair<FName, FGLCellGround>& Entry : Grounds)
	{
		const FGLHeightfield& F = Entry.Value.Field;
		const FBox2D Box(F.GetOrigin(), F.GetOrigin() + FVector2D((F.GetVertsX() - 1) * F.GetSpacing(), (F.GetVertsY() - 1) * F.GetSpacing()));
		if (Box.Intersect(Brush))
		{
			Touched.Add(&Entry.Value);
		}
	}
	bool bAny = false;
	for (FGLCellGround* Ground : Touched)
	{
		const FGLTerrainEditResult Trial = Ground->Field.Apply(Edit, IsProtected, true);
		if (!Trial.bApplied && Trial.Refusal != TEXT("nothing to change") && Trial.Refusal != TEXT("outside this cell's ground"))
		{
			return Trial;
		}
		bAny |= Trial.bApplied;
		if (!Trial.bApplied && Result.Refusal.IsEmpty())
		{
			Result.Refusal = Trial.Refusal;
		}
	}
	if (!bAny)
	{
		if (Result.Refusal.IsEmpty())
		{
			Result.Refusal = TEXT("outside the loaded ground");
		}
		return Result;
	}
	Result = FGLTerrainEditResult();
	for (FGLCellGround* Ground : Touched)
	{
		const FGLTerrainEditResult Done = Ground->Field.Apply(Edit, IsProtected);
		if (!Done.bApplied)
		{
			continue;
		}
		const double ChunkVerts = Ground->VertsPerChunk;
		for (FGLChunkSlot& Slot : Ground->Slots)
		{
			const bool bCovers = Done.DirtyVertices.Min.X <= Slot.First.X + ChunkVerts - 1 && Done.DirtyVertices.Max.X >= Slot.First.X
				&& Done.DirtyVertices.Min.Y <= Slot.First.Y + ChunkVerts - 1 && Done.DirtyVertices.Max.Y >= Slot.First.Y;
			if (!bCovers)
			{
				continue;
			}
			++Slot.Version; // anything in flight for this chunk is now stale
			if (Slot.Actor && Slot.Actor->IsBuilt())
			{
				Slot.Actor->Rebuild(Ground->Field, bNotifyNavigation); // player edits: collision at once
				Slot.BuiltVersion = Slot.Version;
			}
		}
		if (!Result.bApplied)
		{
			Result = Done;
		}
		else
		{
			Result.VerticesChanged += Done.VerticesChanged;
			Result.VolumeM3 += Done.VolumeM3;
		}
	}
	return Result;
}

FGLTerrainEditResult UGLTerrainSubsystem::Terraform(AActor* Instigator, FName TerraformId, const FVector2D& Centre)
{
	FGLTerrainEditResult Refused;
	const FGLTerraformDef* Def = GLContent::Get().Find<FGLTerraformDef>(TerraformId);
	UGLInventoryComponent* Inventory = Instigator ? Instigator->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	auto Refuse = [&](const FString& Reason)
	{
		Refused.Refusal = Reason;
		Emit(TEXT("Event.Terrain.Refused"), TerraformId, Instigator, Reason);
		return Refused;
	};
	if (!Def || !Inventory)
	{
		return Refuse(TEXT("unknown tool stroke"));
	}
	const bool bHasTool = Inventory->GetInventory().GetStacks().ContainsByPredicate([Def](const FGLInventoryStack& Stack)
	{
		const FGLItemDef* Item = GLContent::Get().Find<FGLItemDef>(Stack.Item);
		return Item && Item->Tool.ToolClass == Def->RequiresTool;
	});
	if (!bHasTool)
	{
		return Refuse(TEXT("needs a digging tool"));
	}
	for (const FGLItemStackDef& Cost : Def->Cost)
	{
		if (Inventory->CountOf(Cost.Item) < Cost.Count)
		{
			return Refuse(TEXT("nothing to build the ground up with"));
		}
	}
	FGLInventory Trial = Inventory->GetInventory();
	for (const FGLItemStackDef& Cost : Def->Cost)
	{
		Trial.Remove(Cost.Item, Cost.Count);
	}
	for (const FGLItemStackDef& Yield : Def->Yields)
	{
		if (Trial.Add(GLContent::Get(), Yield.Item, Yield.Count) != Yield.Count)
		{
			return Refuse(TEXT("no room to carry the soil"));
		}
	}

	FGLTerrainEdit Edit;
	Edit.Op = Def->Op == TEXT("RAISE") ? EGLTerrainOp::Raise : Def->Op == TEXT("FLATTEN") ? EGLTerrainOp::Flatten : EGLTerrainOp::Dig;
	Edit.Centre = Centre;
	Edit.RadiusCm = Def->Radius * 100.0;
	Edit.AmountCm = Def->Amount * 100.0;
	Edit.TargetHeightCm = HeightAt(Centre);
	const UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	// Ground under a player piece, an intact grounded authored part or debris does not move (P6: refused for now).
	const FGLTerrainEditResult Result = ApplyEdit(Edit, [Building, Structures](const FVector2D& At)
	{
		return (Building && Building->IsUnderStructure(At)) || (Structures && Structures->IsUnderStructure(At));
	});
	if (!Result.bApplied)
	{
		return Refuse(Result.Refusal);
	}
	// The ground changed: settle the items (the trial above proved they fit).
	for (const FGLItemStackDef& Cost : Def->Cost)
	{
		verify(Inventory->RemoveItem(Cost.Item, Cost.Count));
	}
	for (const FGLItemStackDef& Yield : Def->Yields)
	{
		verify(Inventory->AddItem(Yield.Item, Yield.Count) == Yield.Count);
	}
	Emit(TEXT("Event.Terrain.Edited"), TerraformId, Instigator, FString());
	// Terraforming is never silent (P6): creatures in earshot hear it.
	const TCHAR* Sound = Edit.Op == EGLTerrainOp::Raise ? TEXT("Noise.Terrain.Raise") : Edit.Op == EGLTerrainOp::Flatten ? TEXT("Noise.Terrain.Flatten") : TEXT("Noise.Terrain.Dig");
	UGLNoiseSubsystem::EmitAction(this, Sound, FVector(Centre, HeightAt(Centre)), Instigator);
	return Result;
}

void UGLTerrainSubsystem::Emit(const TCHAR* Tag, FName Subject, AActor* Instigator, const FString& Reason)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
	Event.Subject = Subject;
	Event.Instigator = Instigator;
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
	if (!Reason.IsEmpty())
	{
		UE_LOG(LogGridlands, Log, TEXT("Terrain: %s refused: %s"), *Subject.ToString(), *Reason);
	}
}

bool UGLTerrainSubsystem::CaptureCellDelta(FName CellId, TArray<int32>& OutIndices, TArray<int32>& OutDeltaCm) const
{
	const FGLCellGround* Ground = Grounds.Find(CellId);
	if (!Ground)
	{
		return false;
	}
	Ground->Field.EncodeDelta(OutIndices, OutDeltaCm);
	return true;
}

void UGLTerrainSubsystem::CaptureDelta(TArray<int32>& OutIndices, TArray<int32>& OutDeltaCm) const
{
	GetField().EncodeDelta(OutIndices, OutDeltaCm);
}

bool UGLTerrainSubsystem::RestoreCellDelta(FName CellId, TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm)
{
	FGLCellGround* Ground = Grounds.Find(CellId);
	if (!Ground)
	{
		return false;
	}
	FGLHeightfield Fresh = Ground->Field;
	Fresh.ResetToBase();
	if (!Fresh.ApplyDelta(Indices, DeltaCm))
	{
		return false;
	}
	// Rebuild only the chunks whose heights actually changed (P2: a full rebuild cost 2.8 s at 1 km).
	const FGLHeightfield Old = MoveTemp(Ground->Field);
	Ground->Field = MoveTemp(Fresh);
	for (FGLChunkSlot& Slot : Ground->Slots)
	{
		bool bChanged = false;
		for (int32 Y = Slot.First.Y; Y < Slot.First.Y + Ground->VertsPerChunk && !bChanged; ++Y)
		{
			for (int32 X = Slot.First.X; X < Slot.First.X + Ground->VertsPerChunk && !bChanged; ++X)
			{
				bChanged = Old.VertexHeight(X, Y) != Ground->Field.VertexHeight(X, Y);
			}
		}
		if (bChanged)
		{
			++Slot.Version;
			if (Slot.Actor && Slot.Actor->IsBuilt())
			{
				Slot.Actor->Rebuild(Ground->Field, true);
				Slot.BuiltVersion = Slot.Version;
			}
		}
	}
	return true;
}

bool UGLTerrainSubsystem::RestoreDelta(TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm)
{
	if (Grounds.Contains(NAME_None))
	{
		return RestoreCellDelta(NAME_None, Indices, DeltaCm);
	}
	return Grounds.Num() > 0 && RestoreCellDelta(Grounds.CreateConstIterator()->Key, Indices, DeltaCm);
}
