#include "World/GLGridSubsystem.h"

#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GridlandsGame.h"
#include "Kismet/GameplayStatics.h"
#include "Save/GLSaveSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLGridBoundary.h"
#include "World/GLGridCells.h"
#include "World/GLPlacementSubsystem.h"

double UGLGridSubsystem::Now() const
{
	return FPlatformTime::Seconds();
}

void UGLGridSubsystem::Tick(float DeltaTime)
{
	if (const APawn* Zenny = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		Advance(Zenny->GetActorLocation());
	}
}

void UGLGridSubsystem::Advance(const FVector& Where, double BudgetSeconds)
{
	Update(Where);
	UGLTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UGLTerrainSubsystem>();
	// Safety first: the ground under Zenny exists, with collision, whatever else is in flight.
	Terrain->EnsureReadyAt(FVector2D(Where));
	Terrain->Pump(FVector2D(Where), BudgetSeconds);
	for (TPair<FName, FGLLoadedCell>& Entry : Loaded)
	{
		FGLLoadedCell& Cell = Entry.Value;
		if (Cell.GroundAt < 0.0 && Terrain->HasCell(Entry.Key))
		{
			Cell.GroundAt = Now();
		}
		if (!Cell.bRuntime)
		{
			TryFinishRuntime(Entry.Key, Cell);
		}
		if (Cell.bRuntime && Cell.CompleteAt < 0.0 && Terrain->IsCellComplete(Entry.Key))
		{
			Cell.CompleteAt = Now();
			FGLCellLoadRecord& R = Records.AddDefaulted_GetRef();
			R.Cell = Entry.Key;
			R.Epoch = Cell.Epoch;
			R.GroundSeconds = Cell.GroundAt - Cell.StartedAt;
			R.RuntimeSeconds = Cell.RuntimeAt - Cell.StartedAt;
			R.CompleteSeconds = Cell.CompleteAt - Cell.StartedAt;
			UE_LOG(LogGridlands, Log, TEXT("Grid: %s (epoch %d) ready: ground %.0f ms, runtime %.0f ms, every chunk %.0f ms"),
				*Entry.Key.ToString(), Cell.Epoch, R.GroundSeconds * 1000.0, R.RuntimeSeconds * 1000.0, R.CompleteSeconds * 1000.0);
		}
	}
}

void UGLGridSubsystem::Update(const FVector& Where)
{
	const FVector2D P(Where);
	const FName Now = GLGridCells::CellAt(P);
	if (Now != Current)
	{
		UE_LOG(LogGridlands, Log, TEXT("Grid: entered %s (from %s)"), *Now.ToString(), *Current.ToString());
		Current = Now;
	}
	if (!Current.IsNone() && !IsLoaded(Current))
	{
		LoadCell(Current);
	}
	for (const FName& Cell : GLGridCells::AllCells())
	{
		const FGLCellDef* Def = GLContent::Get().Find<FGLCellDef>(Cell);
		const double Distance = GLGridCells::DistanceToCell(*Def, P);
		if (!IsLoaded(Cell) && Distance <= LoadMarginM * 100.0)
		{
			LoadCell(Cell);
		}
		else if (IsLoaded(Cell) && Cell != Current && Distance > UnloadMarginM * 100.0)
		{
			UnloadCell(Cell);
		}
	}
}

bool UGLGridSubsystem::LevelReady(const FGLLoadedCell& Entry) const
{
	if (bHoldLevels)
	{
		return false;
	}
	return !bLoadLevels || !Entry.Level || Entry.Level->IsLevelVisible();
}

bool UGLGridSubsystem::LoadCell(FName Cell)
{
	const FGLCellDef* Def = GLContent::Get().Find<FGLCellDef>(Cell);
	UWorld* World = GetWorld();
	if (!Def || IsLoaded(Cell))
	{
		return false;
	}
	FGLLoadedCell& Entry = Loaded.Add(Cell);
	Entry.Epoch = ++EpochCounter;
	Entry.StartedAt = Now();
	// 1. The authored level, asynchronously, at the cell's world offset. One streaming level per
	// cell for the session: coming back re-requests the same one (a same-named new instance fails
	// while the old one is still unloading).
	if (bLoadLevels && !Def->Level.IsEmpty())
	{
		TObjectPtr<ULevelStreamingDynamic>& Level = CellLevels.FindOrAdd(Cell);
		if (!Level)
		{
			bool bOk = false;
			Level = ULevelStreamingDynamic::LoadLevelInstance(World, Def->Level, FVector(Def->CentreCm(), 0.0), FRotator::ZeroRotator, bOk,
				FString::Printf(TEXT("GridCell_%s"), *Cell.ToString().Replace(TEXT("."), TEXT("_"))));
		}
		if (Level)
		{
			Level->SetShouldBeLoaded(true);
			Level->SetShouldBeVisible(true);
		}
		Entry.Level = Level;
	}
	// 2. Its ground, on a worker, with its saved edits (the rest of its kept state waits for the runtime layer).
	FGLSavedCell Kept;
	if (const FGLSavedCell* Dormant = World->GetSubsystem<UGLSaveSubsystem>()->PeekDormant(Cell))
	{
		Kept = *Dormant;
	}
	World->GetSubsystem<UGLTerrainSubsystem>()->BeginCellGround(Cell, Kept.TerrainIndices, Kept.TerrainDeltaCm);
	++Loads;
	UE_LOG(LogGridlands, Log, TEXT("Grid: loading %s (epoch %d)"), *Cell.ToString(), Entry.Epoch);
	return true;
}

void UGLGridSubsystem::TryFinishRuntime(FName Cell, FGLLoadedCell& Entry)
{
	UWorld* World = GetWorld();
	UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
	if (!LevelReady(Entry) || !Terrain->HasCell(Cell))
	{
		return;
	}
	UGLSaveSubsystem* Saves = World->GetSubsystem<UGLSaveSubsystem>();
	UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	FGLSavedCell Record;
	Record.Cell = Cell;
	const bool bHadState = Saves->TakeDormant(Cell, Record);
	// Anything done to this cell while it was loading is newer than its kept record: its ground as
	// it is now, and any pieces already placed on it.
	Terrain->CaptureCellDelta(Cell, Record.TerrainIndices, Record.TerrainDeltaCm);
	for (const FGLPlacedPiece& Piece : Building->PiecesOfCell(Cell))
	{
		if (!Record.BuildPieces.ContainsByPredicate([&Piece](const FGLSavedPiece& S) { return S.Id == Piece.Id; }))
		{
			Record.BuildPieces.Add({ Piece.Id, Piece.Def, Piece.Location, Piece.YawQuarter });
		}
	}
	World->GetSubsystem<UGLPlacementSubsystem>()->SpawnCell(Cell);
	Saves->ApplyCell(Record);
	if (bShowBoundaries)
	{
		const FGLCellDef* Def = GLContent::Get().Find<FGLCellDef>(Cell);
		Entry.Boundary = World->SpawnActor<AGLGridBoundary>();
		if (Entry.Boundary && Def)
		{
			Entry.Boundary->Setup(Def->CentreCm(), Def->SizeMetres);
		}
	}
	Entry.bRuntime = true;
	Entry.RuntimeAt = Now();
	UE_LOG(LogGridlands, Log, TEXT("Grid: %s runtime layer in (epoch %d, kept state: %s)"), *Cell.ToString(), Entry.Epoch, bHadState ? TEXT("yes") : TEXT("no"));
}

bool UGLGridSubsystem::UnloadCell(FName Cell)
{
	FGLLoadedCell Entry;
	if (!Loaded.RemoveAndCopyValue(Cell, Entry))
	{
		return false;
	}
	UWorld* World = GetWorld();
	UGLSaveSubsystem* Saves = World->GetSubsystem<UGLSaveSubsystem>();
	// Keep what this cell is, then take it out of the world. A cell whose runtime never came in
	// still has its full kept record: only its ground (which may have been edited) is merged.
	if (Entry.bRuntime)
	{
		Saves->StowCell(Cell);
	}
	else
	{
		Saves->StowTerrainOnly(Cell);
		FGLCellLoadRecord& R = Records.AddDefaulted_GetRef();
		R.Cell = Cell;
		R.Epoch = Entry.Epoch;
		R.bCancelled = true;
	}
	World->GetSubsystem<UGLBuildingSubsystem>()->RemoveCell(Cell);
	World->GetSubsystem<UGLPlacementSubsystem>()->DespawnCell(Cell);
	World->GetSubsystem<UGLTerrainSubsystem>()->RemoveCell(Cell);
	if (Entry.Boundary)
	{
		Entry.Boundary->Destroy();
	}
	if (Entry.Level)
	{
		Entry.Level->SetShouldBeVisible(false);
		Entry.Level->SetShouldBeLoaded(false);
	}
	++Unloads;
	UE_LOG(LogGridlands, Log, TEXT("Grid: unloaded %s (epoch %d, %s)"), *Cell.ToString(), Entry.Epoch, Entry.bRuntime ? TEXT("was ready") : TEXT("cancelled mid-load"));
	return true;
}

void UGLGridSubsystem::FlushAll()
{
	UWorld* World = GetWorld();
	if (bLoadLevels)
	{
		World->FlushLevelStreaming();
	}
	World->GetSubsystem<UGLTerrainSubsystem>()->FlushAll();
	for (TPair<FName, FGLLoadedCell>& Entry : Loaded)
	{
		if (!Entry.Value.bRuntime)
		{
			TryFinishRuntime(Entry.Key, Entry.Value);
		}
	}
}
