#include "Noise/GLNoiseSubsystem.h"

#include "Combat/GLCreature.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GridlandsGame.h"

namespace
{
	constexpr int32 MaxRecent = 64;
}

double UGLNoiseSubsystem::RadiusFor(FName Action, FName Material)
{
	const double* Metres = GLContent::Tuning().Noise.Radius.Find(Action.ToString());
	const FGLMaterialDef* Def = Material.IsNone() ? nullptr : GLContent::Get().Find<FGLMaterialDef>(Material);
	return (Metres ? *Metres : 0.0) * (Def ? Def->NoiseScale : 1.0) * 100.0;
}

FGLNoiseEvent UGLNoiseSubsystem::Make(FName Action, const FVector& Location, AActor* Instigator, FName Material) const
{
	FGLNoiseEvent Noise;
	Noise.Action = Action;
	Noise.Location = Location;
	Noise.RadiusCm = RadiusFor(Action, Material);
	Noise.Instigator = Instigator;
	Noise.InvestigateSeconds = GLContent::Tuning().Noise.InvestigateSeconds;
	return Noise;
}

int32 UGLNoiseSubsystem::Emit(FGLNoiseEvent Noise)
{
	UWorld* World = GetWorld();
	Noise.WorldSeconds = World ? World->GetTimeSeconds() : 0.0;
	if (World && Noise.RadiusCm > 0.0)
	{
		for (TActorIterator<AGLCreature> It(World); It; ++It)
		{
			Noise.Heard += It->HearNoise(Noise) ? 1 : 0;
		}
	}
	++Emitted;
	++Counts.FindOrAdd(Noise.Action);
	UE_LOG(LogGridlands, Verbose, TEXT("Noise: %s at %s, %.0f m, heard by %d"), *Noise.Action.ToString(), *Noise.Location.ToCompactString(), Noise.RadiusCm / 100.0, Noise.Heard);
	if (Recent.Num() >= MaxRecent)
	{
		Recent.RemoveAt(0);
	}
	const int32 Heard = Noise.Heard;
	Recent.Add(MoveTemp(Noise));
	return Heard;
}

int32 UGLNoiseSubsystem::EmitAction(const UObject* Context, FName Action, const FVector& Location, AActor* Instigator, FName Material)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	UGLNoiseSubsystem* Noise = World ? World->GetSubsystem<UGLNoiseSubsystem>() : nullptr;
	return Noise ? Noise->Emit(Noise->Make(Action, Location, Instigator, Material)) : 0;
}

int32 UGLNoiseSubsystem::CountOf(FName Action) const
{
	return Counts.FindRef(Action);
}
