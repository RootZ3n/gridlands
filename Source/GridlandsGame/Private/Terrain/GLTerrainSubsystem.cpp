#include "Terrain/GLTerrainSubsystem.h"

#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Terrain/GLCellNavBounds.h"
#include "Terrain/GLTerrainChunk.h"
#include "World/GLGridCells.h"

namespace
{
	// Vertical room for navigation above and below the base: the edit limits plus headroom for pieces.
	constexpr double NavHeadroomCm = 1500.0;
}

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

bool UGLTerrainSubsystem::SetupCell(FName CellId, TConstArrayView<int32> DeltaIndices, TConstArrayView<int32> DeltaCm)
{
	const FGLCellDef* Cell = GLContent::Get().Find<FGLCellDef>(CellId);
	if (!Cell || !Cell->HasTerrain())
	{
		return false;
	}
	const FGLCellTerrainDef& T = Cell->Terrain;
	// The ground covers the whole cell (GRID-3), or its playable extent for a cell without a pitch.
	const double SizeM = Cell->SizeMetres > 0.0 ? Cell->SizeMetres : 2.0 * FMath::Max(Cell->PlayableHalfExtent, T.ChunkMetres * 0.5);
	const int32 ChunksPerSide = FMath::Max(1, FMath::CeilToInt(SizeM / T.ChunkMetres));
	const int32 ChunkVerts = FMath::RoundToInt(T.ChunkMetres / T.SpacingMetres) + 1;
	const double Half = ChunksPerSide * T.ChunkMetres * 50.0; // cm
	const FVector2D Origin = Cell->CentreCm() - FVector2D(Half, Half);
	TArray<float> Relief;
	const int32 Verts = ChunksPerSide * (ChunkVerts - 1) + 1;
	if (T.Relief.AmplitudeMetres > 0.0)
	{
		Relief = GLTerrainGen::Rolling(T.Relief.Seed, Verts, Verts, T.SpacingMetres * 100.0, T.Relief.AmplitudeMetres * 100.0);
		// Flat to the base height near every edge: neighbouring cells always meet without seams.
		const double Blend = T.Relief.EdgeBlendMetres / T.SpacingMetres;
		for (int32 Y = 0; Y < Verts; ++Y)
		{
			for (int32 X = 0; X < Verts; ++X)
			{
				const double Edge = FMath::Min(FMath::Min(X, Verts - 1 - X), FMath::Min(Y, Verts - 1 - Y));
				const double W = FMath::SmoothStep(0.0, 1.0, FMath::Clamp(Edge / Blend, 0.0, 1.0));
				float& H = Relief[Y * Verts + X];
				H = static_cast<float>(T.BaseHeight * 100.0 + W * H);
			}
		}
	}
	Build(CellId, Origin, ChunksPerSide, ChunksPerSide, ChunkVerts, T.SpacingMetres * 100.0, T.BaseHeight * 100.0,
		T.MaxDigDepth * 100.0, T.MaxRaiseHeight * 100.0, Relief.Num() ? &Relief : nullptr, DeltaIndices, DeltaCm);
	UE_LOG(LogGridlands, Log, TEXT("Terrain: %s ground %dx%d chunks of %d m (%d edited vertices)"), *CellId.ToString(), ChunksPerSide, ChunksPerSide,
		T.ChunkMetres, DeltaIndices.Num());
	return true;
}

void UGLTerrainSubsystem::Setup(const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm, float BaseHeightCm,
	double MaxDigCm, double MaxRaiseCm, TArray<float>* AuthoredBase)
{
	Build(NAME_None, Origin, ChunksX, ChunksY, ChunkVerts, SpacingCm, BaseHeightCm, MaxDigCm, MaxRaiseCm, AuthoredBase, {}, {});
}

void UGLTerrainSubsystem::Build(FName CellId, const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm,
	float BaseHeightCm, double MaxDigCm, double MaxRaiseCm, TArray<float>* AuthoredBase, TConstArrayView<int32> DeltaIndices, TConstArrayView<int32> DeltaCm)
{
	RemoveCell(CellId);
	UWorld* World = GetWorld();
	FGLCellGround& Ground = Grounds.Add(CellId);
	Ground.VertsPerChunk = ChunkVerts;
	// Neighbouring chunks share their edge vertices, so seams can never open.
	Ground.Field.Init(Origin, ChunksX * (ChunkVerts - 1) + 1, ChunksY * (ChunkVerts - 1) + 1, SpacingCm, BaseHeightCm, MaxDigCm, MaxRaiseCm);
	if (AuthoredBase && !Ground.Field.SetBase(MoveTemp(*AuthoredBase)))
	{
		UE_LOG(LogGridlands, Error, TEXT("Terrain: authored base has the wrong size; using flat ground"));
	}
	// Saved edits go in before the first build: each chunk is built once, already edited.
	if (DeltaIndices.Num() > 0 && !Ground.Field.ApplyDelta(DeltaIndices, DeltaCm))
	{
		UE_LOG(LogGridlands, Warning, TEXT("Terrain: saved edits for %s do not fit its ground; ground left as authored"), *CellId.ToString());
	}
	for (int32 CY = 0; CY < ChunksY; ++CY)
	{
		for (int32 CX = 0; CX < ChunksX; ++CX)
		{
			const FIntPoint First(CX * (ChunkVerts - 1), CY * (ChunkVerts - 1));
			const FVector2D At = Ground.Field.VertexLocation(First.X, First.Y);
			AGLTerrainChunk* Chunk = World->SpawnActor<AGLTerrainChunk>(FVector(At.X, At.Y, 0.0), FRotator::ZeroRotator);
			Chunk->Setup(First, ChunkVerts);
			Chunk->Rebuild(Ground.Field, true);
			Ground.Chunks.Add(Chunk);
		}
	}
	// The cell declares its navigable volume; navigation builds inside it at runtime.
	const FVector2D Size((Ground.Field.GetVertsX() - 1) * SpacingCm, (Ground.Field.GetVertsY() - 1) * SpacingCm);
	const FVector Centre(Origin.X + Size.X * 0.5, Origin.Y + Size.Y * 0.5, BaseHeightCm);
	Ground.NavBounds = World->SpawnActor<AGLCellNavBounds>(Centre, FRotator::ZeroRotator);
	if (Ground.NavBounds)
	{
		// Components register during spawn, so the volume first reports its default box; resize,
		// then tell navigation the bounds changed.
		Ground.NavBounds->SetExtent(FVector(Size.X * 0.5, Size.Y * 0.5, FMath::Max(MaxDigCm, MaxRaiseCm) + NavHeadroomCm + 2000.0));
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			Nav->OnNavigationBoundsUpdated(Ground.NavBounds);
		}
	}
}

bool UGLTerrainSubsystem::RemoveCell(FName CellId)
{
	FGLCellGround Ground;
	if (!Grounds.RemoveAndCopyValue(CellId, Ground))
	{
		return false;
	}
	for (AGLTerrainChunk* Chunk : Ground.Chunks)
	{
		if (Chunk)
		{
			Chunk->Destroy();
		}
	}
	if (Ground.NavBounds)
	{
		Ground.NavBounds->Destroy();
	}
	return true;
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
	// Plan on copies of every ground the brush touches; commit only if none refuses (atomic across cells).
	struct FPlan { FGLCellGround* Ground; FGLHeightfield Trial; FGLTerrainEditResult Result; };
	TArray<FPlan> Plans;
	const FBox2D Brush(Edit.Centre - FVector2D(Edit.RadiusCm), Edit.Centre + FVector2D(Edit.RadiusCm));
	for (TPair<FName, FGLCellGround>& Entry : Grounds)
	{
		FGLHeightfield& F = Entry.Value.Field;
		const FBox2D Box(F.GetOrigin(), F.GetOrigin() + FVector2D((F.GetVertsX() - 1) * F.GetSpacing(), (F.GetVertsY() - 1) * F.GetSpacing()));
		if (Box.Intersect(Brush))
		{
			Plans.Add({ &Entry.Value, F, {} });
		}
	}
	bool bAny = false;
	for (FPlan& Plan : Plans)
	{
		Plan.Result = Plan.Trial.Apply(Edit, IsProtected);
		if (!Plan.Result.bApplied && Plan.Result.Refusal != TEXT("nothing to change") && Plan.Result.Refusal != TEXT("outside this cell's ground"))
		{
			Result = Plan.Result;
			return Result;
		}
		bAny |= Plan.Result.bApplied;
	}
	if (!bAny)
	{
		Result.Refusal = Plans.Num() ? Plans[0].Result.Refusal : TEXT("outside the loaded ground");
		return Result;
	}
	for (FPlan& Plan : Plans)
	{
		if (!Plan.Result.bApplied)
		{
			continue;
		}
		Plan.Ground->Field = MoveTemp(Plan.Trial);
		for (AGLTerrainChunk* Chunk : Plan.Ground->Chunks)
		{
			if (Chunk && Chunk->Covers(Plan.Result.DirtyVertices))
			{
				Chunk->Rebuild(Plan.Ground->Field, bNotifyNavigation);
			}
		}
		if (!Result.bApplied)
		{
			Result = Plan.Result;
		}
		else
		{
			Result.VerticesChanged += Plan.Result.VerticesChanged;
			Result.VolumeM3 += Plan.Result.VolumeM3;
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
	const int32 VX = Ground->Field.GetVertsX();
	for (AGLTerrainChunk* Chunk : Ground->Chunks)
	{
		const FIntPoint First = Chunk->GetFirstVertex();
		bool bChanged = false;
		for (int32 Y = First.Y; Y < First.Y + Ground->VertsPerChunk && !bChanged; ++Y)
		{
			for (int32 X = First.X; X < First.X + Ground->VertsPerChunk && !bChanged; ++X)
			{
				bChanged = Old.VertexHeight(X, Y) != Ground->Field.VertexHeight(X, Y);
			}
		}
		if (bChanged)
		{
			Chunk->Rebuild(Ground->Field, true);
		}
	}
	(void)VX;
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
