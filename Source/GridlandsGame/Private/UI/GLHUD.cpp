#include "UI/GLHUD.h"

#include "Building/GLBuildModeComponent.h"
#include "Combat/GLHealthComponent.h"

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
	}
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
