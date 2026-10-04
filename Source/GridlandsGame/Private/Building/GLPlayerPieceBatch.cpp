#include "Building/GLPlayerPieceBatch.h"

#include "Building/GLPiecePresentation.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Presentation/GLVisuals.h"

namespace
{
	const TCHAR* BatchCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* BatchShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FLinearColor HighlightColour(0.95f, 0.1f, 0.08f);
}

AGLPlayerPieceBatch::AGLPlayerPieceBatch()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AGLPlayerPieceBatch::CanInstance(const FGLPlacedPiece& Piece)
{
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);
	FGLPieceLook Look;
	if (!Def || Def->Storage.Slots > 0 || !GLPiecePresentation::Describe(Piece, Look))
	{
		return false;
	}
	const FGLVisualDef* Visual = Look.Visual.IsNone() ? nullptr : GLContent::Get().Find<FGLVisualDef>(Look.Visual);
	return !Visual || (Visual->Corruption.Num() == 0 && Visual->Light.Intensity <= 0.0);
}

UInstancedStaticMeshComponent* AGLPlayerPieceBatch::NewSet(UStaticMesh* Mesh, bool bCollides)
{
	UInstancedStaticMeshComponent* Set = NewObject<UInstancedStaticMeshComponent>(this);
	Set->SetStaticMesh(Mesh);
	Set->SetupAttachment(GetRootComponent());
	Set->bSupportRemoveAtSwap = false; // instances keep their order on removal: the owner tables mirror it exactly
	if (bCollides)
	{
		Set->SetCollisionProfileName(TEXT("BlockAll"));
		Set->SetCanEverAffectNavigation(true);
	}
	else
	{
		Set->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Set->SetCanEverAffectNavigation(false);
		Set->SetGenerateOverlapEvents(false);
	}
	Set->RegisterComponent();
	Owners.Add(Set);
	return Set;
}

UInstancedStaticMeshComponent* AGLPlayerPieceBatch::BoxSet(const FLinearColor& Colour)
{
	const FString Key = FString::Printf(TEXT("box:%.3f,%.3f,%.3f"), Colour.R, Colour.G, Colour.B);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Sets.Find(Key))
	{
		return *Found;
	}
	UInstancedStaticMeshComponent* Set = NewSet(LoadObject<UStaticMesh>(nullptr, BatchCubePath), false);
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, BatchShapeMaterialPath))
	{
		if (UMaterialInstanceDynamic* Paint = Set->CreateDynamicMaterialInstance(0, Shape))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), Colour);
		}
	}
	Sets.Add(Key, Set);
	return Set;
}

UInstancedStaticMeshComponent* AGLPlayerPieceBatch::VisualSet(FName Visual)
{
	const FString Key = TEXT("visual:") + Visual.ToString();
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Sets.Find(Key))
	{
		return *Found;
	}
	const FGLVisualDef* Def = GLContent::Get().Find<FGLVisualDef>(Visual);
	UStaticMesh* Mesh = Def ? GLVisuals::LoadMesh(Def->Mesh) : nullptr;
	if (!Mesh)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* Set = NewSet(Mesh, false);
	// The same presentation GLVisuals::Attach gives one mesh, once per set.
	Set->SetCastShadow(Def->CastShadow);
	if (Def->CullDistance > 0.0)
	{
		Set->SetCullDistances(Def->CullDistance * 100.0, Def->CullDistance * 100.0); // per instance, not per batch
	}
	if (Def->Tint.Num() >= 3 && !(Def->Tint[0] == 1.0 && Def->Tint[1] == 1.0 && Def->Tint[2] == 1.0))
	{
		for (int32 Slot = 0; Slot < Set->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Variant = Set->CreateDynamicMaterialInstance(Slot))
			{
				Variant->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Def->Tint[0], Def->Tint[1], Def->Tint[2]));
			}
		}
	}
	GLVisuals::SetOutlined(Set, Def->Outline);
	Sets.Add(Key, Set);
	return Set;
}

void AGLPlayerPieceBatch::AddInstance(UInstancedStaticMeshComponent* Set, const FTransform& World, int32 PieceId)
{
	if (Set)
	{
		Set->AddInstance(World, /*bWorldSpace=*/true);
		Owners.FindOrAdd(Set).Add(PieceId);
	}
}

bool AGLPlayerPieceBatch::Add(const FGLPlacedPiece& Piece)
{
	FGLPieceLook Look;
	if (Shown.Contains(Piece.Id) || !CanInstance(Piece) || !GLPiecePresentation::Describe(Piece, Look))
	{
		return false;
	}
	if (!Collision)
	{
		Collision = NewSet(LoadObject<UStaticMesh>(nullptr, BatchCubePath), true);
		Collision->SetVisibility(false); // the envelope collides and navigation reads it; what is seen is the look
		Collision->SetCastShadow(false);
	}
	const FTransform At = GLPiecePresentation::PieceTransform(Piece);
	for (const FGLPieceBox& Shape : Look.Shapes)
	{
		AddInstance(Collision, Shape.Local * At, Piece.Id);
		if (Shape.bVisible)
		{
			AddInstance(BoxSet(Shape.Colour), Shape.Local * At, Piece.Id);
		}
	}
	for (const FGLPieceBox& Box : Look.Frame)
	{
		AddInstance(BoxSet(Box.Colour), Box.Local * At, Piece.Id);
	}
	if (!Look.Visual.IsNone())
	{
		AddInstance(VisualSet(Look.Visual), GLPiecePresentation::VisualTransform(Look.Visual) * At, Piece.Id);
	}
	Shown.Add(Piece.Id, Look.Shown);
	return true;
}

bool AGLPlayerPieceBatch::Remove(int32 PieceId)
{
	if (!Shown.Remove(PieceId))
	{
		return false;
	}
	for (TPair<const UInstancedStaticMeshComponent*, TArray<int32>>& Entry : Owners)
	{
		TArray<int32> Indices; // descending, so each removal leaves the earlier indices (and their owners) in place
		for (int32 I = Entry.Value.Num() - 1; I >= 0; --I)
		{
			if (Entry.Value[I] == PieceId)
			{
				Indices.Add(I);
			}
		}
		if (Indices.Num() == 0)
		{
			continue;
		}
		const_cast<UInstancedStaticMeshComponent*>(Entry.Key)->RemoveInstances(Indices, /*bInstanceArrayAlreadySortedInReverseOrder=*/true);
		for (const int32 I : Indices)
		{
			Entry.Value.RemoveAt(I);
		}
	}
	if (Highlighted.Remove(PieceId) > 0)
	{
		SetHighlighted(TArray<int32>(Highlighted));
	}
	return true;
}

FName AGLPlayerPieceBatch::ShownPhaseOf(int32 PieceId) const
{
	return Shown.FindRef(PieceId);
}

int32 AGLPlayerPieceBatch::PieceIdAt(const UPrimitiveComponent* Component, int32 Item) const
{
	if (bRetired)
	{
		return 0; // its cell unloaded: nothing here is a piece any more
	}
	const TArray<int32>* Table = Owners.Find(Cast<UInstancedStaticMeshComponent>(Component));
	return Table && Table->IsValidIndex(Item) ? (*Table)[Item] : 0;
}

void AGLPlayerPieceBatch::SetHighlighted(const TArray<int32>& PieceIds)
{
	if (!Highlight)
	{
		Highlight = NewSet(LoadObject<UStaticMesh>(nullptr, BatchCubePath), false);
		Highlight->SetCastShadow(false);
		if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, BatchShapeMaterialPath))
		{
			if (UMaterialInstanceDynamic* Paint = Highlight->CreateDynamicMaterialInstance(0, Shape))
			{
				Paint->SetVectorParameterValue(TEXT("Color"), HighlightColour);
			}
		}
	}
	Highlight->ClearInstances();
	TArray<int32>& Table = Owners.FindOrAdd(Highlight);
	Table.Reset();
	Highlighted.Reset();
	for (const int32 Id : PieceIds)
	{
		if (!Shown.Contains(Id))
		{
			continue;
		}
		Highlighted.Add(Id);
		// The envelope, a hair larger than the piece so it shows over whatever look the piece has.
		for (const FTransform& Box : CollisionOf(Id))
		{
			FTransform Grown = Box;
			Grown.SetScale3D(Box.GetScale3D() * 1.02);
			AddInstance(Highlight, Grown, Id);
		}
	}
}

TArray<FTransform> AGLPlayerPieceBatch::CollisionOf(int32 PieceId) const
{
	TArray<FTransform> Out;
	const TArray<int32>* Table = Collision ? Owners.Find(Collision) : nullptr;
	for (int32 I = 0; Table && I < Table->Num(); ++I)
	{
		FTransform T;
		if ((*Table)[I] == PieceId && Collision->GetInstanceTransform(I, T, /*bWorldSpace=*/true))
		{
			Out.Add(T);
		}
	}
	return Out;
}

TArray<FTransform> AGLPlayerPieceBatch::VisibleOf(int32 PieceId) const
{
	TArray<FTransform> Out;
	for (const TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& Set : Sets)
	{
		const TArray<int32>* Table = Owners.Find(Set.Value);
		for (int32 I = 0; Table && I < Table->Num(); ++I)
		{
			FTransform T;
			if ((*Table)[I] == PieceId && Set.Value->GetInstanceTransform(I, T, /*bWorldSpace=*/true))
			{
				Out.Add(T);
			}
		}
	}
	return Out;
}

int32 AGLPlayerPieceBatch::NumInstances() const
{
	int32 N = 0;
	for (const TPair<const UInstancedStaticMeshComponent*, TArray<int32>>& Entry : Owners)
	{
		N += Entry.Value.Num();
	}
	return N;
}

int32 AGLPlayerPieceBatch::NumComponents() const
{
	return Owners.Num();
}

void AGLPlayerPieceBatch::Retire()
{
	// Hidden only: turning collision off here would tear down every instance body in the unload frame (linear in the
	// pieces; measured 0.7-1.0 ms for 309). RetireStep spreads that over the next frames.
	SetActorHiddenInGame(true);
	bRetired = true;
	Shown.Reset();
	Highlighted.Reset();
}

bool AGLPlayerPieceBatch::RetireStep(int32 MaxBodies)
{
	const int32 Left = Collision ? Collision->GetInstanceCount() : 0;
	const int32 Count = FMath::Min(Left, FMath::Max(1, MaxBodies));
	if (Count > 0)
	{
		TArray<int32> Last; // from the end: no instance moves, so nothing is re-indexed
		for (int32 I = Left - 1; I >= Left - Count; --I)
		{
			Last.Add(I);
		}
		Collision->RemoveInstances(Last, /*bInstanceArrayAlreadySortedInReverseOrder=*/true);
		if (TArray<int32>* Table = Owners.Find(Collision))
		{
			Table->SetNum(Left - Count);
		}
	}
	return Left - Count <= 0;
}
