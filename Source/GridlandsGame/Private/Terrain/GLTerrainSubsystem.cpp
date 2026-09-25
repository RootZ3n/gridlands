#include "Terrain/GLTerrainSubsystem.h"

#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "GridlandsGame.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Terrain/GLCellNavBounds.h"
#include "Terrain/GLTerrainChunk.h"

namespace
{
	// Vertical room for navigation above and below the base: the edit limits plus headroom for pieces.
	constexpr double NavHeadroomCm = 1500.0;
}

void UGLTerrainSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The cell whose definition names this world's map gets its ground (as placements do).
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

bool UGLTerrainSubsystem::SetupCell(FName CellId)
{
	const FGLCellDef* Cell = GLContent::Get().Find<FGLCellDef>(CellId);
	if (!Cell || !Cell->HasTerrain())
	{
		return false;
	}
	const FGLCellTerrainDef& T = Cell->Terrain;
	const double HalfExtentM = FMath::Max(Cell->PlayableHalfExtent, static_cast<double>(T.ChunkMetres) * 0.5);
	const int32 ChunksPerSide = 2 * FMath::CeilToInt(HalfExtentM / T.ChunkMetres);
	const int32 ChunkVerts = FMath::RoundToInt(T.ChunkMetres / T.SpacingMetres) + 1;
	const double Half = ChunksPerSide * T.ChunkMetres * 50.0; // cm
	Setup(FVector2D(-Half, -Half), ChunksPerSide, ChunksPerSide, ChunkVerts, T.SpacingMetres * 100.0, T.BaseHeight * 100.0,
		T.MaxDigDepth * 100.0, T.MaxRaiseHeight * 100.0);
	UE_LOG(LogGridlands, Log, TEXT("Terrain: %s ground %dx%d chunks of %d m"), *CellId.ToString(), ChunksPerSide, ChunksPerSide, T.ChunkMetres);
	return true;
}

void UGLTerrainSubsystem::Setup(const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm, float BaseHeightCm,
	double MaxDigCm, double MaxRaiseCm)
{
	Clear();
	UWorld* World = GetWorld();
	VertsPerChunk = ChunkVerts;
	// Neighbouring chunks share their edge vertices, so seams can never open.
	Field.Init(Origin, ChunksX * (ChunkVerts - 1) + 1, ChunksY * (ChunkVerts - 1) + 1, SpacingCm, BaseHeightCm, MaxDigCm, MaxRaiseCm);
	for (int32 CY = 0; CY < ChunksY; ++CY)
	{
		for (int32 CX = 0; CX < ChunksX; ++CX)
		{
			const FIntPoint First(CX * (ChunkVerts - 1), CY * (ChunkVerts - 1));
			const FVector2D At = Field.VertexLocation(First.X, First.Y);
			AGLTerrainChunk* Chunk = World->SpawnActor<AGLTerrainChunk>(FVector(At.X, At.Y, 0.0), FRotator::ZeroRotator);
			Chunk->Setup(First, ChunkVerts);
			Chunk->Rebuild(Field, true);
			Chunks.Add(Chunk);
		}
	}
	// The cell declares its navigable volume; navigation builds inside it at runtime.
	const FVector2D Size((Field.GetVertsX() - 1) * SpacingCm, (Field.GetVertsY() - 1) * SpacingCm);
	const FVector Centre(Origin.X + Size.X * 0.5, Origin.Y + Size.Y * 0.5, BaseHeightCm);
	NavBounds = World->SpawnActor<AGLCellNavBounds>(Centre, FRotator::ZeroRotator);
	if (NavBounds)
	{
		// Components register during spawn, so the volume first reports its default box; resize,
		// then tell navigation the bounds changed.
		NavBounds->SetExtent(FVector(Size.X * 0.5, Size.Y * 0.5, FMath::Max(MaxDigCm, MaxRaiseCm) + NavHeadroomCm));
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			Nav->OnNavigationBoundsUpdated(NavBounds);
		}
	}
}

void UGLTerrainSubsystem::Clear()
{
	for (AGLTerrainChunk* Chunk : Chunks)
	{
		if (Chunk)
		{
			Chunk->Destroy();
		}
	}
	Chunks.Reset();
	if (NavBounds)
	{
		NavBounds->Destroy();
		NavBounds = nullptr;
	}
}

FGLTerrainEditResult UGLTerrainSubsystem::ApplyEdit(const FGLTerrainEdit& Edit, TFunctionRef<bool(const FVector2D&)> IsProtected)
{
	if (!HasGround())
	{
		FGLTerrainEditResult None;
		None.Refusal = TEXT("no ground here");
		return None;
	}
	const FGLTerrainEditResult Result = Field.Apply(Edit, IsProtected);
	if (Result.bApplied)
	{
		for (AGLTerrainChunk* Chunk : Chunks)
		{
			if (Chunk && Chunk->Covers(Result.DirtyVertices))
			{
				Chunk->Rebuild(Field, bNotifyNavigation);
			}
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
	const FGLTerrainEditResult Result = ApplyEdit(Edit, [Building](const FVector2D& At) { return Building && Building->IsUnderStructure(At); });
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

bool UGLTerrainSubsystem::RestoreDelta(TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm)
{
	if (!HasGround())
	{
		return false;
	}
	FGLHeightfield Fresh = Field;
	Fresh.ResetToBase();
	if (!Fresh.ApplyDelta(Indices, DeltaCm))
	{
		return false;
	}
	Field = MoveTemp(Fresh);
	for (AGLTerrainChunk* Chunk : Chunks)
	{
		Chunk->Rebuild(Field, true);
	}
	return true;
}
