#include "Building/GLBuildingSubsystem.h"

#include "Building/GLBuildPiece.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"

double UGLBuildingSubsystem::GroundAt(const FVector2D& At) const
{
	const UGLTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UGLTerrainSubsystem>();
	return Terrain && Terrain->HasGround() ? Terrain->HeightAt(At) : 0.0;
}

FGLBuildCheck UGLBuildingSubsystem::Check(const AActor* Builder, const FGLPlacedPiece& Candidate) const
{
	const UGLInventoryComponent* Inventory = Builder ? Builder->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	const UGLKnowledgeSubsystem* Knowledge = GetWorld()->GetSubsystem<UGLKnowledgeSubsystem>();
	if (!Inventory || !Knowledge)
	{
		FGLBuildCheck NoBuilder;
		NoBuilder.Refusal = EGLBuildRefusal::MissingItems;
		NoBuilder.Reason = TEXT("nobody to build it");
		return NoBuilder;
	}
	return GLStructureRules::CanPlace(GLContent::Get(), Pieces, Candidate, [this](const FVector2D& At) { return GroundAt(At); },
		Knowledge->GetKnowledge(), Inventory->GetInventory());
}

FGLBuildCheck UGLBuildingSubsystem::Place(AActor* Builder, const FGLPlacedPiece& Candidate)
{
	const FGLBuildCheck Result = Check(Builder, Candidate);
	if (!Result.IsAllowed())
	{
		Emit(TEXT("Event.Building.Refused"), Candidate.Def, Builder);
		return Result;
	}
	// Check() proved every cost is carried, so these removals cannot partly fail.
	UGLInventoryComponent* Inventory = Builder->FindComponentByClass<UGLInventoryComponent>();
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Candidate.Def);
	for (const FGLItemStackDef& Cost : Def->Cost)
	{
		verify(Inventory->RemoveItem(Cost.Item, Cost.Count));
	}
	FGLPlacedPiece Placed = Candidate;
	Placed.Id = NextId++;
	Pieces.Add(Placed);
	SpawnPiece(Placed);
	Emit(TEXT("Event.Building.Placed"), Placed.Def, Builder, { { TEXT("support"), Result.Support } });
	return Result;
}

FGLDemolishResult UGLBuildingSubsystem::Demolish(AActor* Builder, int32 PieceId)
{
	FGLDemolishResult Result;
	UGLInventoryComponent* Inventory = Builder ? Builder->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	const FGLPlacedPiece* Target = Pieces.FindByPredicate([PieceId](const FGLPlacedPiece& P) { return P.Id == PieceId; });
	if (!Target || !Inventory)
	{
		Result.Refusal = EGLDemolishRefusal::UnknownPiece;
		return Result;
	}
	Result.Removed.Add(PieceId);
	Result.Removed.Append(GLStructureRules::CollapsesAfterRemoving(GLContent::Get(), Pieces, PieceId, [this](const FVector2D& At) { return GroundAt(At); }));
	for (const int32 Id : Result.Removed)
	{
		const FGLPlacedPiece* Piece = Pieces.FindByPredicate([Id](const FGLPlacedPiece& P) { return P.Id == Id; });
		if (const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece->Def))
		{
			for (const FGLItemStackDef& Cost : Def->Cost)
			{
				Result.Refunded.FindOrAdd(Cost.Item) += Cost.Count;
			}
		}
	}
	// The whole refund must fit, or nothing happens (no silent loss).
	FGLInventory Trial = Inventory->GetInventory();
	for (const TPair<FName, int32>& Refund : Result.Refunded)
	{
		if (Trial.Add(GLContent::Get(), Refund.Key, Refund.Value) != Refund.Value)
		{
			Result.Refusal = EGLDemolishRefusal::NoRoomForRefund;
			Result.Removed.Reset();
			return Result;
		}
	}
	const FName TargetDef = Target->Def;
	for (const int32 Id : Result.Removed)
	{
		Pieces.RemoveAll([Id](const FGLPlacedPiece& P) { return P.Id == Id; });
		TObjectPtr<AGLBuildPiece> Actor;
		if (Actors.RemoveAndCopyValue(Id, Actor) && Actor)
		{
			Actor->Destroy();
		}
	}
	for (const TPair<FName, int32>& Refund : Result.Refunded)
	{
		verify(Inventory->AddItem(Refund.Key, Refund.Value) == Refund.Value);
	}
	const int32 Collapsed = Result.Removed.Num() - 1;
	Emit(TEXT("Event.Building.Demolished"), TargetDef, Builder, { { TEXT("collapsed"), static_cast<double>(Collapsed) } });
	if (Collapsed > 0)
	{
		Emit(TEXT("Event.Building.Collapsed"), TargetDef, Builder, { { TEXT("count"), static_cast<double>(Collapsed) } });
	}
	return Result;
}

bool UGLBuildingSubsystem::Snap(FName Def, const FVector& Aim, int32 YawQuarter, FGLPlacedPiece& OutCandidate) const
{
	return GLStructureRules::Snap(GLContent::Get(), Pieces, Def, Aim, YawQuarter, [this](const FVector2D& At) { return GroundAt(At); }, OutCandidate);
}

bool UGLBuildingSubsystem::IsUnderStructure(const FVector2D& World) const
{
	return GLStructureRules::IsUnderStructure(GLContent::Get(), Pieces, World);
}

TMap<int32, double> UGLBuildingSubsystem::Support() const
{
	return GLStructureRules::ComputeSupport(GLContent::Get(), Pieces, [this](const FVector2D& At) { return GroundAt(At); });
}

AGLBuildPiece* UGLBuildingSubsystem::FindActor(int32 PieceId) const
{
	const TObjectPtr<AGLBuildPiece>* Found = Actors.Find(PieceId);
	return Found ? Found->Get() : nullptr;
}

int32 UGLBuildingSubsystem::PieceIdOf(const AActor* Actor) const
{
	for (const TPair<int32, TObjectPtr<AGLBuildPiece>>& Entry : Actors)
	{
		if (Entry.Value == Actor)
		{
			return Entry.Key;
		}
	}
	return 0;
}

void UGLBuildingSubsystem::Restore(const TArray<FGLPlacedPiece>& InPieces, int32 InNextId)
{
	for (const TPair<int32, TObjectPtr<AGLBuildPiece>>& Entry : Actors)
	{
		if (Entry.Value)
		{
			Entry.Value->Destroy();
		}
	}
	Actors.Reset();
	Pieces.Reset();
	NextId = FMath::Max(1, InNextId);
	for (const FGLPlacedPiece& Piece : InPieces)
	{
		if (!GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def))
		{
			continue; // stale content ids are reported by the save subsystem
		}
		Pieces.Add(Piece);
		SpawnPiece(Piece);
		NextId = FMath::Max(NextId, Piece.Id + 1);
	}
}

AGLBuildPiece* UGLBuildingSubsystem::SpawnPiece(const FGLPlacedPiece& Piece)
{
	AGLBuildPiece* Actor = GetWorld()->SpawnActor<AGLBuildPiece>(Piece.Location, FRotator(0.0, 90.0 * Piece.YawQuarter, 0.0));
	if (Actor && Actor->Setup(Piece))
	{
		Actors.Add(Piece.Id, Actor);
	}
	return Actor;
}

void UGLBuildingSubsystem::Emit(const TCHAR* Tag, FName Subject, AActor* Instigator, const TMap<FName, double>& Numbers)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
	Event.Subject = Subject;
	Event.Instigator = Instigator;
	for (const TPair<FName, double>& Number : Numbers)
	{
		Event.Numbers.Add(Number.Key, Number.Value);
	}
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
}
