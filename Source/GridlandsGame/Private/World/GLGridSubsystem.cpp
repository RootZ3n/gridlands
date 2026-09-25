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

void UGLGridSubsystem::Tick(float DeltaTime)
{
	SinceUpdate += DeltaTime;
	if (SinceUpdate < 0.25)
	{
		return;
	}
	SinceUpdate = 0.0;
	if (const APawn* Zenny = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		Update(Zenny->GetActorLocation());
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
	// The cell Zenny stands in first, so there is always ground under him.
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
		else if (IsLoaded(Cell) && Distance > UnloadMarginM * 100.0)
		{
			UnloadCell(Cell);
		}
	}
}

bool UGLGridSubsystem::LoadCell(FName Cell)
{
	const FGLCellDef* Def = GLContent::Get().Find<FGLCellDef>(Cell);
	UWorld* World = GetWorld();
	if (!Def || IsLoaded(Cell))
	{
		return false;
	}
	const double Start = FPlatformTime::Seconds();
	FGLLoadedCell& Entry = Loaded.Add(Cell);
	// 1. The authored level, at the cell's world offset (its actors use cell-local coordinates).
	if (bLoadLevels && !Def->Level.IsEmpty())
	{
		// One streaming level per cell for the whole session: coming back re-requests the same one.
		// (A fresh instance with the same name while the old one is still unloading fails.)
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
		// Synchronous for now so anchored placements find their map actors (a streaming hitch; see ADR-0026).
		World->FlushLevelStreaming();
		if (!Level || !Level->IsLevelLoaded())
		{
			UE_LOG(LogGridlands, Warning, TEXT("Grid: %s level %s did not load"), *Cell.ToString(), *Def->Level);
		}
		Entry.Level = Level;
	}
	// 2. Its runtime layer, with the state it had when it was stowed (or saved).
	UGLSaveSubsystem* Saves = World->GetSubsystem<UGLSaveSubsystem>();
	FGLSavedCell Record;
	Record.Cell = Cell;
	const bool bHadState = Saves && Saves->TakeDormant(Cell, Record);
	World->GetSubsystem<UGLTerrainSubsystem>()->SetupCell(Cell, Record.TerrainIndices, Record.TerrainDeltaCm);
	World->GetSubsystem<UGLPlacementSubsystem>()->SpawnCell(Cell);
	if (Saves && bHadState)
	{
		Saves->ApplyCell(Record);
	}
	if (bShowBoundaries)
	{
		Entry.Boundary = World->SpawnActor<AGLGridBoundary>();
		if (Entry.Boundary)
		{
			Entry.Boundary->Setup(Def->CentreCm(), Def->SizeMetres);
		}
	}
	++Loads;
	UE_LOG(LogGridlands, Log, TEXT("Grid: loaded %s in %.0f ms (kept state: %s)"), *Cell.ToString(), (FPlatformTime::Seconds() - Start) * 1000.0, bHadState ? TEXT("yes") : TEXT("no"));
	return true;
}

bool UGLGridSubsystem::UnloadCell(FName Cell)
{
	FGLLoadedCell Entry;
	if (!Loaded.RemoveAndCopyValue(Cell, Entry))
	{
		return false;
	}
	UWorld* World = GetWorld();
	// Keep everything about it first, then take it out of the world.
	if (UGLSaveSubsystem* Saves = World->GetSubsystem<UGLSaveSubsystem>())
	{
		Saves->StowCell(Cell);
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
	UE_LOG(LogGridlands, Log, TEXT("Grid: unloaded %s"), *Cell.ToString());
	return true;
}
