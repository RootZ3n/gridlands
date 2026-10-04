#include "Building/GLBuildPiece.h"

#include "Presentation/GLVisuals.h"

#include "Building/GLPiecePresentation.h"
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
}

AGLBuildPiece::AGLBuildPiece()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AGLBuildPiece::Setup(const FGLPlacedPiece& InPiece, bool bGhost)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, ShapeMaterialPath);
	FGLPieceLook Look;
	if (!Cube || !GLPiecePresentation::Describe(InPiece, Look, bGhost))
	{
		return false;
	}
	Piece = InPiece;
	bIsGhost = bGhost;
	for (UStaticMeshComponent* Box : Boxes)
	{
		Box->DestroyComponent();
	}
	for (UStaticMeshComponent* Box : Looks)
	{
		Box->DestroyComponent();
	}
	Boxes.Reset();
	Looks.Reset();
	GLVisuals::Detach(this);
	SetActorTransform(GLPiecePresentation::PieceTransform(Piece));
	// P11: what to show follows the fact (GLPiecePresentation, shared with the instanced batch of a player structure).
	// P7: an authored look replaces the blockout boxes; the boxes keep the authoritative collision. Decided first, so
	// hidden boxes are registered hidden and unpainted.
	Shown = Look.Shown;
	auto MakeBox = [&](const FGLPieceBox& Shape, bool bCollides) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
		Box->SetStaticMesh(Cube);
		Box->SetupAttachment(GetRootComponent());
		Box->SetRelativeTransform(Shape.Local);
		if (bCollides && !bGhost)
		{
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetCanEverAffectNavigation(true);
		}
		else
		{
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->SetCanEverAffectNavigation(false);
		}
		if (bGhost)
		{
			Box->SetCastShadow(false);
		}
		if (!Shape.bVisible)
		{
			Box->SetVisibility(false);
			Box->SetCastShadow(false);
		}
		Box->RegisterComponent();
		if (ShapeMaterial && Shape.bVisible)
		{
			if (UMaterialInstanceDynamic* Paint = Box->CreateDynamicMaterialInstance(0, ShapeMaterial))
			{
				Paint->SetVectorParameterValue(TEXT("Color"), Shape.Colour);
			}
		}
		return Box;
	};
	for (const FGLPieceBox& Shape : Look.Shapes)
	{
		Boxes.Add(MakeBox(Shape, true));
	}
	for (const FGLPieceBox& Shape : Look.Frame)
	{
		Looks.Add(MakeBox(Shape, false));
	}
	if (!Look.Visual.IsNone())
	{
		GLVisuals::Attach(this, GetRootComponent(), Look.Visual);
	}
	if (bHighlighted)
	{
		SetRemovalHighlight(true);
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
	SetGhostPreview(bValid ? EGLPreview::Green : EGLPreview::Red);
}

void AGLBuildPiece::SetGhostPreview(EGLPreview Preview)
{
	const FLinearColor Colour = Preview == EGLPreview::Green ? FLinearColor(0.2f, 0.9f, 0.3f)
		: Preview == EGLPreview::Yellow ? FLinearColor(0.98f, 0.82f, 0.1f) : FLinearColor(0.95f, 0.15f, 0.1f);
	for (UStaticMeshComponent* Box : Boxes)
	{
		if (UMaterialInstanceDynamic* Paint = Cast<UMaterialInstanceDynamic>(Box->GetMaterial(0)))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), Colour);
		}
	}
}

void AGLBuildPiece::SetRemovalHighlight(bool bOn)
{
	bHighlighted = bOn;
	UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, ShapeMaterialPath);
	for (UStaticMeshComponent* Box : Boxes)
	{
		// The collision envelope, shown red over whatever look the piece has while it is predicted to fall.
		Box->SetVisibility(bOn || (Shown == FName(TEXT("complete")) && !GLVisuals::HasLook(this)) || (Shown == FName(TEXT("finish")) && !GLVisuals::HasLook(this)));
		if (bOn && ShapeMaterial)
		{
			UMaterialInstanceDynamic* Paint = Cast<UMaterialInstanceDynamic>(Box->GetMaterial(0));
			Paint = Paint ? Paint : Box->CreateDynamicMaterialInstance(0, ShapeMaterial);
			if (Paint)
			{
				Paint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.1f, 0.08f));
			}
		}
	}
	if (!bOn)
	{
		Setup(Piece, bIsGhost); // repaint from the fact
	}
}
