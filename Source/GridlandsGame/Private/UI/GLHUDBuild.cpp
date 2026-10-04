// P12 build mode HUD (ADR-0040): drawn with Canvas, no assets. It only reads FGLBuildView (computed from canonical results
// by UGLBuildModeComponent) and decides nothing.

#include "UI/GLHUD.h"

#include "Building/GLBuildModeComponent.h"
#include "Building/GLBuildText.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"

namespace
{
	TAutoConsoleVariable<float> CVarBuildTextScale(TEXT("gl.Build.TextScale"), 1.f, TEXT("P12 build HUD text scale (accessibility)."));

	const TCHAR* StateLabel(EGLBuildState State)
	{
		switch (State)
		{
		case EGLBuildState::Browse: return TEXT("BROWSE");
		case EGLBuildState::Finish: return TEXT("FINISH");
		case EGLBuildState::Remove: return TEXT("REMOVE");
		default: return TEXT("PLACE");
		}
	}

	FString PieceName(UWorld* World, int32 Id)
	{
		const UGLStructureSubsystem* Structures = World ? World->GetSubsystem<UGLStructureSubsystem>() : nullptr;
		const FGLStructurePartRuntime* Part = Structures ? Structures->FindPlayerPiece(Id) : nullptr;
		const FGLBuildPieceDef* Def = Part ? GLContent::Get().Find<FGLBuildPieceDef>(Part->Piece.Def) : nullptr;
		return Def ? Def->DisplayName : FString(TEXT("piece"));
	}

	FString FinishName(FName Id)
	{
		const FGLFinishDef* Def = GLContent::Get().Find<FGLFinishDef>(Id);
		return Def ? Def->DisplayName : Id.ToString();
	}

	const FLinearColor Ink(0.97f, 0.97f, 0.97f);
	const FLinearColor Dim(0.7f, 0.7f, 0.72f);
	const FLinearColor Accent(1.f, 0.85f, 0.3f);
	const FLinearColor Marker(0.2f, 0.9f, 1.f);
}

void AGLHUD::Line(const FString& Text, const FLinearColor& Colour, float X, float& Y, float Scale)
{
	float W = 0.f, H = 0.f;
	Canvas->TextSize(GEngine->GetMediumFont(), Text, W, H, Scale, Scale);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 8.f, Y - 2.f, W + 16.f, H + 4.f); // contrast backing
	DrawText(Text, Colour, X, Y, GEngine->GetMediumFont(), Scale);
	Y += H + 6.f;
}

void AGLHUD::DrawWorldMarkers(const FGLBuildView& View, float Scale)
{
	// Claim: a ring at the edge of each area (provisional size; never labelled a maximum; cell boundaries are never drawn).
	if (View.bShowClaim)
	{
		const UGLTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UGLTerrainSubsystem>();
		for (const FGLClaimArea& Area : View.ClaimAreas)
		{
			FVector Previous;
			bool bPrevious = false;
			for (int32 I = 0; I <= 64; ++I)
			{
				const double A = 2.0 * PI * I / 64.0;
				const FVector2D P = Area.Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Area.RadiusCm;
				const FVector World(P, (Terrain ? Terrain->HeightAt(P) : 0.0) + 20.0);
				const FVector Screen = Project(World);
				const bool bVisible = Screen.Z > 0.f;
				if (bVisible && bPrevious)
				{
					DrawLine(Previous.X, Previous.Y, Screen.X, Screen.Y, FLinearColor(0.4f, 0.8f, 1.f, 0.8f), 2.f);
				}
				Previous = Screen;
				bPrevious = bVisible;
			}
		}
	}
	// Snap: the target socket (from the snap the commit uses), a line to the candidate, and what meets what.
	if (View.State == EGLBuildState::Place && View.bHasCandidate && View.Snap.bSnapped)
	{
		const FVector Target = Project(View.Snap.TargetLocation);
		const FVector Centre = Project(View.Candidate.Location + FVector(0, 0, 50));
		if (Target.Z > 0.f)
		{
			DrawRect(Marker, Target.X - 7.f, Target.Y - 7.f, 14.f, 14.f);
			DrawRect(FLinearColor::Black, Target.X - 3.f, Target.Y - 3.f, 6.f, 6.f);
			if (Centre.Z > 0.f)
			{
				DrawLine(Target.X, Target.Y, Centre.X, Centre.Y, Marker, 2.f);
			}
			DrawText(FString::Printf(TEXT("%s -> %s"), *View.Snap.OwnSocket.ToString(), *View.Snap.TargetSocket.ToString()), Marker,
				Target.X + 10.f, Target.Y - 10.f, GEngine->GetSmallFont(), Scale);
		}
	}
	// The collapse confirmation: a ring filling around the crosshair while held.
	if (View.ConfirmProgress > 0.f)
	{
		const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f, R = 26.f;
		const int32 Segments = FMath::Max(1, FMath::RoundToInt(48.f * View.ConfirmProgress));
		for (int32 I = 0; I < Segments; ++I)
		{
			const float A0 = -PI / 2 + 2 * PI * I / 48.f, A1 = -PI / 2 + 2 * PI * (I + 1) / 48.f;
			DrawLine(CX + R * FMath::Cos(A0), CY + R * FMath::Sin(A0), CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1), FLinearColor(0.95f, 0.15f, 0.1f), 4.f);
		}
	}
}

void AGLHUD::DrawBuildMode(const APawn* Zenny)
{
	const UGLBuildModeComponent* Build = Zenny ? Zenny->FindComponentByClass<UGLBuildModeComponent>() : nullptr;
	if (!Build || Build->GetMode() == EGLToolMode::None)
	{
		return;
	}
	const float Scale = 1.2f * CVarBuildTextScale.GetValueOnGameThread() * FMath::Max(0.75f, Canvas->ClipY / 1080.f);
	if (Build->GetMode() != EGLToolMode::Build)
	{
		float Y = Canvas->ClipY - 60.f * Scale;
		Line(Build->StatusLine(), Accent, 24.f, Y, Scale);
		return;
	}
	const FGLBuildView& View = Build->GetView();
	DrawWorldMarkers(View, Scale);
	const float X = 24.f;
	float Y = Canvas->ClipY * 0.58f;

	// The build bar: where you are and what you hold.
	FString Head = FString::Printf(TEXT("[BUILD - %s]  %s > %s  (%s)   yaw %.1f deg"), StateLabel(View.State), *View.CategoryName, *View.PieceName,
		*View.Vocabulary, GLStructureRules::YawDegrees(View.YawStep));
	if (View.State == EGLBuildState::Place && View.bHasCandidate)
	{
		Head += View.Snap.bSnapped
			? (View.Snap.bYawFromData ? FString::Printf(TEXT("   SNAPPED (angle from the socket: %.1f deg)"), GLStructureRules::YawDegrees(View.Candidate.YawStep)) : FString(TEXT("   SNAPPED")))
			: FString(TEXT("   FREE"));
	}
	if (View.bBuildCamera)
	{
		Head += TEXT("   [build camera]");
	}
	Line(Head, Accent, X, Y, Scale);
	if (View.bShowClaim)
	{
		Line(View.bInsideClaim ? FString::Printf(TEXT("Base area (%.0f m, provisional): base storage supplies building here"), View.ClaimAreas.Num() ? View.ClaimAreas[0].RadiusCm / 100.0 : 0.0)
			: FString(TEXT("Near a base area: step inside for base storage")), Dim, X, Y, Scale);
	}

	switch (View.State)
	{
	case EGLBuildState::Place:
		if (View.bHasCandidate)
		{
			// The canonical check: a word, an icon and a colour (never colour alone), and why.
			Line(FString::Printf(TEXT("%s %s: %s"), *GLBuildText::StateIcon(View.Check.Preview), *GLBuildText::StateWord(View.Check.Preview), *View.Reason),
				GLBuildText::StateColour(View.Check.Preview), X, Y, Scale);
			Line(FString::Printf(TEXT("Cost: %s   %s"), *GLBuildText::Cost(View.Cost), View.Cost.bAffordable ? TEXT("[can afford]") : TEXT("[not enough]")), Ink, X, Y, Scale);
		}
		else
		{
			Line(View.Reason, Dim, X, Y, Scale);
		}
		Line(TEXT("LMB place   wheel piece   Shift+wheel category   Tab browse   Z/Shift+Z 90   C 15   Ctrl+wheel 2.5   Y finish   X remove   Alt camera"), Dim, X, Y, Scale * 0.8f);
		break;
	case EGLBuildState::Browse:
	{
		const TArray<FGLCatalogCategory>& Catalog = Build->GetCatalog();
		FString Tabs;
		for (int32 I = 0; I < Catalog.Num(); ++I)
		{
			Tabs += I == View.BrowseCategory ? FString::Printf(TEXT("[%s]  "), *Catalog[I].DisplayName) : Catalog[I].DisplayName + TEXT("  ");
		}
		Line(Tabs, Ink, X, Y, Scale);
		const TArray<FName> Pieces = Build->PiecesOf(View.BrowseCategory);
		for (int32 I = 0; I < Pieces.Num(); ++I)
		{
			const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Pieces[I]);
			const FGLEraDef* Era = Def ? GLContent::Get().Find<FGLEraDef>(Def->Era) : nullptr;
			Line(FString::Printf(TEXT("%s %s   (%s)"), I == View.BrowseIndex ? TEXT(">") : TEXT(" "), Def ? *Def->DisplayName : *Pieces[I].ToString(), Era ? *Era->DisplayName : TEXT("")),
				I == View.BrowseIndex ? Accent : Ink, X, Y, Scale);
		}
		FString Favs = TEXT("Favorites: ");
		for (int32 I = 0; I < Build->GetFavorites().Num(); ++I)
		{
			const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Build->GetFavorites()[I]);
			Favs += FString::Printf(TEXT("%d %s  "), I + 1, Def ? *Def->DisplayName : TEXT("-"));
		}
		Line(Favs, Dim, X, Y, Scale * 0.85f);
		FString Recent = TEXT("Recent: ");
		for (const FName& Id : Build->GetRecents())
		{
			const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Id);
			Recent += (Def ? Def->DisplayName : Id.ToString()) + TEXT("  ");
		}
		Line(Recent, Dim, X, Y, Scale * 0.85f);
		Line(TEXT("wheel piece   Shift+wheel category   LMB choose   Ctrl+1..8 pin favorite   Tab/Esc back"), Dim, X, Y, Scale * 0.8f);
		break;
	}
	case EGLBuildState::Finish:
		if (View.FinishTarget)
		{
			Line(FString::Printf(TEXT("Finish the %s:"), *PieceName(GetWorld(), View.FinishTarget)), Ink, X, Y, Scale);
			for (const FName& Choice : View.FinishChoices)
			{
				Line(FString::Printf(TEXT("%s %s"), Choice == View.Finish ? TEXT(">") : TEXT(" "), *FinishName(Choice)), Choice == View.Finish ? Accent : Ink, X, Y, Scale);
			}
			if (!View.Finish.IsNone())
			{
				Line(FString::Printf(TEXT("Cost: %s   %s"), *GLBuildText::Cost(View.FinishCost), View.FinishCheck.IsAllowed() ? TEXT("[can apply]") : *FString::Printf(TEXT("[%s]"), *View.FinishCheck.Reason)),
					View.FinishCheck.IsAllowed() ? Ink : GLBuildText::StateColour(EGLPreview::Red), X, Y, Scale);
			}
		}
		else
		{
			Line(Build->StatusLine(), Dim, X, Y, Scale);
		}
		Line(TEXT("LMB apply   wheel choose finish (remembered per form)   Y/Esc back"), Dim, X, Y, Scale * 0.8f);
		break;
	case EGLBuildState::Remove:
		if (View.RemoveTarget)
		{
			Line(FString::Printf(TEXT("Remove the %s: %s"), *PieceName(GetWorld(), View.RemoveTarget),
				View.Predicted.Num() ? *FString::Printf(TEXT("THESE %d piece(s) come down too (shown red)"), View.Predicted.Num()) : TEXT("nothing else falls")),
				View.Predicted.Num() ? GLBuildText::StateColour(EGLPreview::Red) : Ink, X, Y, Scale);
			Line(FString::Printf(TEXT("%s careful: %s"), View.Path == EGLSalvagePath::Careful ? TEXT(">") : TEXT(" "), *GLBuildText::Items(View.YieldCareful)),
				View.Path == EGLSalvagePath::Careful ? Accent : Ink, X, Y, Scale);
			Line(FString::Printf(TEXT("%s smash:   %s"), View.Path == EGLSalvagePath::Destructive ? TEXT(">") : TEXT(" "), *GLBuildText::Items(View.YieldDestructive)),
				View.Path == EGLSalvagePath::Destructive ? Accent : Ink, X, Y, Scale);
			if (View.bStorageNotEmpty)
			{
				Line(TEXT("It holds things: empty it first"), GLBuildText::StateColour(EGLPreview::Red), X, Y, Scale);
			}
			else if (!View.bYieldFits)
			{
				Line(TEXT("No room for what it gives back"), GLBuildText::StateColour(EGLPreview::Red), X, Y, Scale);
			}
			if (View.bNeedsConfirm)
			{
				Line(View.bArmed ? TEXT("Press LMB again to bring them down") : Build->ConfirmMode == EGLHoldMode::Toggle ? TEXT("Press LMB twice to bring them down")
					: TEXT("Hold LMB to bring them down"), GLBuildText::StateColour(EGLPreview::Yellow), X, Y, Scale);
			}
		}
		else
		{
			Line(Build->StatusLine(), Dim, X, Y, Scale);
		}
		Line(TEXT("LMB remove   N careful/smash   X/Esc back"), Dim, X, Y, Scale * 0.8f);
		break;
	}
}
