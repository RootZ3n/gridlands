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
#include "Presentation/GLVisuals.h"
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
			// A pink key over a violet-cyan fill: greens go teal and rose, never brown.
			return { FRotator(-16.0, 250.0, 0.0), FLinearColor(1.0f, 0.52f, 0.66f), 4.5f, FLinearColor(0.45f, 0.70f, 1.0f), 3.6f, FLinearColor(0.95f, 0.45f, 0.70f), 1.2f, 1.45f };
		}
		if (Name == TEXT("night"))
		{
			// A cool moon as the key light; the sky stays blue-violet; lit signs and colour carry the scene.
			return { FRotator(-10.0, 120.0, 0.0), FLinearColor(0.45f, 0.58f, 1.0f), 1.4f, FLinearColor(0.35f, 0.45f, 1.0f), 3.2f, FLinearColor(0.22f, 0.16f, 0.55f), 1.1f, 1.35f };
		}
		return { FRotator(-42.0, 60.0, 0.0), FLinearColor(1.0f, 0.93f, 0.80f), 9.0f, FLinearColor(0.92f, 0.98f, 1.1f), 1.2f, FLinearColor(0.52f, 0.72f, 1.0f), 0.3f, 1.18f };
	}

	UGLStyleSubsystem* StyleOf(UWorld* World) { return World ? World->GetSubsystem<UGLStyleSubsystem>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs PresetCommand(TEXT("gl.Style.Preset"), TEXT("P7 look: day | dusk | night"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGLStyleSubsystem* Style = StyleOf(World)) { Style->ApplyPreset(Args.Num() ? FName(*Args[0]) : FName(TEXT("day"))); }
		}));
	FAutoConsoleCommandWithWorldAndArgs VariantCommand(TEXT("gl.Style.Variant"), TEXT("P7.1 visual variant: A (canonical) | P7 | B | C (review record only)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGLStyleSubsystem* Style = StyleOf(World)) { Style->SetVariant(Args.Num() ? FName(*Args[0]) : FName(TEXT("A"))); }
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
	GLVisuals::Preload();
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
	// P7.1 variants layer on the preset: the outline treatment, and (A/B/C) richer, more dimensional light.
	const bool bP7 = Variant == TEXT("P7");
	const bool bC = Variant == TEXT("C");
	const bool bNight = Name == TEXT("night");
	float Saturation = P.Saturation;
	float Bias = P.ExposureBias;
	if (!bP7)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			// A stronger, warmer key against a cooler sky fill: form reads from light, not from lines.
			It->GetComponent()->SetIntensity(P.SunLux * (bNight ? 1.15f : 1.1f));
			It->GetComponent()->SetLightColor(P.SunColour * (bC ? FLinearColor(1.06f, 0.95f, 0.82f) : FLinearColor(1.04f, 1.0f, 0.94f)));
		}
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			It->GetLightComponent()->SetLightColor(P.SkyTint * FLinearColor(0.92f, 0.98f, 1.1f));
			It->GetLightComponent()->SetIntensity(P.SkyIntensity * (bC ? 0.95f : 0.85f));
			It->GetLightComponent()->RecaptureSky();
		}
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			// Atmospheric depth: a sun-coloured glow toward the key (layering), fog density untouched.
			It->GetComponent()->SetDirectionalInscatteringColor(P.SunColour * (bC ? 0.45f : 0.22f));
			It->GetComponent()->SetDirectionalInscatteringExponent(6.0f);
		}
		// Colourful does not mean flat, and dimensional does not mean muted: depth comes from AO and light,
		// while saturation holds (A/B lift it slightly; C keeps P7's).
		Saturation = bC ? P.Saturation : P.Saturation + 0.1f;
		Bias = P.ExposureBias - (bC ? 0.25f : 0.08f);
	}
	if (Volume)
	{
		Volume->Settings.bOverride_AutoExposureBias = true;
		Volume->Settings.AutoExposureBias = Bias;
		Volume->Settings.bOverride_ColorSaturation = true;
		Volume->Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0);
		Volume->Settings.bOverride_AmbientOcclusionIntensity = true;
		Volume->Settings.AmbientOcclusionIntensity = bP7 ? 0.5f : (bC ? 0.85f : 0.7f);
		Volume->Settings.bOverride_AmbientOcclusionRadius = true;
		Volume->Settings.AmbientOcclusionRadius = bP7 ? 200.f : 120.f;
		Volume->Settings.bOverride_BloomIntensity = true;
		Volume->Settings.BloomIntensity = bP7 ? 0.675f : (bNight ? 1.0f : 0.6f);
	}
	struct FOutline { float Env, Char, Creases, Cel, Width, Darkness, FadeStart, FadeEnd; };
	const FOutline O = bP7 ? FOutline{ 1.f, 1.f, 1.f, 1.f, 1.5f, 0.08f, 3500.f, 9000.f }
		: Variant == TEXT("A") ? FOutline{ 0.f, 0.85f, 0.f, 0.f, 1.2f, 0.12f, 3500.f, 9000.f }
		: bC ? FOutline{ 0.f, 0.55f, 0.f, 0.f, 1.0f, 0.2f, 3000.f, 7000.f }
		: FOutline{ 0.8f, 0.9f, 0.f, 0.f, 1.3f, 0.12f, 1500.f, 4500.f }; // B
	SetParam(TEXT("EnvOutline"), O.Env);
	SetParam(TEXT("CharOutline"), O.Char);
	SetParam(TEXT("EnvCreases"), O.Creases);
	SetParam(TEXT("CelOn"), O.Cel);
	SetParam(TEXT("OutlineWidth"), O.Width);
	SetParam(TEXT("OutlineDarkness"), O.Darkness);
	SetParam(TEXT("FadeStart"), O.FadeStart);
	SetParam(TEXT("FadeEnd"), O.FadeEnd);
	UE_LOG(LogGridlands, Log, TEXT("Style: preset %s, variant %s"), *Name.ToString(), *Variant.ToString());
}

void UGLStyleSubsystem::SetVariant(FName InVariant)
{
	Variant = InVariant;
	ApplyPreset(Preset);
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
