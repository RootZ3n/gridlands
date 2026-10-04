#include "Building/GLPlanRules.h"

FGLPlan GLPlanRules::Capture(TConstArrayView<FGLPlacedPiece> Pieces, const FVector& AnchorLocation, int32 AnchorYawStep)
{
	FGLPlan Plan;
	for (const FGLPlacedPiece& Piece : Pieces)
	{
		FGLPlanEntry& Entry = Plan.Entries.AddDefaulted_GetRef();
		Entry.Def = Piece.Def;
		const FVector Delta = Piece.Location - AnchorLocation;
		const FVector2D Local = GLStructureRules::RotateXY(FVector2D(Delta), -AnchorYawStep);
		Entry.Offset = FVector(Local, Delta.Z);
		Entry.YawStep = GLStructureRules::NormalizeYawStep(Piece.YawStep - AnchorYawStep);
		Entry.Layers = Piece.Layers;
	}
	return Plan;
}

TArray<FGLPlacedPiece> GLPlanRules::Instantiate(const FGLPlan& Plan, const FVector& AnchorLocation, int32 AnchorYawStep, int32 FirstId, EGLPieceOrigin Origin)
{
	TArray<FGLPlacedPiece> Out;
	int32 Id = FirstId;
	for (const FGLPlanEntry& Entry : Plan.Entries)
	{
		FGLPlacedPiece& Piece = Out.AddDefaulted_GetRef();
		Piece.Id = Id++;
		Piece.Def = Entry.Def;
		const FVector2D World = GLStructureRules::RotateXY(FVector2D(Entry.Offset), AnchorYawStep);
		Piece.Location = AnchorLocation + FVector(World, Entry.Offset.Z);
		Piece.YawStep = GLStructureRules::NormalizeYawStep(Entry.YawStep + AnchorYawStep);
		Piece.Origin = Origin;
		Piece.Layers = Entry.Layers;
	}
	return Out;
}
