#include "UI/GLHUD.h"

#include "Building/GLBuildModeComponent.h"
#include "Character/GLCharacter.h"
#include "Engine/Font.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/GLInteractorComponent.h"
#include "UI/GLSubtitles.h"
#include "Combat/GLHealthComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "World/GLGridCells.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "World/GLStabilitySubsystem.h"

void AGLHUD::DrawHUD()
{
	Super::DrawHUD();
	const APawn* Zenny = GetOwningPawn();
	const UGLStabilitySubsystem* Stability = GetWorld() ? GetWorld()->GetSubsystem<UGLStabilitySubsystem>() : nullptr;
	if (!Zenny || !Stability || !Canvas)
	{
		return;
	}
	const double Interference = Stability->InterferenceAt(Zenny->GetActorLocation());

	// Static: a grey veil, flickering specks and scanlines, all growing with interference.
	if (Interference > 0.02)
	{
		DrawRect(FLinearColor(0.55f, 0.55f, 0.6f, 0.22f * static_cast<float>(Interference)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
	const int32 Specks = FMath::RoundToInt(Interference * 7000.0);
	for (int32 I = 0; I < Specks; ++I)
	{
		const float X = Noise.FRand() * Canvas->ClipX, Y = Noise.FRand() * Canvas->ClipY;
		const float Grey = Noise.FRandRange(0.3f, 1.f);
		const float Size = Noise.FRandRange(1.f, 3.f + 5.f * static_cast<float>(Interference));
		DrawRect(FLinearColor(Grey, Grey, Grey, 0.25f + 0.6f * static_cast<float>(Interference)), X, Y, Size, Size * 0.6f);
	}
	if (Interference > 0.35)
	{
		const int32 Lines = FMath::RoundToInt((Interference - 0.35) * 40.0);
		for (int32 I = 0; I < Lines; ++I)
		{
			DrawRect(FLinearColor(0.8f, 0.8f, 0.85f, 0.12f), 0.f, Noise.FRand() * Canvas->ClipY, Canvas->ClipX, 2.f);
		}
	}
	if (bShowReadout)
	{
		const FString Readout = FString::Printf(TEXT("Grid: %s (%.2f)   NICE composure %d%%"),
			GLStabilityModel::TierName(GLStabilityModel::TierFor(Interference)), Interference, FMath::RoundToInt(Stability->NiceComposure() * 100.0));
		DrawText(Readout, FLinearColor(0.f, 1.f, 1.f), 24.f, 24.f, GEngine->GetMediumFont(), 1.2f);
		// Development readout: which Grid cell Zenny is in (P3).
		const FName CellId = GLGridCells::CellAt(FVector2D(Zenny->GetActorLocation()));
		if (const FGLCellDef* Cell = GLContent::Get().Find<FGLCellDef>(CellId))
		{
			const FGLBandDef* Band = GLContent::Get().Find<FGLBandDef>(Cell->Band);
			DrawText(FString::Printf(TEXT("Cell: %s   depth %d"), *Cell->DisplayName, Band ? Band->Depth : -1), FLinearColor(0.f, 1.f, 1.f), 24.f, 84.f, GEngine->GetSmallFont(), 1.1f);
		}
	}
	DrawInteractionPrompt(Zenny);
	DrawSubtitles();
	if (const UGLHealthComponent* Life = Zenny->FindComponentByClass<UGLHealthComponent>())
	{
		const float Fraction = static_cast<float>(Life->GetCurrent() / FMath::Max(1.0, Life->GetMax()));
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 24.f, 60.f, 204.f, 14.f);
		DrawRect(FLinearColor(0.2f + 0.8f * (1.f - Fraction), 0.85f * Fraction, 0.25f, 0.9f), 26.f, 62.f, 200.f * Fraction, 10.f);
		if (Life->IsDead())
		{
			DrawText(TEXT("De-rezzed. Rebuilding Zenny..."), FLinearColor(1.f, 0.3f, 0.9f), Canvas->ClipX * 0.5f - 160.f, Canvas->ClipY * 0.45f, GEngine->GetLargeFont(), 1.2f);
		}
	}
	if (const UGLBuildModeComponent* Tools = Zenny->FindComponentByClass<UGLBuildModeComponent>(); Tools && !Tools->StatusLine().IsEmpty())
	{
		DrawText(Tools->StatusLine(), FLinearColor(1.f, 0.85f, 0.3f), 24.f, Canvas->ClipY - 60.f, GEngine->GetMediumFont(), 1.2f);
	}
}

namespace
{
	/** Greedy word wrap to MaxWidth pixels. */
	TArray<FString> Wrap(UCanvas* Canvas, UFont* Font, const FString& Text, float MaxWidth, float Scale)
	{
		TArray<FString> Words, Lines;
		Text.ParseIntoArrayWS(Words);
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Trial = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			float W = 0.f, H = 0.f;
			Canvas->TextSize(Font, Trial, W, H, Scale, Scale);
			if (W > MaxWidth && !Line.IsEmpty())
			{
				Lines.Add(Line);
				Line = Word;
			}
			else
			{
				Line = Trial;
			}
		}
		if (!Line.IsEmpty())
		{
			Lines.Add(Line);
		}
		return Lines;
	}
}

void AGLHUD::DrawSubtitles()
{
	const UGLSubtitleSubsystem* Subtitles = GetWorld() ? GetWorld()->GetSubsystem<UGLSubtitleSubsystem>() : nullptr;
	if (!Subtitles || !Canvas)
	{
		return;
	}
	const TArray<FGLSubtitle> Lines = Subtitles->Visible();
	if (Lines.Num() == 0)
	{
		return;
	}
	static const IConsoleVariable* TextScaleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("gl.Subtitles.TextScale"));
	const float Scale = 1.6f * (TextScaleVar ? TextScaleVar->GetFloat() : 1.f) * FMath::Max(0.75f, Canvas->ClipY / 1080.f);
	UFont* Font = GEngine->GetMediumFont();
	const float MaxWidth = Canvas->ClipX * 0.6f;
	float Bottom = Canvas->ClipY - 110.f * FMath::Max(0.75f, Canvas->ClipY / 1080.f);
	// Newest at the bottom, older lines stacked above it.
	for (int32 I = Lines.Num() - 1; I >= 0; --I)
	{
		const FGLSubtitle& S = Lines[I];
		const FGLSpeakerStyle Style = FGLSpeakerStyle::For(S.Speaker);
		const FString Tag = Style.Name + TEXT(":  ");
		float TagW = 0.f, LineH = 0.f;
		Canvas->TextSize(Font, Tag, TagW, LineH, Scale, Scale);
		const TArray<FString> Wrapped = Wrap(Canvas, Font, S.Text, MaxWidth - TagW, Scale);
		float TextW = 0.f;
		for (const FString& L : Wrapped)
		{
			float W = 0.f, H = 0.f;
			Canvas->TextSize(Font, L, W, H, Scale, Scale);
			TextW = FMath::Max(TextW, W);
		}
		const float BlockW = TagW + TextW + 32.f;
		const float BlockH = LineH * Wrapped.Num() + 16.f;
		const float X = (Canvas->ClipX - BlockW) * 0.5f;
		const float Y = Bottom - BlockH;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), X, Y, BlockW, BlockH); // contrast backing
		DrawText(Tag, Style.Colour, X + 16.f, Y + 8.f, Font, Scale);
		for (int32 L = 0; L < Wrapped.Num(); ++L)
		{
			DrawText(Wrapped[L], FLinearColor(0.97f, 0.97f, 0.97f), X + 16.f + TagW, Y + 8.f + L * LineH, Font, Scale);
		}
		Bottom = Y - 8.f;
	}
}

void AGLHUD::DrawInteractionPrompt(const APawn* Zenny)
{
	const AGLCharacter* Character = Cast<AGLCharacter>(Zenny);
	const UGLInteractorComponent* Interactor = Character ? Character->GetInteractor() : nullptr;
	const UGLBuildModeComponent* Tools = Zenny->FindComponentByClass<UGLBuildModeComponent>();
	if (!Interactor || !Interactor->GetFocus() || (Tools && Tools->GetMode() != EGLToolMode::None))
	{
		return; // nothing in reach, or a tool is out: no prompt, no clutter
	}
	TArray<FGLInteractionOption> Options;
	Interactor->GetFocusOptions(Options);
	if (Options.Num() == 0)
	{
		return;
	}
	const FGLInteractionOption& Option = Options[0];
	UFont* Font = GEngine->GetMediumFont();
	const float Scale = 1.5f * FMath::Max(0.75f, Canvas->ClipY / 1080.f);
	const FString Key = TEXT("[E]  ");
	const FString Label = Option.bEnabled ? Option.Label.ToString()
		: FString::Printf(TEXT("%s  (%s)"), *Option.Label.ToString(), *Option.DisabledReason.ToString());
	float KeyW = 0.f, H = 0.f, LabelW = 0.f;
	Canvas->TextSize(Font, Key, KeyW, H, Scale, Scale);
	Canvas->TextSize(Font, Label, LabelW, H, Scale, Scale);
	const float X = (Canvas->ClipX - KeyW - LabelW) * 0.5f;
	const float Y = Canvas->ClipY * 0.62f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 12.f, Y - 6.f, KeyW + LabelW + 24.f, H + 12.f);
	DrawText(Key, Option.bEnabled ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(0.5f, 0.5f, 0.5f), X, Y, Font, Scale);
	DrawText(Label, Option.bEnabled ? FLinearColor::White : FLinearColor(0.65f, 0.65f, 0.65f), X + KeyW, Y, Font, Scale);
}
