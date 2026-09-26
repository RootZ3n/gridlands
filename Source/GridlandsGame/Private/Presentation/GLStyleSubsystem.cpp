#include "Presentation/GLStyleSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	struct FStylePreset
	{
		FRotator Sun;
		FLinearColor SunColour;
		float SunLux;
		FLinearColor SkyTint;
		float SkyIntensity;
		FLinearColor FogColour;
		float ExposureBias;
		float Saturation;
	};

	/**
	 * Provisional look (operator review decides). Colour survives the dark: dusk and night shift hue and
	 * value, never collapse to grey (VISUAL-DIRECTION).
	 */
	FStylePreset PresetFor(FName Name)
	{
		if (Name == TEXT("dusk"))
		{
			return { FRotator(-9.0, 250.0, 0.0), FLinearColor(1.0f, 0.46f, 0.32f), 4.5f, FLinearColor(0.72f, 0.46f, 1.0f), 1.3f, FLinearColor(0.85f, 0.32f, 0.55f), 0.8f, 1.2f };
		}
		if (Name == TEXT("night"))
		{
			// A cool moon as the key light; the sky stays blue-violet; lit signs and colour carry the scene.
			return { FRotator(-38.0, 120.0, 0.0), FLinearColor(0.42f, 0.55f, 1.0f), 0.9f, FLinearColor(0.34f, 0.36f, 0.95f), 1.1f, FLinearColor(0.18f, 0.12f, 0.45f), 2.2f, 1.25f };
		}
		return { FRotator(-42.0, 60.0, 0.0), FLinearColor(1.0f, 0.93f, 0.80f), 9.0f, FLinearColor(0.92f, 0.98f, 1.1f), 1.2f, FLinearColor(0.52f, 0.72f, 1.0f), 0.3f, 1.18f };
	}

	UGLStyleSubsystem* StyleOf(UWorld* World) { return World ? World->GetSubsystem<UGLStyleSubsystem>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs PresetCommand(TEXT("gl.Style.Preset"), TEXT("P7 look: day | dusk | night"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGLStyleSubsystem* Style = StyleOf(World)) { Style->ApplyPreset(Args.Num() ? FName(*Args[0]) : FName(TEXT("day"))); }
		}));
	FAutoConsoleCommandWithWorldAndArgs PostCommand(TEXT("gl.Style.Post"), TEXT("P7 stylize post-process on (1) / off (0)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGLStyleSubsystem* Style = StyleOf(World)) { Style->SetPostEnabled(!Args.Num() || Args[0] != TEXT("0")); }
		}));
	FAutoConsoleCommandWithWorldAndArgs OutlineCommand(TEXT("gl.Style.Outline"), TEXT("P7 graphic outlines on (1) / off (0)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGLStyleSubsystem* Style = StyleOf(World)) { Style->SetParam(TEXT("OutlineOn"), !Args.Num() || Args[0] != TEXT("0") ? 1.f : 0.f); }
		}));
	FAutoConsoleCommandWithWorldAndArgs CelCommand(TEXT("gl.Style.Cel"), TEXT("P7 light banding on (1) / off (0)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGLStyleSubsystem* Style = StyleOf(World)) { Style->SetParam(TEXT("CelOn"), !Args.Num() || Args[0] != TEXT("0") ? 1.f : 0.f); }
		}));
	FAutoConsoleCommandWithWorldAndArgs ParamCommand(TEXT("gl.Style.Param"), TEXT("P7: gl.Style.Param <PP_GLStylize scalar> <value>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGLStyleSubsystem* Style = StyleOf(World); Style && Args.Num() >= 2) { Style->SetParam(FName(*Args[0]), FCString::Atof(*Args[1])); }
		}));
}

bool UGLStyleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// The real game (and PIE) only: automation worlds keep their plain rendering.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && !IsRunningCommandlet() && FApp::CanEverRender();
}

void UGLStyleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.GetFirstPlayerController() && InWorld.GetNetMode() != NM_Standalone)
	{
		return;
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Gridlands/Art/Materials/PP_GLStylize.PP_GLStylize"));
	Volume = InWorld.SpawnActor<APostProcessVolume>();
	if (Volume)
	{
		Volume->bUnbound = true;
		Volume->Priority = 10.f;
		if (Material)
		{
			Stylize = UMaterialInstanceDynamic::Create(Material, this);
			Volume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Stylize));
		}
		Volume->Settings.bOverride_AutoExposureMinBrightness = true;
		Volume->Settings.AutoExposureMinBrightness = 0.6f;
		Volume->Settings.bOverride_AutoExposureMaxBrightness = true;
		Volume->Settings.AutoExposureMaxBrightness = 1.6f;
	}
	ApplyPreset(Preset);
	UE_LOG(LogGridlands, Log, TEXT("Style: stylize post %s, preset %s"), Stylize ? TEXT("on") : TEXT("MISSING (run Tools/art.sh)"), *Preset.ToString());
}

void UGLStyleSubsystem::ApplyPreset(FName Name)
{
	UWorld* World = GetWorld();
	Preset = Name;
	const FStylePreset P = PresetFor(Name);
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		It->SetActorRotation(P.Sun);
		It->GetComponent()->SetLightColor(P.SunColour);
		It->GetComponent()->SetIntensity(P.SunLux);
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		It->GetLightComponent()->SetLightColor(P.SkyTint);
		It->GetLightComponent()->SetIntensity(P.SkyIntensity);
		It->GetLightComponent()->RecaptureSky();
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		It->GetComponent()->SetFogInscatteringColor(P.FogColour); // density stays the stability system's
	}
	if (Volume)
	{
		Volume->Settings.bOverride_AutoExposureBias = true;
		Volume->Settings.AutoExposureBias = P.ExposureBias;
		Volume->Settings.bOverride_ColorSaturation = true;
		Volume->Settings.ColorSaturation = FVector4(P.Saturation, P.Saturation, P.Saturation, 1.0);
	}
	UE_LOG(LogGridlands, Log, TEXT("Style: preset %s"), *Name.ToString());
}

void UGLStyleSubsystem::SetPostEnabled(bool bEnabled)
{
	bPost = bEnabled;
	if (Volume && Stylize)
	{
		Volume->Settings.WeightedBlendables.Array.Reset();
		if (bEnabled)
		{
			Volume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Stylize));
		}
	}
}

void UGLStyleSubsystem::SetParam(FName Name, float Value)
{
	if (Stylize)
	{
		Stylize->SetScalarParameterValue(Name, Value);
	}
}
