#include "Building/GLBuildModeComponent.h"

#include "Building/GLBuildPiece.h"
#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
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
}

UGLBuildModeComponent::UGLBuildModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGLBuildModeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearGhost();
	Super::EndPlay(Reason);
}

FName UGLBuildModeComponent::GetSelectedPiece() const
{
	return Pieces.IsValidIndex(Selected) ? Pieces[Selected] : NAME_None;
}

void UGLBuildModeComponent::ToggleBuild()
{
	Mode = Mode == EGLToolMode::Build ? EGLToolMode::None : EGLToolMode::Build;
	// Offer every piece in the data, sorted; the building rules refuse what Zenny doesn't know.
	Pieces.Reset();
	GLContent::Get().ForEachEntry([this](const FGLContentEntry& Entry)
	{
		const FGLBuildPieceDef* Def = Entry.Definition.GetPtr<FGLBuildPieceDef>();
		if (Entry.Kind == TEXT("buildpiece") && Def && Def->Buildable) // world-only pieces are never offered (P6)
		{
			Pieces.Add(Entry.Id);
		}
	});
	Pieces.Sort(FNameLexicalLess());
	Selected = FMath::Clamp(Selected, 0, FMath::Max(0, Pieces.Num() - 1));
	if (Mode != EGLToolMode::Build)
	{
		ClearGhost();
	}
}

void UGLBuildModeComponent::CycleTerraform()
{
	ClearGhost();
	switch (Mode)
	{
	case EGLToolMode::Dig: Mode = EGLToolMode::Raise; break;
	case EGLToolMode::Raise: Mode = EGLToolMode::Flatten; break;
	case EGLToolMode::Flatten: Mode = EGLToolMode::None; break;
	default: Mode = EGLToolMode::Dig; break;
	}
}

void UGLBuildModeComponent::CyclePiece(int32 Direction)
{
	if (Mode == EGLToolMode::Build && Pieces.Num() > 0)
	{
		Selected = (Selected + Direction + Pieces.Num()) % Pieces.Num();
		ClearGhost();
	}
}

void UGLBuildModeComponent::Rotate()
{
	YawStep = GLStructureRules::NormalizeYawStep(YawStep + GLStructureRules::QuarterTurnSteps);
	ClearGhost();
}

void UGLBuildModeComponent::RotateFine()
{
	YawStep = GLStructureRules::NormalizeYawStep(YawStep + GLStructureRules::YawStepFromDegrees(15.0));
	ClearGhost();
}

bool UGLBuildModeComponent::Aim(FHitResult& OutHit) const
{
	const APawn* Owner = Cast<APawn>(GetOwner());
	const AController* Controller = Owner ? Owner->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Controller->GetPlayerViewPoint(Eye, View);
	FCollisionQueryParams Params(TEXT("GLBuildAim"), false, Owner);
	if (Ghost)
	{
		Params.AddIgnoredActor(Ghost);
	}
	// Reach is measured from Zenny, but the ray starts at the (third-person) camera.
	const double Extra = FVector::Dist(Eye, Owner->GetActorLocation());
	return GetWorld()->LineTraceSingleByChannel(OutHit, Eye, Eye + View.Vector() * (ReachCm + Extra), ECC_Visibility, Params);
}

void UGLBuildModeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePreview();
}

void UGLBuildModeComponent::UpdatePreview()
{
	bHasCandidate = false;
	if (Mode == EGLToolMode::None)
	{
		Status.Reset();
		UpdateRemovalPreview(nullptr);
		return;
	}
	FHitResult Hit;
	const bool bAimed = Aim(Hit);
	UpdateRemovalPreview(bAimed && Mode == EGLToolMode::Build ? &Hit : nullptr);
	if (Mode != EGLToolMode::Build)
	{
		Status = FString::Printf(TEXT("[%s] shovel stroke at the aim point (T: next tool)"), ModeName(Mode));
		return;
	}
	const FName Def = GetSelectedPiece();
	const FGLBuildPieceDef* PieceDef = GLContent::Get().Find<FGLBuildPieceDef>(Def);
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	if (!PieceDef || !Building)
	{
		Status = TEXT("[BUILD] nothing to build");
		return;
	}
	FString Cost;
	for (const FGLItemStackDef& Stack : PieceDef->Cost)
	{
		Cost += FString::Printf(TEXT(" %dx %s"), Stack.Count, *Stack.Item.ToString().RightChop(Stack.Item.ToString().Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1));
	}
	if (!bAimed || !Building->Snap(Def, Hit.ImpactPoint, YawStep, Candidate))
	{
		ClearGhost();
		Status = FString::Printf(TEXT("[BUILD] %s (%s) - aim at the ground or a free socket"), *PieceDef->DisplayName, *Cost.TrimStart());
		return;
	}
	bHasCandidate = true;
	const FGLBuildCheck Check = Building->Check(GetOwner(), Candidate);
	if (!Ghost)
	{
		Ghost = GetWorld()->SpawnActor<AGLBuildPiece>(Candidate.Location, FRotator::ZeroRotator);
	}
	if (Ghost)
	{
		const FGLPlacedPiece& Shown = Ghost->GetPiece();
		if (Shown.Def != Candidate.Def || Shown.YawStep != Candidate.YawStep || !Shown.Location.Equals(Candidate.Location, 0.1))
		{
			Ghost->Setup(Candidate, true);
		}
		Ghost->SetGhostPreview(Check.Preview); // the commit's own check: GREEN / YELLOW / RED
	}
	const TCHAR* Colour = Check.Preview == EGLPreview::Green ? TEXT("GREEN") : Check.Preview == EGLPreview::Yellow ? TEXT("YELLOW: at its limit") : TEXT("RED");
	Status = Check.IsAllowed()
		? FString::Printf(TEXT("[BUILD] %s (%s) - %s, support %.2f, yaw %.1f"), *PieceDef->DisplayName, *Cost.TrimStart(), Colour, Check.Support,
			GLStructureRules::YawDegrees(Candidate.YawStep))
		: FString::Printf(TEXT("[BUILD] %s (%s) - %s: %s"), *PieceDef->DisplayName, *Cost.TrimStart(), Colour, *Check.Reason);
}

void UGLBuildModeComponent::Primary()
{
	if (Mode == EGLToolMode::Build)
	{
		UpdatePreview();
		if (bHasCandidate)
		{
			GetWorld()->GetSubsystem<UGLBuildingSubsystem>()->Place(GetOwner(), Candidate);
		}
		return;
	}
	const FName Terraform = TerraformFor(Mode);
	FHitResult Hit;
	if (!Terraform.IsNone() && Aim(Hit))
	{
		GetWorld()->GetSubsystem<UGLTerrainSubsystem>()->Terraform(GetOwner(), Terraform, FVector2D(Hit.ImpactPoint));
	}
}

void UGLBuildModeComponent::Demolish()
{
	FHitResult Hit;
	if (Mode != EGLToolMode::Build || !Aim(Hit))
	{
		return;
	}
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	if (const int32 Id = Building->PieceIdAt(Hit))
	{
		const FGLDemolishResult Result = Building->Dismantle(GetOwner(), Id);
		if (!Result.IsDone())
		{
			Status = Result.Refusal == EGLDemolishRefusal::StorageNotEmpty ? TEXT("[BUILD] empty it first") : TEXT("[BUILD] no room for what it gives back");
		}
	}
}

void UGLBuildModeComponent::Smash()
{
	FHitResult Hit;
	if (Mode != EGLToolMode::Build || !Aim(Hit))
	{
		return;
	}
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	if (const int32 Id = Building->PieceIdAt(Hit))
	{
		Building->Smash(GetOwner(), Id);
	}
}

void UGLBuildModeComponent::InstallFinish()
{
	FHitResult Hit;
	if (Mode != EGLToolMode::Build || !Aim(Hit))
	{
		return;
	}
	UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>();
	const int32 Id = Building->PieceIdAt(Hit);
	if (!Id)
	{
		return;
	}
	TArray<FName> Finishes;
	GLContent::Get().ForEachEntry([&Finishes](const FGLContentEntry& Entry)
	{
		if (Entry.Kind == TEXT("finish"))
		{
			Finishes.Add(Entry.Id);
		}
	});
	Finishes.Sort(FNameLexicalLess());
	for (const FName& Finish : Finishes)
	{
		if (Building->CheckInstall(GetOwner(), Id, Finish).IsAllowed())
		{
			Building->InstallFinish(GetOwner(), Id, Finish);
			return;
		}
	}
	Status = TEXT("[BUILD] no finish fits (or no materials)");
}

void UGLBuildModeComponent::UpdateRemovalPreview(const FHitResult* Hit)
{
	TArray<int32> Now;
	UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
	if (Hit && Building)
	{
		if (const int32 Id = Building->PieceIdAt(*Hit))
		{
			Now = Building->PreviewRemoval(Id); // the commit's own function: what is shown is what will fall
		}
	}
	if (Now == Highlighted)
	{
		return;
	}
	if (Building)
	{
		Building->SetRemovalHighlight(Now); // instanced pieces through their batch, actor-presented ones on their actor
	}
	Highlighted = Now;
}

void UGLBuildModeComponent::ClearGhost()
{
	UpdateRemovalPreview(nullptr);
	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}
}
