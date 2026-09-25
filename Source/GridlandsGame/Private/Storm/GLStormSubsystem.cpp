#include "Storm/GLStormSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "GridlandsGame.h"
#include "Storm/GLStormArtifact.h"
#include "Terrain/GLTerrainSubsystem.h"

void UGLStormSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGLEventSubsystem* Bus = Collection.InitializeDependency<UGLEventSubsystem>())
	{
		Subscription = Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event")),
			FGLGameplayEventDelegate::CreateUObject(this, &UGLStormSubsystem::HandleEvent));
	}
}

void UGLStormSubsystem::HandleEvent(const FGLGameplayEvent& Event)
{
	const FName Tag = Event.Tag.GetTagName();
	int32& Count = Counts.FindOrAdd(Tag);
	++Count;
	if (IsRaining())
	{
		return;
	}
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLStormDef* Storm = Entry.Definition.GetPtr<FGLStormDef>();
		if (Storm && !IsRaining() && !Occurred.Contains(Entry.Id) && Storm->Trigger.EventCount == Tag && Count >= Storm->Trigger.Min)
		{
			const UGLGlitchSubsystem* Glitches = GetWorld()->GetSubsystem<UGLGlitchSubsystem>();
			Start(Entry.Id, Glitches ? Glitches->GetCommander() : nullptr);
		}
	});
}

bool UGLStormSubsystem::Start(FName StormId, const AActor* Center)
{
	if (IsRaining() || !GLContent::Get().Find<FGLStormDef>(StormId) || !Center)
	{
		return false;
	}
	Active = StormId;
	CenterActor = Center;
	Elapsed = 0.0;
	SpawnDebt = 0.0;
	Spawned = 0;
	Occurred.AddUnique(StormId); // it happened, whatever happens next (it will not repeat)
	UE_LOG(LogGridlands, Log, TEXT("Storm: %s begins"), *StormId.ToString());
	Emit(TEXT("Event.Storm.Started"), StormId);
	return true;
}

void UGLStormSubsystem::Tick(float DeltaTime)
{
	if (IsRaining() || Artifacts.Num() > 0)
	{
		Step(DeltaTime);
	}
}

void UGLStormSubsystem::Step(float DeltaTime)
{
	// Artifacts fall and de-rez whether or not the storm is still going.
	for (int32 I = Artifacts.Num() - 1; I >= 0; --I)
	{
		AGLStormArtifact* Artifact = Artifacts[I];
		if (!Artifact || !IsValid(Artifact))
		{
			Artifacts.RemoveAt(I);
			continue;
		}
		if (!Artifact->Advance(DeltaTime))
		{
			Artifact->Destroy();
			Artifacts.RemoveAt(I);
		}
	}
	const FGLStormDef* Storm = GLContent::Get().Find<FGLStormDef>(Active);
	if (!Storm)
	{
		return;
	}
	Elapsed += DeltaTime;
	if (Elapsed >= Storm->DurationSeconds || !CenterActor.IsValid())
	{
		Stop();
		return;
	}
	SpawnDebt += Storm->SpawnPerSecond * DeltaTime;
	const UGLTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UGLTerrainSubsystem>();
	while (SpawnDebt >= 1.0 && Spawned < Storm->MaxArtifacts)
	{
		SpawnDebt -= 1.0;
		const FVector Centre = CenterActor->GetActorLocation();
		const double Angle = Random.FRandRange(0.0, 2.0 * PI);
		const double Distance = FMath::Sqrt(Random.FRand()) * Storm->Radius * 100.0;
		const FVector2D XY(Centre.X + FMath::Cos(Angle) * Distance, Centre.Y + FMath::Sin(Angle) * Distance);
		const double Ground = Terrain && Terrain->HasGround() && Terrain->GetField().Contains(XY) ? Terrain->HeightAt(XY) : Centre.Z - 90.0;
		AGLStormArtifact* Artifact = GetWorld()->SpawnActor<AGLStormArtifact>(FVector(XY.X, XY.Y, Centre.Z + Random.FRandRange(1500.0, 2500.0)), FRotator::ZeroRotator);
		if (Artifact)
		{
			const TArray<FName>& Kinds = Storm->Artifacts;
			Artifact->Setup(Kinds[Random.RandRange(0, Kinds.Num() - 1)], Random.GetCurrentSeed() + Spawned, Ground);
			Artifacts.Add(Artifact);
			++Spawned;
		}
	}
}

void UGLStormSubsystem::Stop()
{
	const FName Ended = Active;
	Active = NAME_None;
	// Clean up: nothing from the storm outlives it.
	for (AGLStormArtifact* Artifact : Artifacts)
	{
		if (Artifact && IsValid(Artifact))
		{
			Artifact->Destroy();
		}
	}
	Artifacts.Reset();
	UE_LOG(LogGridlands, Log, TEXT("Storm: %s ends (%d artifacts fell)"), *Ended.ToString(), Spawned);
	Emit(TEXT("Event.Storm.Ended"), Ended);
}

int32 UGLStormSubsystem::NumArtifacts() const
{
	int32 Alive = 0;
	for (const AGLStormArtifact* Artifact : Artifacts)
	{
		Alive += Artifact && IsValid(Artifact) ? 1 : 0;
	}
	return Alive;
}

void UGLStormSubsystem::Restore(const TArray<FName>& InOccurred, const TMap<FName, int32>& EventCounts)
{
	Occurred = InOccurred;
	Counts = EventCounts;
}

void UGLStormSubsystem::Emit(const TCHAR* Tag, FName Subject)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
	Event.Subject = Subject;
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
}
