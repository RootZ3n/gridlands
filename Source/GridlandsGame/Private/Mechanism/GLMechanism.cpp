#include "Mechanism/GLMechanism.h"

#include "Components/StaticMeshComponent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Mechanism/GLMechanismSubsystem.h"

AGLMechanism::AGLMechanism()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

UStaticMeshComponent* AGLMechanism::AddCube(const FVector& Local, const FVector& Scale, const FLinearColor& Colour)
{
	UStaticMeshComponent* Cube = NewObject<UStaticMeshComponent>(this);
	Cube->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Cube->SetupAttachment(GetRootComponent());
	Cube->SetCollisionEnabled(ECollisionEnabled::NoCollision); // presentation only: the record is the gameplay
	Cube->SetCanEverAffectNavigation(false);
	Cube->SetUsingAbsoluteRotation(true);
	Cube->SetUsingAbsoluteLocation(true); // world placement from the record (no inherited yaw)
	Cube->SetRelativeLocation(GetActorLocation() + Local);
	Cube->SetRelativeScale3D(Scale);
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Paint = Cube->CreateDynamicMaterialInstance(0, Shape))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), Colour);
		}
	}
	Cube->RegisterComponent();
	return Cube;
}

void AGLMechanism::Setup(const FGLMechanismRecord& Record)
{
	PlacementId = Record.Placement;
	const FGLMechanismDef* Def = UGLMechanismSubsystem::DefOf(Record);
	if (!Def)
	{
		return;
	}
	if (Def->CanOperate())
	{
		AddCube(FVector(0, 0, 60), FVector(0.4, 0.2, 1.2), FLinearColor(0.9f, 0.7f, 0.1f)); // the control panel
	}
	if (Def->HasNeutralize())
	{
		// A cage frame over the box: four bars and a lid, 3 m above the floor while raised.
		const FBox Box = UGLMechanismSubsystem::NeutralizeBox(Record);
		const FVector Extent = Box.GetExtent();
		CageBase = Box.GetCenter() - FVector(0, 0, Extent.Z) - GetActorLocation();
		CageHeight = Extent.Z * 2.0;
		for (const FVector2D& Corner : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) })
		{
			CagePieces.Add(AddCube(FVector::ZeroVector, FVector(0.1, 0.1, CageHeight / 100.0), FLinearColor(0.2f, 0.2f, 0.25f)));
			CagePieces.Last()->ComponentTags.Add(*FString::Printf(TEXT("%f,%f"), Corner.X * Extent.X, Corner.Y * Extent.Y));
		}
		CagePieces.Add(AddCube(FVector::ZeroVector, FVector(Extent.X * 2.0 / 100.0, Extent.Y * 2.0 / 100.0, 0.1), FLinearColor(0.2f, 0.2f, 0.25f)));
		CagePieces.Last()->ComponentTags.Add(TEXT("lid"));
		DropSeconds = Def->Neutralize.PresentSeconds;
	}
	if (Def->HasAmbient())
	{
		Fan = AddCube(FVector(0, 0, 150), FVector(1.2, 0.2, 1.2), FLinearColor(0.5f, 0.6f, 0.7f));
	}
	PresentState(Record);
	CageLift = CageTarget; // made at rest
}

void AGLMechanism::PresentState(const FGLMechanismRecord& Record)
{
	const FGLMechanismDef* Def = UGLMechanismSubsystem::DefOf(Record);
	if (Def && Def->HasNeutralize())
	{
		CageTarget = Record.State == Def->Neutralize.InState ? 0.0 : 300.0;
		const bool bLive = GetWorld() && GetWorld()->GetTimeSeconds() - Record.SwitchedAt < 1e6;
		if (!bLive || DropSeconds <= 0.0)
		{
			CageLift = CageTarget; // a restored state is shown as it is: nothing is replayed
		}
	}
	bFanSpinning = Def && Def->HasAmbient() && Def->Ambient.ActiveIn.Contains(Record.State);
	Tick(0.f);
}

void AGLMechanism::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CagePieces.Num() > 0)
	{
		if (CageLift != CageTarget)
		{
			const double Step = DropSeconds > 0.0 ? 300.0 * DeltaSeconds / DropSeconds : 1e9;
			CageLift = CageLift > CageTarget ? FMath::Max(CageTarget, CageLift - Step) : FMath::Min(CageTarget, CageLift + Step);
		}
		for (UStaticMeshComponent* Piece : CagePieces)
		{
			const FString Tag = Piece->ComponentTags.Num() ? Piece->ComponentTags[0].ToString() : FString();
			FString X, Y;
			if (Tag == TEXT("lid"))
			{
				Piece->SetRelativeLocation(GetActorLocation() + CageBase + FVector(0, 0, CageLift + CageHeight));
			}
			else if (Tag.Split(TEXT(","), &X, &Y))
			{
				Piece->SetRelativeLocation(GetActorLocation() + CageBase + FVector(FCString::Atod(*X), FCString::Atod(*Y), CageLift + CageHeight * 0.5));
			}
		}
	}
	if (Fan && bFanSpinning)
	{
		Fan->AddWorldRotation(FRotator(0.0, 0.0, 720.0 * DeltaSeconds));
	}
}

void AGLMechanism::Retire()
{
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
}
