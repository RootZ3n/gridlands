#pragma once

// P11 scaling (ADR-0039 §11): a test-only audit that a cell's player-construction presentation is exactly what the model
// says, in both directions. Shared by the presentation, WINCHESTER and streaming tests.

#include "Building/GLPiecePresentation.h"
#include "Building/GLPlayerPieceBatch.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLPlayerPresentationCheck
{
	inline bool Same(const FTransform& A, const FTransform& B)
	{
		return FVector::Dist(A.GetTranslation(), B.GetTranslation()) < 0.5 && A.GetRotation().AngularDistance(B.GetRotation()) < 1e-3
			&& A.GetScale3D().Equals(B.GetScale3D(), 1e-3);
	}

	/**
	 * Problems with Cell's player presentation (empty: consistent):
	 * - every intact piece is presented exactly once: instanced when it can be (in the cell's one batch), else by its own
	 *   actor; debris only by its actor; pending pieces not at all;
	 * - every instanced piece's collision and visible instances are exactly its look at its transform, its shown phase its
	 *   look's;
	 * - every collision instance's owner is a live instanced piece whose box it really is (identity through the owner
	 *   table, checked against geometry, never renderer order);
	 * - nothing else is in the batch, and at most one live batch exists for the cell.
	 */
	inline TArray<FString> Problems(UWorld* World, FName Cell)
	{
		TArray<FString> Out;
		UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
		const FGLStructureRuntime* Structure = Structures->Find(UGLStructureSubsystem::PlayerKey(Cell));
		const AGLPlayerPieceBatch* Batch = Structures->BatchOf(Cell);
		TMap<int32, TArray<FTransform>> Boxes;
		int32 Expected = 0, Instanced = 0;
		for (const FGLStructurePartRuntime& Part : Structure ? Structure->Parts : TArray<FGLStructurePartRuntime>())
		{
			const int32 Id = Part.Piece.Id;
			const bool bInBatch = Batch && Batch->Contains(Id);
			if (Structures->IsPartPending(Structure->Placement, Part.Name))
			{
				if (bInBatch || Part.Actor.IsValid())
				{
					Out.Add(FString::Printf(TEXT("piece %d is presented while still pending"), Id));
				}
				continue;
			}
			if (Part.State == EGLStructurePartState::Debris)
			{
				if (bInBatch || Part.bInstanced || !Part.Actor.IsValid())
				{
					Out.Add(FString::Printf(TEXT("debris %d: instanced %d, actor %d (an actor only)"), Id, bInBatch ? 1 : 0, Part.Actor.IsValid() ? 1 : 0));
				}
				continue;
			}
			if (Part.State != EGLStructurePartState::Intact)
			{
				continue;
			}
			const bool bShouldInstance = AGLPlayerPieceBatch::CanInstance(Part.Piece);
			if (bShouldInstance != bInBatch || bInBatch == Part.Actor.IsValid() || Part.bInstanced != bInBatch)
			{
				Out.Add(FString::Printf(TEXT("intact %d (%s): instanced %d, actor %d, flag %d"), Id, *Part.Piece.Def.ToString(), bInBatch ? 1 : 0, Part.Actor.IsValid() ? 1 : 0, Part.bInstanced ? 1 : 0));
				continue;
			}
			if (!bInBatch)
			{
				continue;
			}
			++Instanced;
			FGLPieceLook Look;
			GLPiecePresentation::Describe(Part.Piece, Look);
			const FTransform At = GLPiecePresentation::PieceTransform(Part.Piece);
			TArray<FTransform>& Mine = Boxes.Add(Id);
			int32 Visible = Look.Frame.Num() + (Look.Visual.IsNone() ? 0 : 1);
			for (const FGLPieceBox& Shape : Look.Shapes)
			{
				Mine.Add(Shape.Local * At);
				Visible += Shape.bVisible ? 1 : 0;
			}
			const TArray<FTransform> Collision = Batch->CollisionOf(Id);
			bool bWhere = Collision.Num() == Mine.Num();
			for (int32 I = 0; bWhere && I < Mine.Num(); ++I)
			{
				bWhere = Same(Collision[I], Mine[I]);
			}
			if (!bWhere)
			{
				Out.Add(FString::Printf(TEXT("piece %d: collision instances are not its shapes at its transform (%d vs %d)"), Id, Collision.Num(), Mine.Num()));
			}
			if (Batch->VisibleOf(Id).Num() != Visible)
			{
				Out.Add(FString::Printf(TEXT("piece %d: %d visible instances, its look has %d"), Id, Batch->VisibleOf(Id).Num(), Visible));
			}
			if (Batch->ShownPhaseOf(Id) != Look.Shown)
			{
				Out.Add(FString::Printf(TEXT("piece %d shows %s, its fact is %s"), Id, *Batch->ShownPhaseOf(Id).ToString(), *Look.Shown.ToString()));
			}
			Expected += Mine.Num() + Visible;
		}
		if (Batch)
		{
			if (Batch->NumPieces() != Instanced)
			{
				Out.Add(FString::Printf(TEXT("the batch holds %d pieces, the model %d instanced"), Batch->NumPieces(), Instanced));
			}
			int32 Highlight = 0;
			for (const int32 Id : Batch->GetHighlighted())
			{
				Highlight += Batch->CollisionOf(Id).Num();
			}
			if (Batch->NumInstances() - Highlight != Expected)
			{
				Out.Add(FString::Printf(TEXT("%d instances, the model's pieces need %d"), Batch->NumInstances() - Highlight, Expected));
			}
			const UInstancedStaticMeshComponent* Set = Batch->GetCollisionSet();
			for (int32 I = 0; Set && I < Set->GetInstanceCount(); ++I)
			{
				const int32 Owner = Batch->PieceIdAt(Set, I);
				FTransform T;
				Set->GetInstanceTransform(I, T, true);
				const TArray<FTransform>* Theirs = Boxes.Find(Owner);
				if (!Theirs || !Theirs->ContainsByPredicate([&T](const FTransform& B) { return Same(B, T); }))
				{
					Out.Add(FString::Printf(TEXT("collision instance %d says piece %d, but it is not one of that piece's boxes"), I, Owner));
				}
			}
		}
		else if (Instanced > 0)
		{
			Out.Add(TEXT("instanced pieces without a batch"));
		}
		int32 Live = 0;
		for (TActorIterator<AGLPlayerPieceBatch> It(World); It; ++It)
		{
			Live += It->IsHidden() ? 0 : 1;
		}
		if (Live > Structures->PlayerPresentation().Batches)
		{
			Out.Add(FString::Printf(TEXT("%d live batch actors for %d player structures"), Live, Structures->PlayerPresentation().Batches));
		}
		return Out;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
