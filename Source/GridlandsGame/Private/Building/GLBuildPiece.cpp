#include "Building/GLBuildPiece.h"

#include "Presentation/GLVisuals.h"

#include "Building/GLConstructionRules.h"
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

	/** A raw-framing colour: unfinished timber reads as framing at a glance. */
	const FLinearColor FrameColour(0.82f, 0.66f, 0.42f);

	double Axis(const TArray<double>& V, int32 I) { return V.IsValidIndex(I) ? V[I] : 0.0; }

	/** The last installed finish (the visible surface), or null. */
	const FGLFinishDef* VisibleFinish(const FGLPlacedPiece& Piece)
	{
		for (int32 I = Piece.Layers.Num() - 1; I >= 0; --I)
		{
			if (const FGLFinishDef* Finish = GLContent::Get().Find<FGLFinishDef>(Piece.Layers[I]))
			{
				return Finish;
			}
		}
		return nullptr;
	}
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
	for (UStaticMeshComponent* Look : Looks)
	{
		Look->DestroyComponent();
	}
	Boxes.Reset();
	Looks.Reset();
	GLVisuals::Detach(this);
	SetActorLocationAndRotation(Piece.Location, FRotator(0.0, GLStructureRules::YawDegrees(Piece.YawStep), 0.0));
	// P11: what to show follows the fact. A frame waiting for its finish shows its frame; a finished piece its finish;
	// a piece complete as built its own look. P7: an authored look replaces the blockout boxes; the boxes keep the
	// authoritative collision. Decided first, so hidden boxes are registered hidden and unpainted.
	const bool bFrame = !bGhost && GLConstructionRules::ShowsFrame(GLContent::Get(), Piece);
	const FGLFinishDef* Finish = bGhost ? nullptr : VisibleFinish(Piece);
	FName LookVisual = bFrame ? NAME_None : (Finish ? Finish->Visual : Def->Visual);
	const FGLVisualDef* Look = bGhost || LookVisual.IsNone() ? nullptr : GLContent::Get().Find<FGLVisualDef>(LookVisual);
	const bool bLooked = Look && GLVisuals::LoadMesh(Look->Mesh);
	const bool bTinted = !bFrame && !bLooked && Finish && Finish->Tint.Num() >= 3;
	Shown = bFrame ? FName(TEXT("frame")) : (Finish ? FName(TEXT("finish")) : FName(TEXT("complete")));
	const bool bHideBoxes = bLooked || bFrame;
	auto MakeBox = [&](const FGLBuildShapeDef& Shape, bool bCollides, bool bVisible, const FLinearColor& Colour) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
		Box->SetStaticMesh(Cube);
		Box->SetupAttachment(GetRootComponent());
		// The engine cube is 100 cm on a side, centred on its origin.
		Box->SetRelativeScale3D(FVector(Axis(Shape.Size, 0), Axis(Shape.Size, 1), Axis(Shape.Size, 2)));
		const FQuat Tilt(FVector::XAxisVector, FMath::DegreesToRadians(Shape.Pitch)); // positive lifts +Y
		Box->SetRelativeLocationAndRotation(FVector(Axis(Shape.Offset, 0), Axis(Shape.Offset, 1), Axis(Shape.Offset, 2)) * 100.0, Tilt);
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
		if (!bVisible)
		{
			Box->SetVisibility(false);
			Box->SetCastShadow(false);
		}
		Box->RegisterComponent();
		if (ShapeMaterial && bVisible)
		{
			if (UMaterialInstanceDynamic* Paint = Box->CreateDynamicMaterialInstance(0, ShapeMaterial))
			{
				Paint->SetVectorParameterValue(TEXT("Color"), Colour);
			}
		}
		return Box;
	};
	const FLinearColor BodyColour = bTinted ? FLinearColor(Finish->Tint[0], Finish->Tint[1], Finish->Tint[2]) : ColourFor(*Def);
	for (const FGLBuildShapeDef& Shape : Def->Shapes)
	{
		Boxes.Add(MakeBox(Shape, true, !bHideBoxes, BodyColour));
	}
	if (bFrame)
	{
		for (const FGLBuildShapeDef& Shape : Def->FrameShapes)
		{
			Looks.Add(MakeBox(Shape, false, true, FrameColour));
		}
	}
	if (bLooked)
	{
		GLVisuals::Attach(this, GetRootComponent(), LookVisual);
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
