#include "Building/GLBuildModeComponent.h"

#include "Building/GLBuildPiece.h"
#include "Building/GLBuildText.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"

namespace
{
	const TCHAR* ModeName(EGLToolMode Mode)
	{
		switch (Mode)
		{
		case EGLToolMode::Build: return TEXT("BUILD");
		case EGLToolMode::Dig: return TEXT("DIG");
		case EGLToolMode::Raise: return TEXT("RAISE");
		case EGLToolMode::Flatten: return TEXT("FLATTEN");
		default: return TEXT("");
		}
	}

	FName TerraformFor(EGLToolMode Mode)
	{
		switch (Mode)
		{
		case EGLToolMode::Dig: return TEXT("terraform.shovel.dig");
		case EGLToolMode::Raise: return TEXT("terraform.shovel.raise");
		case EGLToolMode::Flatten: return TEXT("terraform.shovel.flatten");
		default: return NAME_None;
		}
	}

	FString VocabularyOf(const FGLBuildPieceDef& Def)
	{
		const FGLEraDef* Era = GLContent::Get().Find<FGLEraDef>(Def.Era);
		const FGLMaterialDef* Material = GLContent::Get().Find<FGLMaterialDef>(Def.Material);
		return FString::Printf(TEXT("%s, %s"), Era && !Era->DisplayName.IsEmpty() ? *Era->DisplayName : *Def.Era.ToString(),
			Material && !Material->DisplayName.IsEmpty() ? *Material->DisplayName : *Def.Material.ToString());
	}

	/** A collapse confirmation never transfers: a different target or a different prediction starts over. */
	bool SameConfirmation(int32 Target, const TArray<int32>& Prediction, int32 OtherTarget, const TArray<int32>& OtherPrediction)
	{
		return Target != 0 && Target == OtherTarget && Prediction == OtherPrediction;
	}
}

UGLBuildModeComponent::UGLBuildModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	Favorites.Init(NAME_None, FavoriteSlots);
	ProfilePath = FPaths::ProjectSavedDir() / TEXT("Profile") / TEXT("build-profile.json");
}

void UGLBuildModeComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadProfile();
}

void UGLBuildModeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearGhost();
	Super::EndPlay(Reason);
}

void UGLBuildModeComponent::Count(const TCHAR* Intent)
{
	IntentCounts.FindOrAdd(FName(Intent)) += 1;
	++IntentTotal;
}

// ---- profile (playtest convenience: favorites, recents, remembered finishes, settings; never part of a world save) ----

void UGLBuildModeComponent::SetProfilePath(const FString& InPath)
{
	ProfilePath = InPath;
	bProfileLoaded = false;
	LoadProfile();
}

void UGLBuildModeComponent::LoadProfile()
{
	bProfileLoaded = true;
	Favorites.Init(NAME_None, FavoriteSlots);
	Recents.Reset();
	FinishByRole.Reset();
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *ProfilePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("favorites"), List))
	{
		for (int32 I = 0; I < FMath::Min(List->Num(), FavoriteSlots); ++I)
		{
			const FName Id((*List)[I]->AsString());
			Favorites[I] = GLContent::Get().Find<FGLBuildPieceDef>(Id) ? Id : NAME_None; // a removed piece is dropped, not kept
		}
	}
	if (Root->TryGetArrayField(TEXT("recents"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const FName Id(Value->AsString());
			if (GLContent::Get().Find<FGLBuildPieceDef>(Id) && !Recents.Contains(Id) && Recents.Num() < RecentCount)
			{
				Recents.Add(Id);
			}
		}
	}
	const TSharedPtr<FJsonObject>* Finishes = nullptr;
	if (Root->TryGetObjectField(TEXT("finishByRole"), Finishes))
	{
		for (const auto& Entry : (*Finishes)->Values)
		{
			const FName Finish(Entry.Value->AsString());
			if (GLContent::Get().Find<FGLFinishDef>(Finish))
			{
				FinishByRole.Add(FName(*Entry.Key), Finish);
			}
		}
	}
	FString Camera, Confirm, LastText;
	Root->TryGetStringField(TEXT("cameraMode"), Camera);
	Root->TryGetStringField(TEXT("confirmMode"), Confirm);
	Root->TryGetStringField(TEXT("lastPiece"), LastText);
	CameraMode = Camera == TEXT("toggle") ? EGLHoldMode::Toggle : EGLHoldMode::Hold;
	ConfirmMode = Confirm == TEXT("toggle") ? EGLHoldMode::Toggle : EGLHoldMode::Hold;
	const FName Last(LastText);
	if (GLContent::Get().Find<FGLBuildPieceDef>(Last))
	{
		Piece = Last;
	}
}

void UGLBuildModeComponent::SaveProfile() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Favs, Recent;
	for (const FName& Id : Favorites)
	{
		Favs.Add(MakeShared<FJsonValueString>(Id.IsNone() ? FString() : Id.ToString()));
	}
	for (const FName& Id : Recents)
	{
		Recent.Add(MakeShared<FJsonValueString>(Id.ToString()));
	}
	TSharedRef<FJsonObject> Finishes = MakeShared<FJsonObject>();
	TArray<FName> Roles;
	FinishByRole.GetKeys(Roles);
	Roles.Sort(FNameLexicalLess());
	for (const FName& Role : Roles)
	{
		Finishes->SetStringField(Role.ToString(), FinishByRole[Role].ToString());
	}
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	Root->SetArrayField(TEXT("favorites"), Favs);
	Root->SetArrayField(TEXT("recents"), Recent);
	Root->SetObjectField(TEXT("finishByRole"), Finishes);
	Root->SetStringField(TEXT("cameraMode"), CameraMode == EGLHoldMode::Toggle ? TEXT("toggle") : TEXT("hold"));
	Root->SetStringField(TEXT("confirmMode"), ConfirmMode == EGLHoldMode::Toggle ? TEXT("toggle") : TEXT("hold"));
	Root->SetStringField(TEXT("lastPiece"), Piece.IsNone() ? FString() : Piece.ToString());
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ProfilePath), true);
	FFileHelper::SaveStringToFile(Text, *ProfilePath);
}

void UGLBuildModeComponent::SetCameraMode(EGLHoldMode InMode)
{
	CameraMode = InMode;
	bBuildCamera = false;
	SaveProfile();
}

void UGLBuildModeComponent::SetConfirmMode(EGLHoldMode InMode)
{
	ConfirmMode = InMode;
	bHolding = bArmed = false;
	SaveProfile();
}

// ---- catalogue and selection ----

void UGLBuildModeComponent::RebuildCatalog()
{
	Catalog = GLBuildCatalog::Build(GLContent::Get());
	if (CategoryIndexOf(Piece) == INDEX_NONE)
	{
		Piece = Catalog.Num() && Catalog[0].Pieces.Num() ? Catalog[0].Pieces[0] : NAME_None;
	}
}

int32 UGLBuildModeComponent::CategoryIndexOf(FName InPiece) const
{
	return Catalog.IndexOfByPredicate([InPiece](const FGLCatalogCategory& C) { return C.Pieces.Contains(InPiece); });
}

TArray<FName> UGLBuildModeComponent::PiecesOf(int32 CategoryIndex) const
{
	TArray<FName> Out;
	if (!Catalog.IsValidIndex(CategoryIndex))
	{
		return Out;
	}
	for (const FName& Id : Catalog[CategoryIndex].Pieces)
	{
		const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Id);
		if (EraFilter.IsNone() || (Def && Def->Era == EraFilter)) // a filter narrows the list; it never forbids building
		{
			Out.Add(Id);
		}
	}
	return Out;
}

void UGLBuildModeComponent::SelectPiece(FName InPiece)
{
	if (CategoryIndexOf(InPiece) == INDEX_NONE)
	{
		return;
	}
	Piece = InPiece;
	LastPieceByCategory.Add(Catalog[CategoryIndexOf(InPiece)].Id, InPiece);
	ClearGhost();
	SaveProfile();
}

void UGLBuildModeComponent::SetState(EGLBuildState InState)
{
	if (State != InState)
	{
		bHolding = bArmed = false; // a confirmation never survives a change of mode
		HoldElapsed = ArmElapsed = 0.f;
		ConfirmTarget = 0;
		ConfirmPrediction.Reset();
	}
	State = InState;
	if (State != EGLBuildState::Place)
	{
		ClearGhost();
	}
}

void UGLBuildModeComponent::ToggleBuild()
{
	Count(TEXT("ToggleBuild"));
	Mode = Mode == EGLToolMode::Build ? EGLToolMode::None : EGLToolMode::Build;
	if (!bProfileLoaded)
	{
		LoadProfile();
	}
	RebuildCatalog();
	SetState(EGLBuildState::Place);
	if (Mode != EGLToolMode::Build)
	{
		bBuildCamera = false;
		ClearGhost();
	}
}

void UGLBuildModeComponent::CycleTerraform()
{
	Count(TEXT("CycleTerraform"));
	ClearGhost();
	bBuildCamera = false;
	switch (Mode)
	{
	case EGLToolMode::Dig: Mode = EGLToolMode::Raise; break;
	case EGLToolMode::Raise: Mode = EGLToolMode::Flatten; break;
	case EGLToolMode::Flatten: Mode = EGLToolMode::None; break;
	default: Mode = EGLToolMode::Dig; break;
	}
}

void UGLBuildModeComponent::ToggleBrowser()
{
	Count(TEXT("ToggleBrowser"));
	if (Mode != EGLToolMode::Build)
	{
		return;
	}
	if (State == EGLBuildState::Browse)
	{
		SetState(EGLBuildState::Place);
		return;
	}
	BrowseCategoryIndex = FMath::Max(0, CategoryIndexOf(Piece));
	BrowseIndex = FMath::Max(0, PiecesOf(BrowseCategoryIndex).IndexOfByKey(Piece));
	SetState(EGLBuildState::Browse);
}

void UGLBuildModeComponent::BrowseMove(int32 Delta)
{
	Count(TEXT("BrowseMove"));
	const int32 N = PiecesOf(BrowseCategoryIndex).Num();
	if (State == EGLBuildState::Browse && N > 0)
	{
		BrowseIndex = ((BrowseIndex + Delta) % N + N) % N;
	}
}

void UGLBuildModeComponent::BrowseCategory(int32 Delta)
{
	Count(TEXT("BrowseCategory"));
	if (State == EGLBuildState::Browse && Catalog.Num() > 0)
	{
		BrowseCategoryIndex = ((BrowseCategoryIndex + Delta) % Catalog.Num() + Catalog.Num()) % Catalog.Num();
		BrowseIndex = 0;
	}
}

void UGLBuildModeComponent::BrowseSelect()
{
	Count(TEXT("BrowseSelect"));
	const TArray<FName> List = PiecesOf(BrowseCategoryIndex);
	if (State == EGLBuildState::Browse && List.IsValidIndex(BrowseIndex))
	{
		SelectPiece(List[BrowseIndex]);
		SetState(EGLBuildState::Place); // predictable: a selection always returns to placing
	}
}

void UGLBuildModeComponent::SetEraFilter(FName Era)
{
	Count(TEXT("SetEraFilter"));
	EraFilter = Era;
	BrowseIndex = 0;
}

void UGLBuildModeComponent::Back()
{
	Count(TEXT("Back"));
	if (Mode == EGLToolMode::Build && State != EGLBuildState::Place)
	{
		SetState(EGLBuildState::Place);
	}
	else if (Mode != EGLToolMode::None)
	{
		Mode = EGLToolMode::None;
		bBuildCamera = false;
		ClearGhost();
		SetState(EGLBuildState::Place);
	}
}

void UGLBuildModeComponent::CycleVariant(int32 Delta)
{
	Count(TEXT("CycleVariant"));
	if (Mode != EGLToolMode::Build || State != EGLBuildState::Place)
	{
		return;
	}
	const TArray<FName> List = PiecesOf(FMath::Max(0, CategoryIndexOf(Piece)));
	if (List.Num() > 0)
	{
		const int32 At = FMath::Max(0, List.IndexOfByKey(Piece));
		SelectPiece(List[((At + Delta) % List.Num() + List.Num()) % List.Num()]);
	}
}

void UGLBuildModeComponent::CycleCategory(int32 Delta)
{
	Count(TEXT("CycleCategory"));
	if (Mode != EGLToolMode::Build || State != EGLBuildState::Place || Catalog.Num() == 0)
	{
		return;
	}
	const int32 Next = ((FMath::Max(0, CategoryIndexOf(Piece)) + Delta) % Catalog.Num() + Catalog.Num()) % Catalog.Num();
	const FName* Remembered = LastPieceByCategory.Find(Catalog[Next].Id);
	const TArray<FName> List = PiecesOf(Next);
	SelectPiece(Remembered && List.Contains(*Remembered) ? *Remembered : (List.Num() ? List[0] : Catalog[Next].Pieces[0]));
}

void UGLBuildModeComponent::SelectFavorite(int32 Slot)
{
	Count(TEXT("SelectFavorite"));
	if (Mode == EGLToolMode::Build && Favorites.IsValidIndex(Slot) && !Favorites[Slot].IsNone())
	{
		SelectPiece(Favorites[Slot]);
		SetState(EGLBuildState::Place);
	}
}

void UGLBuildModeComponent::PinFavorite(int32 Slot)
{
	Count(TEXT("PinFavorite"));
	const TArray<FName> List = PiecesOf(BrowseCategoryIndex);
	const FName Pin = State == EGLBuildState::Browse && List.IsValidIndex(BrowseIndex) ? List[BrowseIndex] : Piece;
	if (Mode == EGLToolMode::Build && Favorites.IsValidIndex(Slot) && !Pin.IsNone())
	{
		Favorites[Slot] = Pin;
		SaveProfile();
	}
}

void UGLBuildModeComponent::RotateQuarter(int32 Direction)
{
	Count(TEXT("RotateQuarter"));
	YawStep = GLStructureRules::NormalizeYawStep(YawStep + (Direction >= 0 ? 1 : -1) * GLStructureRules::QuarterTurnSteps);
	ClearGhost();
}

void UGLBuildModeComponent::Rotate15()
{
	Count(TEXT("Rotate15"));
	YawStep = GLStructureRules::NormalizeYawStep(YawStep + GLStructureRules::YawStepFromDegrees(15.0));
	ClearGhost();
}

void UGLBuildModeComponent::RotateFine(int32 Direction)
{
	Count(TEXT("RotateFine"));
	YawStep = GLStructureRules::NormalizeYawStep(YawStep + (Direction >= 0 ? 1 : -1)); // one canonical 2.5 degree step
	ClearGhost();
}

void UGLBuildModeComponent::ToggleFinishMode()
{
	Count(TEXT("ToggleFinishMode"));
	if (Mode == EGLToolMode::Build)
	{
		SetState(State == EGLBuildState::Finish ? EGLBuildState::Place : EGLBuildState::Finish);
	}
}

void UGLBuildModeComponent::CycleFinish(int32 Delta)
{
	Count(TEXT("CycleFinish"));
	if (State != EGLBuildState::Finish || View.FinishChoices.Num() == 0 || View.FinishRole.IsNone())
	{
		return;
	}
	const int32 At = FMath::Max(0, View.FinishChoices.IndexOfByKey(View.Finish));
	const int32 N = View.FinishChoices.Num();
	FinishByRole.Add(View.FinishRole, View.FinishChoices[((At + Delta) % N + N) % N]); // remembered per role
	SaveProfile();
	RefreshView();
}

void UGLBuildModeComponent::ToggleRemoveMode()
{
	Count(TEXT("ToggleRemoveMode"));
	if (Mode == EGLToolMode::Build)
	{
		SetState(State == EGLBuildState::Remove ? EGLBuildState::Place : EGLBuildState::Remove);
	}
}

void UGLBuildModeComponent::TogglePath()
{
	Count(TEXT("TogglePath"));
	if (State == EGLBuildState::Remove)
	{
		Path = Path == EGLSalvagePath::Careful ? EGLSalvagePath::Destructive : EGLSalvagePath::Careful;
		bHolding = bArmed = false;
	}
}

void UGLBuildModeComponent::BuildCamera(bool bPressed)
{
	if (bPressed)
	{
		Count(TEXT("BuildCamera"));
	}
	if (Mode != EGLToolMode::Build)
	{
		bBuildCamera = false;
		return;
	}
	if (CameraMode == EGLHoldMode::Hold)
	{
		bBuildCamera = bPressed;
	}
	else if (bPressed)
	{
		bBuildCamera = !bBuildCamera;
	}
}

void UGLBuildModeComponent::AdjustCameraHeight(float DeltaCm)
{
	Count(TEXT("CameraHeight"));
	if (bBuildCamera)
	{
		CameraHeightCm = FMath::Clamp(CameraHeightCm + DeltaCm, -300.f, 300.f); // bounded around Zenny: no free flight
	}
}

// ---- the view (computed every frame from canonical results) ----

bool UGLBuildModeComponent::Aim(FHitResult& OutHit, FVector& OutStart, FVector& OutEnd) const
{
	const APawn* Owner = Cast<APawn>(GetOwner());
	if (!Owner)
	{
		return false;
	}
	FVector Eye;
	FRotator ViewRot;
	// The player's view is the camera's (including the build camera): what the camera points at is what the commit
	// targets (CAMERA AIM == GAMEPLAY AIM). A pawn nobody controls (a test world) aims from its eyes.
	if (const AController* Controller = Owner->GetController())
	{
		Controller->GetPlayerViewPoint(Eye, ViewRot);
	}
	else
	{
		Owner->GetActorEyesViewPoint(Eye, ViewRot);
	}
	FCollisionQueryParams Params(TEXT("GLBuildAim"), false, Owner);
	if (Ghost)
	{
		Params.AddIgnoredActor(Ghost);
	}
	// Reach is measured from Zenny, but the ray starts at the (third-person) camera.
	const double Extra = FVector::Dist(Eye, Owner->GetActorLocation());
	OutStart = Eye;
	OutEnd = Eye + ViewRot.Vector() * (ReachCm + Extra);
	return GetWorld()->LineTraceSingleByChannel(OutHit, OutStart, OutEnd, ECC_Visibility, Params);
}

void UGLBuildModeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double TickStart = FPlatformTime::Seconds();
	ON_SCOPE_EXIT { LastTickMs = (FPlatformTime::Seconds() - TickStart) * 1000.0; };
	RefreshView();
	if (State == EGLBuildState::Remove)
	{
		// The confirmation belongs to one target and one prediction: anything else changing cancels it.
		const bool bSame = SameConfirmation(ConfirmTarget, ConfirmPrediction, View.RemoveTarget, View.Predicted);
		if (bHolding)
		{
			HoldElapsed = bSame ? HoldElapsed + DeltaTime : 0.f;
			bHolding = bSame;
			if (bHolding && HoldElapsed >= ConfirmHoldSeconds)
			{
				bHolding = false;
				CommitRemoval();
			}
		}
		if (bArmed)
		{
			ArmElapsed += DeltaTime;
			bArmed = bSame && ArmElapsed <= ConfirmArmSeconds;
		}
	}
	View.ConfirmProgress = bHolding ? FMath::Clamp(HoldElapsed / FMath::Max(0.01f, ConfirmHoldSeconds), 0.f, 1.f) : 0.f;
	View.bArmed = bArmed;
}

void UGLBuildModeComponent::RefreshView()
{
	View.Mode = Mode;
	View.State = State;
	View.YawStep = YawStep;
	View.Path = Path;
	View.EraFilter = EraFilter;
	View.BrowseCategory = BrowseCategoryIndex;
	View.BrowseIndex = BrowseIndex;
	View.bBuildCamera = bBuildCamera && Mode == EGLToolMode::Build;
	View.CameraHeightCm = CameraHeightCm;
	View.Piece = Piece;
	const FGLBuildPieceDef* PieceDef = GLContent::Get().Find<FGLBuildPieceDef>(Piece);
	View.PieceName = PieceDef ? PieceDef->DisplayName : FString();
	View.Vocabulary = PieceDef ? VocabularyOf(*PieceDef) : FString();
	const int32 CategoryIndex = CategoryIndexOf(Piece);
	View.Category = Catalog.IsValidIndex(CategoryIndex) ? Catalog[CategoryIndex].Id : NAME_None;
	View.CategoryName = Catalog.IsValidIndex(CategoryIndex) ? Catalog[CategoryIndex].DisplayName : FString();
	View.bHasCandidate = false;
	View.FinishTarget = View.RemoveTarget = 0;
	View.Predicted.Reset();
	UpdateClaim();
	if (Mode == EGLToolMode::None)
	{
		Status.Reset();
		View.bAimed = false;
		if (Highlighted.Num())
		{
			GetWorld()->GetSubsystem<UGLBuildingSubsystem>()->SetRemovalHighlight({});
			Highlighted.Reset();
		}
		return;
	}
	FHitResult Hit;
	View.bAimed = Aim(Hit, View.AimStart, View.AimEnd);
	View.AimPoint = View.bAimed ? FVector(Hit.ImpactPoint) : View.AimEnd;
	const FHitResult* Aimed = View.bAimed ? &Hit : nullptr;
	if (Mode != EGLToolMode::Build)
	{
		Status = FString::Printf(TEXT("[%s] shovel stroke at the aim point (T: next tool)"), ModeName(Mode));
		return;
	}
	UpdatePlacement(State == EGLBuildState::Place ? Aimed : nullptr);
	UpdateFinish(State == EGLBuildState::Finish ? Aimed : nullptr);
	UpdateRemoval(State == EGLBuildState::Remove ? Aimed : nullptr);
}

void UGLBuildModeComponent::UpdateClaim()
{
	View.bShowClaim = View.bInsideClaim = false;
	View.ClaimAreas.Reset();
	const UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
	if (Mode != EGLToolMode::Build || !Building || !GetOwner())
	{
		return;
	}
	const FVector2D At(GetOwner()->GetActorLocation());
	for (const FGLClaim& Claim : Building->Claims())
	{
		bool bNear = false;
		for (const FGLClaimArea& Area : Claim.Areas)
		{
			bNear |= FVector2D::Distance(At, Area.Centre) <= Area.RadiusCm + 1000.0; // inside, or within 10 m of its edge
		}
		if (bNear)
		{
			View.bShowClaim = true;
			View.bInsideClaim = Claim.Contains(At);
			View.ClaimAreas = Claim.Areas;
			return;
		}
	}
}

void UGLBuildModeComponent::UpdatePlacement(const FHitResult* Hit)
{
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	const FGLBuildPieceDef* PieceDef = GLContent::Get().Find<FGLBuildPieceDef>(Piece);
	if (State != EGLBuildState::Place)
	{
		ClearGhost();
		Status = State == EGLBuildState::Browse ? TEXT("[BUILD] choose a piece") : Status;
		return;
	}
	if (!PieceDef || !Building)
	{
		ClearGhost();
		Status = TEXT("[BUILD] nothing to build");
		return;
	}
	if (!Hit || !Building->Snap(Piece, Hit->ImpactPoint, YawStep, View.Candidate, &View.Snap, View.AimEnd - View.AimStart))
	{
		ClearGhost();
		View.Reason = TEXT("Aim at the ground or a free socket");
		Status = FString::Printf(TEXT("[BUILD] %s - %s"), *PieceDef->DisplayName, *View.Reason);
		return;
	}
	View.bHasCandidate = true;
	View.Check = Building->Check(GetOwner(), View.Candidate);
	View.Cost = Building->CostView(GetOwner(), View.Candidate.Location, PieceDef->Cost);
	FString Blocking;
	if (const FGLStructurePartRuntime* Part = GetWorld()->GetSubsystem<UGLStructureSubsystem>()->FindPlayerPiece(View.Check.BlockingPieceId))
	{
		const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Part->Piece.Def);
		Blocking = Def ? Def->DisplayName.ToLower() : FString();
	}
	View.Reason = GLBuildText::Explain(View.Check, Blocking);
	if (!Ghost)
	{
		Ghost = GetWorld()->SpawnActor<AGLBuildPiece>(View.Candidate.Location, FRotator::ZeroRotator);
	}
	if (Ghost)
	{
		const FGLPlacedPiece& Shown = Ghost->GetPiece();
		if (Shown.Def != View.Candidate.Def || Shown.YawStep != View.Candidate.YawStep || !Shown.Location.Equals(View.Candidate.Location, 0.1))
		{
			Ghost->Setup(View.Candidate, true);
		}
		Ghost->SetGhostPreview(View.Check.Preview); // the commit's own check: GREEN / YELLOW / RED
	}
	Status = FString::Printf(TEXT("[BUILD] %s %s %s: %s"), *PieceDef->DisplayName, *GLBuildText::StateIcon(View.Check.Preview),
		*GLBuildText::StateWord(View.Check.Preview), *View.Reason);
}

void UGLBuildModeComponent::UpdateFinish(const FHitResult* Hit)
{
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	View.FinishChoices.Reset();
	View.Finish = NAME_None;
	View.FinishRole = NAME_None;
	View.FinishCheck = FGLInstallCheck();
	View.FinishCost = FGLCostView();
	if (State != EGLBuildState::Finish || !Hit)
	{
		return;
	}
	const int32 Target = Building->PieceIdAt(*Hit);
	const FGLStructurePartRuntime* Part = Target ? GetWorld()->GetSubsystem<UGLStructureSubsystem>()->FindPlayerPiece(Target) : nullptr;
	const FGLBuildPieceDef* Def = Part ? GLContent::Get().Find<FGLBuildPieceDef>(Part->Piece.Def) : nullptr;
	if (!Def || Part->State != EGLStructurePartState::Intact)
	{
		Status = TEXT("[FINISH] aim at a frame");
		return;
	}
	View.FinishTarget = Target;
	View.FinishRole = Def->Role;
	View.FinishChoices = Building->FinishesFor(Target);
	const FName* Remembered = FinishByRole.Find(Def->Role);
	View.Finish = Remembered && View.FinishChoices.Contains(*Remembered) ? *Remembered : (View.FinishChoices.Num() ? View.FinishChoices[0] : NAME_None);
	if (View.Finish.IsNone())
	{
		Status = FString::Printf(TEXT("[FINISH] %s: no finish fits (complete as built, or already finished)"), *Def->DisplayName);
		return;
	}
	View.FinishCheck = Building->CheckInstall(GetOwner(), Target, View.Finish);
	View.FinishCost = Building->CostView(GetOwner(), Part->Piece.Location, GLContent::Get().Find<FGLFinishDef>(View.Finish)->Cost);
	Status = FString::Printf(TEXT("[FINISH] %s -> %s (%s)"), *Def->DisplayName, *GLContent::Get().Find<FGLFinishDef>(View.Finish)->DisplayName, *GLBuildText::Cost(View.FinishCost));
}

void UGLBuildModeComponent::UpdateRemoval(const FHitResult* Hit)
{
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	View.Yield.Reset();
	View.YieldCareful.Reset();
	View.YieldDestructive.Reset();
	View.bNeedsConfirm = false;
	View.bYieldFits = true;
	View.bStorageNotEmpty = false;
	TArray<int32> Now;
	if (State == EGLBuildState::Remove && Hit)
	{
		View.RemoveTarget = Building->PieceIdAt(*Hit);
		const FGLStructurePartRuntime* Part = View.RemoveTarget ? GetWorld()->GetSubsystem<UGLStructureSubsystem>()->FindPlayerPiece(View.RemoveTarget) : nullptr;
		if (!Part || Part->State != EGLStructurePartState::Intact)
		{
			View.RemoveTarget = 0;
		}
		else
		{
			Now = Building->PreviewRemoval(View.RemoveTarget); // the commit's own function: what is shown is what will fall
			View.Predicted = Now;
			View.bNeedsConfirm = Now.Num() > 0; // collateral collapse, not the piece's importance, decides
			View.Yield = Building->RemovalYield(View.RemoveTarget, Path);
			View.YieldCareful = Building->RemovalYield(View.RemoveTarget, EGLSalvagePath::Careful);
			View.YieldDestructive = Building->RemovalYield(View.RemoveTarget, EGLSalvagePath::Destructive);
			View.bYieldFits = Building->RemovalYieldFits(GetOwner(), View.RemoveTarget, Path);
			View.bStorageNotEmpty = !Part->Contents.IsEmpty();
		}
	}
	if (Now != Highlighted)
	{
		Building->SetRemovalHighlight(Now);
		Highlighted = Now;
	}
	if (State == EGLBuildState::Remove)
	{
		Status = View.RemoveTarget
			? FString::Printf(TEXT("[REMOVE %s] %s; recovers %s"), Path == EGLSalvagePath::Careful ? TEXT("careful") : TEXT("smash"),
				View.Predicted.Num() ? *FString::Printf(TEXT("brings down %d piece(s)"), View.Predicted.Num()) : TEXT("nothing else falls"), *GLBuildText::Items(View.Yield))
			: FString(TEXT("[REMOVE] aim at one of your pieces"));
	}
}

// ---- the primary action: one meaning per state ----

void UGLBuildModeComponent::Primary()
{
	Count(TEXT("Primary"));
	if (Mode == EGLToolMode::None)
	{
		return;
	}
	if (Mode != EGLToolMode::Build)
	{
		const FName Terraform = TerraformFor(Mode);
		FHitResult Hit;
		FVector Start, End;
		if (!Terraform.IsNone() && Aim(Hit, Start, End))
		{
			GetWorld()->GetSubsystem<UGLTerrainSubsystem>()->Terraform(GetOwner(), Terraform, FVector2D(Hit.ImpactPoint));
		}
		return;
	}
	RefreshView(); // the commit re-aims and re-checks now: nothing shown earlier is trusted
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	switch (State)
	{
	case EGLBuildState::Browse:
		BrowseSelect();
		return;
	case EGLBuildState::Place:
		if (View.bHasCandidate && Building->Place(GetOwner(), View.Candidate).IsAllowed())
		{
			Recents.Remove(Piece);
			Recents.Insert(Piece, 0);
			Recents.SetNum(FMath::Min(Recents.Num(), RecentCount));
			SaveProfile();
		}
		return;
	case EGLBuildState::Finish:
		if (View.FinishTarget && !View.Finish.IsNone())
		{
			Building->InstallFinish(GetOwner(), View.FinishTarget, View.Finish);
		}
		return;
	case EGLBuildState::Remove:
		if (!View.RemoveTarget)
		{
			return;
		}
		if (!View.bNeedsConfirm)
		{
			ConfirmTarget = View.RemoveTarget;
			CommitRemoval(); // a safe removal is one click
		}
		else if (ConfirmMode == EGLHoldMode::Hold)
		{
			bHolding = true;
			HoldElapsed = 0.f;
			ConfirmTarget = View.RemoveTarget;
			ConfirmPrediction = View.Predicted;
		}
		else if (bArmed && SameConfirmation(ConfirmTarget, ConfirmPrediction, View.RemoveTarget, View.Predicted))
		{
			bArmed = false;
			CommitRemoval();
		}
		else
		{
			bArmed = true;
			ArmElapsed = 0.f;
			ConfirmTarget = View.RemoveTarget;
			ConfirmPrediction = View.Predicted;
		}
		return;
	}
}

void UGLBuildModeComponent::PrimaryReleased()
{
	if (bHolding)
	{
		bHolding = false; // released early: nothing happens
		HoldElapsed = 0.f;
	}
}

void UGLBuildModeComponent::CommitRemoval()
{
	RefreshView();
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	// Only the piece the confirmation was for, with the prediction it was given (re-derived now by the canonical function).
	if (State != EGLBuildState::Remove || !View.RemoveTarget || View.RemoveTarget != ConfirmTarget
		|| (View.bNeedsConfirm && View.Predicted != ConfirmPrediction))
	{
		return;
	}
	const FGLDemolishResult Result = Building->Remove(GetOwner(), View.RemoveTarget, Path);
	if (!Result.IsDone())
	{
		Status = Result.Refusal == EGLDemolishRefusal::StorageNotEmpty ? TEXT("[REMOVE] empty it first") : TEXT("[REMOVE] no room for what it gives back");
	}
	ConfirmTarget = 0;
	ConfirmPrediction.Reset();
}

void UGLBuildModeComponent::ClearGhost()
{
	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}
}

namespace GLBuildModeSettings
{
	// Accessibility (ADR-0040): the build camera and the removal confirmation are hold by default; either can be a toggle.
	// Saved in the playtest profile, never in a world save.
	void SetHoldMode(UWorld* World, const TArray<FString>& Args, bool bCamera)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		UGLBuildModeComponent* Build = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UGLBuildModeComponent>() : nullptr;
		const TCHAR* Command = bCamera ? TEXT("gl.Build.CameraMode") : TEXT("gl.Build.ConfirmMode");
		if (!Build || Args.Num() != 1 || (Args[0] != TEXT("hold") && Args[0] != TEXT("toggle")))
		{
			UE_LOG(LogGridlands, Display, TEXT("%s: usage: %s hold|toggle"), Command, Command);
			return;
		}
		const EGLHoldMode Mode = Args[0] == TEXT("toggle") ? EGLHoldMode::Toggle : EGLHoldMode::Hold;
		bCamera ? Build->SetCameraMode(Mode) : Build->SetConfirmMode(Mode);
		UE_LOG(LogGridlands, Display, TEXT("%s: %s"), Command, *Args[0]);
	}

	FAutoConsoleCommandWithWorldAndArgs CameraModeCommand(TEXT("gl.Build.CameraMode"),
		TEXT("Build camera (Alt): hold (default) or toggle."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World) { SetHoldMode(World, Args, true); }));
	FAutoConsoleCommandWithWorldAndArgs ConfirmModeCommand(TEXT("gl.Build.ConfirmMode"),
		TEXT("Removal that brings other pieces down: hold (default) or toggle (press twice)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World) { SetHoldMode(World, Args, false); }));
}
