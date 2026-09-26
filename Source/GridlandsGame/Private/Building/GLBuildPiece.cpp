#include "Building/GLBuildPiece.h"

#include "Presentation/GLVisuals.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* ShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	/** Blockout colour by material family (look only; ERA-1). */
	FLinearColor ColourFor(const FGLBuildPieceDef& Def)
	{
		const FGLMaterialDef* Material = GLContent::Get().Find<FGLMaterialDef>(Def.Material);
		if (Material && Material->Tags.Contains(FName(TEXT("Material.Masonry"))))
		{
			return FLinearColor(0.55f, 0.53f, 0.5f);
		}
		return FLinearColor(0.45f, 0.3f, 0.16f); // timber
	}

	double Axis(const TArray<double>& V, int32 I) { return V.IsValidIndex(I) ? V[I] : 0.0; }
}

AGLBuildPiece::AGLBuildPiece()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AGLBuildPiece::Setup(const FGLPlacedPiece& InPiece, bool bGhost)
{
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(InPiece.Def);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, ShapeMaterialPath);
	if (!Def || !Cube)
	{
		return false;
	}
	Piece = InPiece;
	bIsGhost = bGhost;
	for (UStaticMeshComponent* Box : Boxes)
	{
		Box->DestroyComponent();
	}
	Boxes.Reset();
	SetActorLocationAndRotation(Piece.Location, FRotator(0.0, 90.0 * Piece.YawQuarter, 0.0));
	// P7: an authored look replaces the blockout boxes; the boxes keep the authoritative collision.
	// Decided first, so hidden boxes are registered hidden and unpainted (no render state built and
	// thrown away inside a streaming frame).
	const FGLVisualDef* Look = bGhost || Def->Visual.IsNone() ? nullptr : GLContent::Get().Find<FGLVisualDef>(Def->Visual);
	const bool bLooked = Look && GLVisuals::LoadMesh(Look->Mesh);
	for (const FGLBuildShapeDef& Shape : Def->Shapes)
	{
		UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
		Box->SetStaticMesh(Cube);
		Box->SetupAttachment(GetRootComponent());
		// The engine cube is 100 cm on a side, centred on its origin.
		Box->SetRelativeScale3D(FVector(Axis(Shape.Size, 0), Axis(Shape.Size, 1), Axis(Shape.Size, 2)));
		const FQuat Tilt(FVector::XAxisVector, FMath::DegreesToRadians(Shape.Pitch)); // positive lifts +Y
		Box->SetRelativeLocationAndRotation(FVector(Axis(Shape.Offset, 0), Axis(Shape.Offset, 1), Axis(Shape.Offset, 2)) * 100.0, Tilt);
		if (bGhost)
		{
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->SetCanEverAffectNavigation(false);
			Box->SetCastShadow(false);
		}
		else
		{
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetCanEverAffectNavigation(true);
		}
		if (bLooked)
		{
			Box->SetVisibility(false);
			Box->SetCastShadow(false);
		}
		Box->RegisterComponent();
		if (ShapeMaterial && !bLooked)
		{
			if (UMaterialInstanceDynamic* Paint = Box->CreateDynamicMaterialInstance(0, ShapeMaterial))
			{
				Paint->SetVectorParameterValue(TEXT("Color"), ColourFor(*Def));
			}
		}
		Boxes.Add(Box);
	}
	if (bLooked)
	{
		GLVisuals::Attach(this, GetRootComponent(), Def->Visual);
	}
	return true;
}

void AGLBuildPiece::SetSolid(bool bSolid)
{
	for (UStaticMeshComponent* Box : Boxes)
	{
		Box->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Box->SetCanEverAffectNavigation(bSolid);
	}
}

void AGLBuildPiece::SetGhostValid(bool bValid)
{
	const FLinearColor Colour = bValid ? FLinearColor(0.2f, 0.9f, 0.3f) : FLinearColor(0.95f, 0.15f, 0.1f);
	for (UStaticMeshComponent* Box : Boxes)
	{
		if (UMaterialInstanceDynamic* Paint = Cast<UMaterialInstanceDynamic>(Box->GetMaterial(0)))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), Colour);
		}
	}
}
