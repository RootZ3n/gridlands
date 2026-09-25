#include "Puzzle/GLPuzzleSite.h"

#include "Components/StaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Puzzle/GLPuzzleSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AGLPuzzleSite::AGLPuzzleSite()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.1f)); // a waist-high stand
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

bool AGLPuzzleSite::Setup(FName InPuzzleId)
{
	if (!GLContent::Get().Find<FGLPuzzleDef>(InPuzzleId))
	{
		return false;
	}
	PuzzleId = InPuzzleId;
	return true;
}

void AGLPuzzleSite::GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const
{
	const FGLPuzzleDef* Puzzle = GLContent::Get().Find<FGLPuzzleDef>(PuzzleId);
	const UGLPuzzleSubsystem* Puzzles = GetWorld() ? GetWorld()->GetSubsystem<UGLPuzzleSubsystem>() : nullptr;
	if (!Puzzle || !Puzzles || Puzzles->IsSolved(PuzzleId) || Puzzle->Answer.Mode != TEXT("PRESENT"))
	{
		return;
	}
	const UGLInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	const FGLItemDef* Item = GLContent::Get().Find<FGLItemDef>(Puzzle->Answer.Item);
	FGLInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
	Option.Verb = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Present"));
	Option.bEnabled = Inventory && Inventory->CountOf(Puzzle->Answer.Item) > 0;
	// The label never reveals the answer before Zenny holds it.
	Option.Label = Option.bEnabled && Item ? FText::Format(NSLOCTEXT("Gridlands", "Present", "Place {0}"), FText::FromString(Item->DisplayName))
		: NSLOCTEXT("Gridlands", "PresentUnknown", "Place something here");
	if (!Option.bEnabled)
	{
		Option.DisabledReason = NSLOCTEXT("Gridlands", "NothingFits", "Nothing you carry fits here yet");
	}
}

bool AGLPuzzleSite::Interact(AActor* Interactor, FGameplayTag Verb)
{
	const FGLPuzzleDef* Puzzle = GLContent::Get().Find<FGLPuzzleDef>(PuzzleId);
	UGLPuzzleSubsystem* Puzzles = GetWorld() ? GetWorld()->GetSubsystem<UGLPuzzleSubsystem>() : nullptr;
	UGLInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	if (!Puzzle || !Puzzles || !Inventory || Puzzles->IsSolved(PuzzleId) || Puzzle->Answer.Mode != TEXT("PRESENT")
		|| !Inventory->RemoveItem(Puzzle->Answer.Item, 1))
	{
		return false;
	}
	return Puzzles->Solve(PuzzleId); // the answer is the act of placing it
}
