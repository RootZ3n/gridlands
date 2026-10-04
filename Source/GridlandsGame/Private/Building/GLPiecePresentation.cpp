#include "Building/GLPiecePresentation.h"

#include "Building/GLConstructionRules.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Presentation/GLVisuals.h"

namespace
{
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

	FTransform BoxTransform(const FGLBuildShapeDef& Shape)
	{
		// The engine cube is 100 cm on a side, centred on its origin.
		const FQuat Tilt(FVector::XAxisVector, FMath::DegreesToRadians(Shape.Pitch)); // positive lifts +Y
		return FTransform(Tilt, FVector(Axis(Shape.Offset, 0), Axis(Shape.Offset, 1), Axis(Shape.Offset, 2)) * 100.0,
			FVector(Axis(Shape.Size, 0), Axis(Shape.Size, 1), Axis(Shape.Size, 2)));
	}

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

FTransform GLPiecePresentation::PieceTransform(const FGLPlacedPiece& Piece)
{
	return FTransform(FRotator(0.0, GLStructureRules::YawDegrees(Piece.YawStep), 0.0), Piece.Location);
}

FTransform GLPiecePresentation::VisualTransform(FName Visual)
{
	const FGLVisualDef* Def = GLContent::Get().Find<FGLVisualDef>(Visual);
	if (!Def)
	{
		return FTransform::Identity;
	}
	const FVector Offset = Def->Offset.Num() >= 3 ? FVector(Def->Offset[0], Def->Offset[1], Def->Offset[2]) : FVector::ZeroVector;
	return FTransform(FRotator(0.0, Def->Yaw, 0.0), Offset * 100.0, FVector(Def->Scale));
}

bool GLPiecePresentation::Describe(const FGLPlacedPiece& Piece, FGLPieceLook& Out, bool bGhost)
{
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);
	if (!Def)
	{
		return false;
	}
	Out = FGLPieceLook();
	const bool bFrame = !bGhost && GLConstructionRules::ShowsFrame(GLContent::Get(), Piece);
	const FGLFinishDef* Finish = bGhost ? nullptr : VisibleFinish(Piece);
	const FName LookVisual = bFrame ? NAME_None : (Finish ? Finish->Visual : Def->Visual);
	const FGLVisualDef* Look = bGhost || LookVisual.IsNone() ? nullptr : GLContent::Get().Find<FGLVisualDef>(LookVisual);
	const bool bLooked = Look && GLVisuals::LoadMesh(Look->Mesh);
	const bool bTinted = !bFrame && !bLooked && Finish && Finish->Tint.Num() >= 3;
	Out.Shown = bFrame ? FName(TEXT("frame")) : (Finish ? FName(TEXT("finish")) : FName(TEXT("complete")));
	const FLinearColor Body = bTinted ? FLinearColor(Finish->Tint[0], Finish->Tint[1], Finish->Tint[2]) : ColourFor(*Def);
	for (const FGLBuildShapeDef& Shape : Def->Shapes)
	{
		Out.Shapes.Add({ BoxTransform(Shape), Body, !(bLooked || bFrame) });
	}
	if (bFrame)
	{
		for (const FGLBuildShapeDef& Shape : Def->FrameShapes)
		{
			Out.Frame.Add({ BoxTransform(Shape), FrameColour, true });
		}
	}
	Out.Visual = bLooked ? LookVisual : NAME_None;
	return true;
}
