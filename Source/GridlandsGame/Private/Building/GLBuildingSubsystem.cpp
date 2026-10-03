#include "Building/GLBuildingSubsystem.h"

#include "Noise/GLNoiseSubsystem.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"

#include "Building/GLBuildPiece.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Economy/GLWorldSettingsSubsystem.h"
#include "Economy/GLYield.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLGridCells.h"

void FGLMaterialSources::AnnounceAcquired(AActor* Who, const TMap<FName, int32>& Items) const
{
	if (!Who)
	{
		return;
	}
	TArray<FName> Keys;
	Items.GetKeys(Keys);
	Keys.Sort(FNameLexicalLess());
	for (const FName& Item : Keys)
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Item.Acquired"));
		Event.Subject = Item;
		Event.Instigator = Who;
		Event.Numbers.Add(TEXT("count"), Items[Item]);
		UGLEventSubsystem::Emit(Who, MoveTemp(Event));
	}
}

double UGLBuildingSubsystem::GroundAt(const FVector2D& At) const
{
	const UGLTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UGLTerrainSubsystem>();
	return Terrain && Terrain->HasGround() ? Terrain->HeightAt(At) : 0.0;
}

TArray<FGLPlacedPiece> UGLBuildingSubsystem::GetPieces() const
{
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	return Structures ? Structures->PlayerPieces() : TArray<FGLPlacedPiece>();
}

TArray<FGLPlacedPiece> UGLBuildingSubsystem::PiecesOfCell(FName Cell) const
{
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	return Structures ? Structures->PlayerPieces(Cell) : TArray<FGLPlacedPiece>();
}

TSet<FName> UGLBuildingSubsystem::CellsWithPieces() const
{
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	return Structures ? Structures->CellsWithPlayerPieces() : TSet<FName>();
}

TArray<FGLClaim> UGLBuildingSubsystem::Claims() const
{
	return GLClaimRules::ClaimsFrom(GLContent::Get(), GetPieces(), GLContent::Tuning().Building.ClaimRadiusMetres * 100.0);
}

FGLInventory* UGLBuildingSubsystem::StorageOf(int32 PieceId)
{
	UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	FGLStructurePartRuntime* Part = Structures ? Structures->FindPlayerPieceMutable(PieceId) : nullptr;
	return Part && Part->State == EGLStructurePartState::Intact && Part->Contents.GetMaxSlots() > 0 ? &Part->Contents : nullptr;
}

FGLMaterialSources UGLBuildingSubsystem::SourcesFor(const AActor* Who, const FVector& At) const
{
	FGLMaterialSources Sources;
	Sources.Personal = Who ? const_cast<AActor*>(Who)->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	const TArray<FGLClaim> AllClaims = Claims();
	const FGLClaim* Claim = Who ? GLClaimRules::ClaimAt(AllClaims, FVector2D(Who->GetActorLocation())) : nullptr;
	if (Claim && Claim->Contains(FVector2D(At)))
	{
		// Eligible storage: player-built, intact (a falling piece is debris the moment its support failed), storage-capable,
		// inside the same claim. Deterministic order: distance from the operation's point, then piece id.
		Sources.Claim = Claim->Id;
		UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
		TArray<TPair<double, int32>> Order;
		for (const FGLPlacedPiece& Piece : GetPieces())
		{
			const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);
			if (Def && Def->Storage.Slots > 0 && Piece.Origin == EGLPieceOrigin::Player && Claim->Contains(FVector2D(Piece.Location)))
			{
				Order.Add({ FVector::Dist(Piece.Location, At), Piece.Id });
			}
		}
		Order.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });
		for (const TPair<double, int32>& Entry : Order)
		{
			if (FGLStructurePartRuntime* Part = Structures->FindPlayerPieceMutable(Entry.Value))
			{
				Sources.Inventories.Add(&Part->Contents);
				Sources.Containers.Add(Entry.Value);
			}
		}
	}
	// Consumption: storage first, then Zenny. Delivery: Zenny first, then storage.
	const int32 Storage = Sources.Inventories.Num();
	if (Sources.Personal)
	{
		Sources.Inventories.Add(&Sources.Personal->GetMutableInventory());
		Sources.DeliverOrder.Add(Storage);
	}
	for (int32 I = 0; I < Storage; ++I)
	{
		Sources.DeliverOrder.Add(I);
	}
	return Sources;
}

TArray<FName> UGLBuildingSubsystem::StationsNear(const FVector& At, double ReachCm) const
{
	TArray<FName> Out;
	for (const FGLPlacedPiece& Piece : GetPieces())
	{
		const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);
		if (Def && !Def->Station.IsNone() && FVector::Dist(Piece.Location, At) <= ReachCm)
		{
			Out.AddUnique(Def->Station);
		}
	}
	return Out;
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
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Candidate.Def);
	if (Def && !Def->Buildable)
	{
		FGLBuildCheck WorldOnly;
		WorldOnly.Refusal = EGLBuildRefusal::UnknownPiece;
		WorldOnly.Reason = TEXT("that is not something Zenny can build");
		return WorldOnly;
	}
	const FGLMaterialSources Sources = SourcesFor(Builder, Candidate.Location);
	const FGLMaterialPool Pool = Sources.Pool();
	FGLBuildCheck Result = GLStructureRules::CanPlace(GLContent::Get(), GetPieces(), Candidate, [this](const FVector2D& At) { return GroundAt(At); },
		Knowledge->GetKnowledge(), [&Pool](FName Item) { return Pool.CountOf(Item); });
	// Authored structures and any debris (P6) are in the way like any other piece (oriented, P11).
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	if (Result.IsAllowed() && Def && Structures && Structures->Overlaps(GLStructureRules::Footprint(*Def, Candidate)))
	{
		Result.Refusal = EGLBuildRefusal::Overlaps;
		Result.Reason = TEXT("something is already there");
	}
	// P11 behaviour (not an invariant): a new base core's claim may not overlap another claim.
	if (Result.IsAllowed() && Def && Def->Role == FName(GLClaimRules::RoleBaseCore))
	{
		FGLPlacedPiece Core = Candidate;
		Core.Origin = EGLPieceOrigin::Player;
		Core.Id = INT32_MAX;
		const TArray<FGLClaim> New = GLClaimRules::ClaimsFrom(GLContent::Get(), { Core }, GLContent::Tuning().Building.ClaimRadiusMetres * 100.0);
		for (const FGLClaim& Existing : Claims())
		{
			if (New.Num() && New[0].Overlaps(Existing))
			{
				Result.Refusal = EGLBuildRefusal::OutsideClaim;
				Result.Reason = TEXT("too close to another base (claims may not overlap)");
				break;
			}
		}
	}
	Result.Preview = GLStructureRules::PreviewOf(Result);
	return Result;
}

FGLBuildCheck UGLBuildingSubsystem::Place(AActor* Builder, const FGLPlacedPiece& Candidate)
{
	const FGLBuildCheck Result = Check(Builder, Candidate);
	if (!Result.IsAllowed())
	{
		Emit(TEXT("Event.Building.Refused"), Candidate.Def, Builder);
		return Result;
	}
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Candidate.Def);
	FGLMaterialSources Sources = SourcesFor(Builder, Candidate.Location);
	FGLMaterialPool Pool = Sources.Pool();
	FGLPlacedPiece Placed = Candidate;
	Placed.Id = NextId++;
	Placed.Cell = CellFor(Placed.Location);
	Placed.Origin = EGLPieceOrigin::Player;
	Placed.Layers.Reset(); // a piece is placed as its FRAME; finishes are installed afterwards
	if (!GetWorld()->GetSubsystem<UGLStructureSubsystem>()->AddPlayerPiece(Placed, false))
	{
		FGLBuildCheck Refused = Result;
		Refused.Refusal = EGLBuildRefusal::UnknownPiece;
		Refused.Reason = TEXT("could not be added to the structure");
		Refused.Preview = EGLPreview::Red;
		return Refused; // nothing was paid
	}
	verify(Pool.Consume(Def->Cost)); // Check() proved every cost is available: all or nothing
	Emit(TEXT("Event.Building.Placed"), Placed.Def, Builder, { { TEXT("support"), Result.Support }, { TEXT("fromStorage"), static_cast<double>(Sources.Containers.Num()) } });
	UGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Build.Place"), GLStructureRules::Bounds(*Def, Placed).GetCenter(), Builder, Def->Material);
	return Result;
}

TArray<int32> UGLBuildingSubsystem::PreviewRemoval(int32 PieceId) const
{
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	if (!Structures || !Structures->FindPlayerPiece(PieceId))
	{
		return {};
	}
	return GLStructureRules::CollapsesAfterRemoving(GLContent::Get(), Structures->PlayerStructureOf(PieceId), PieceId, [this](const FVector2D& At) { return GroundAt(At); });
}

TMap<FName, int32> UGLBuildingSubsystem::ScaledYield(const FGLPlacedPiece& Piece, EGLSalvagePath Path) const
{
	TMap<FName, int32> Items;
	const UGLWorldSettingsSubsystem* Settings = GetWorld()->GetSubsystem<UGLWorldSettingsSubsystem>();
	const FGLSettingsPresetDef* Preset = Settings ? Settings->GetPreset() : nullptr;
	for (const FGLSalvageYieldDef& Yield : GLConstructionRules::PieceYields(GLContent::Get(), Piece, Path))
	{
		const FGLYieldCategoryDef* Category = GLContent::Get().Find<FGLYieldCategoryDef>(Yield.YieldCategory);
		Items.FindOrAdd(Yield.Item) += Category && Preset ? GLYield::Apply(Yield.Count, *Category, *Preset) : Yield.Count; // E-1
	}
	return Items;
}

FGLDemolishResult UGLBuildingSubsystem::Remove(AActor* Builder, int32 PieceId, EGLSalvagePath Path)
{
	FGLDemolishResult Result;
	Result.Path = Path;
	UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	const FGLStructurePartRuntime* Part = Structures ? Structures->FindPlayerPiece(PieceId) : nullptr;
	if (!Part || Part->State != EGLStructurePartState::Intact || !Builder)
	{
		Result.Refusal = EGLDemolishRefusal::UnknownPiece;
		return Result;
	}
	if (!Part->Contents.IsEmpty())
	{
		Result.Refusal = EGLDemolishRefusal::StorageNotEmpty; // empty it first: its contents are never destroyed
		return Result;
	}
	const FGLPlacedPiece Piece = Part->Piece;
	Result.Recovered = ScaledYield(Piece, Path);
	FGLMaterialSources Sources = SourcesFor(Builder, Piece.Location);
	FGLMaterialPool Pool = Sources.Pool();
	if (!Pool.CanDeliver(GLContent::Get(), Result.Recovered))
	{
		Result.Refusal = EGLDemolishRefusal::NoRoomForRefund; // the whole return must fit, or nothing happens
		if (Sources.Personal)
		{
			Sources.Personal->AnnounceFull(Piece.Def);
		}
		return Result;
	}
	Result.Predicted = PreviewRemoval(PieceId);
	verify(Pool.Deliver(GLContent::Get(), Result.Recovered));
	Sources.AnnounceAcquired(Builder, Result.Recovered);
	Result.Collapsed = Structures->RemovePlayerPiece(PieceId, Builder);
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);
	UGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Build.Demolish"), Piece.Location, Builder, Def ? Def->Material : NAME_None);
	Emit(Path == EGLSalvagePath::Destructive ? TEXT("Event.Building.Smashed") : TEXT("Event.Building.Demolished"), Piece.Def, Builder,
		{ { TEXT("collapsed"), static_cast<double>(Result.Collapsed.Num()) } });
	if (Result.Collapsed.Num() > 0)
	{
		Emit(TEXT("Event.Building.Collapsed"), Piece.Def, Builder, { { TEXT("count"), static_cast<double>(Result.Collapsed.Num()) } });
	}
	if (Result.Collapsed != Result.Predicted)
	{
		UE_LOG(LogGridlands, Error, TEXT("Building: removal of %d: predicted %d collapsing, %d collapsed (PREVIEW != REALITY)"), PieceId, Result.Predicted.Num(), Result.Collapsed.Num());
	}
	return Result;
}

FGLInstallCheck UGLBuildingSubsystem::CheckInstall(const AActor* Builder, int32 PieceId, FName Layer) const
{
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	const FGLStructurePartRuntime* Part = Structures ? Structures->FindPlayerPiece(PieceId) : nullptr;
	FGLInstallCheck Check;
	if (!Part || Part->State != EGLStructurePartState::Intact)
	{
		Check.Refusal = EGLInstallRefusal::UnknownLayer;
		Check.Reason = TEXT("nothing to finish there");
		return Check;
	}
	const UGLKnowledgeSubsystem* Knowledge = GetWorld()->GetSubsystem<UGLKnowledgeSubsystem>();
	Check = GLConstructionRules::CanInstall(GLContent::Get(), Part->Piece, Layer, Knowledge ? &Knowledge->GetKnowledge() : nullptr);
	if (Check.IsAllowed())
	{
		const FGLFinishDef* Finish = GLContent::Get().Find<FGLFinishDef>(Layer);
		if (!SourcesFor(Builder, Part->Piece.Location).Pool().CanConsume(Finish->Cost))
		{
			Check.Refusal = EGLInstallRefusal::UnknownLayer;
			Check.Reason = TEXT("missing materials");
		}
	}
	return Check;
}

FGLInstallCheck UGLBuildingSubsystem::InstallFinish(AActor* Builder, int32 PieceId, FName Layer)
{
	const FGLInstallCheck Check = CheckInstall(Builder, PieceId, Layer);
	if (!Check.IsAllowed())
	{
		return Check;
	}
	UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	const FGLPlacedPiece Piece = Structures->FindPlayerPiece(PieceId)->Piece;
	FGLMaterialSources Sources = SourcesFor(Builder, Piece.Location);
	FGLMaterialPool Pool = Sources.Pool();
	verify(Pool.Consume(GLContent::Get().Find<FGLFinishDef>(Layer)->Cost));
	Structures->SetPlayerLayers(PieceId, GLConstructionRules::WithLayer(GLContent::Get(), Piece, Layer).Layers);
	Emit(TEXT("Event.Building.FinishInstalled"), Layer, Builder);
	return Check;
}

bool UGLBuildingSubsystem::Snap(FName Def, const FVector& Aim, int32 YawStep, FGLPlacedPiece& OutCandidate) const
{
	return GLStructureRules::Snap(GLContent::Get(), GetPieces(), Def, Aim, YawStep, [this](const FVector2D& At) { return GroundAt(At); }, OutCandidate);
}

bool UGLBuildingSubsystem::IsUnderStructure(const FVector2D& World) const
{
	// The structural model holds player pieces too: its footprints are oriented and need no copy of the piece list (the
	// terrain asks per vertex, vegetation per tuft).
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	return Structures && Structures->IsUnderStructure(World, 50.0);
}

TMap<int32, double> UGLBuildingSubsystem::Support() const
{
	TMap<int32, double> All;
	for (const FName& Cell : CellsWithPieces())
	{
		All.Append(GLStructureRules::ComputeSupport(GLContent::Get(), PiecesOfCell(Cell), [this](const FVector2D& At) { return GroundAt(At); }));
	}
	return All;
}

AGLBuildPiece* UGLBuildingSubsystem::FindActor(int32 PieceId) const
{
	const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>();
	const FGLStructurePartRuntime* Part = Structures ? Structures->FindPlayerPiece(PieceId) : nullptr;
	return Part ? Part->Actor.Get() : nullptr;
}

int32 UGLBuildingSubsystem::PieceIdOf(const AActor* Actor) const
{
	const AGLStructurePart* Part = Cast<AGLStructurePart>(Actor);
	if (!Part || !Part->StructurePlacement.ToString().StartsWith(TEXT("player:")))
	{
		return 0;
	}
	return Part->GetPiece().Id;
}

FName UGLBuildingSubsystem::CellFor(const FVector& Location) const
{
	FName Cell;
	const UGLTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UGLTerrainSubsystem>();
	if (Terrain && Terrain->GroundCellAt(FVector2D(Location), Cell))
	{
		return Cell;
	}
	return GLGridCells::CellAt(FVector2D(Location));
}

void UGLBuildingSubsystem::RestoreCell(FName Cell, const TArray<FGLSavedPiece>& Saved, const TArray<FGLSavedCollapse>& InFlight, TArray<FString>* OutProblems)
{
	for (const FGLSavedPiece& Piece : Saved)
	{
		NextId = FMath::Max(NextId, Piece.Id + 1);
	}
	GetWorld()->GetSubsystem<UGLStructureSubsystem>()->RestorePlayerCell(Cell, Saved, InFlight, OutProblems);
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
