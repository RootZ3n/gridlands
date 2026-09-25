#include "World/GLAmbientSubsystem.h"

#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagsManager.h"
#include "Kismet/GameplayStatics.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "World/GLPlacementSubsystem.h"

void UGLAmbientSubsystem::Tick(float DeltaTime)
{
	if (const UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		Step(DeltaTime, UGameplayStatics::GetPlayerPawn(World, 0));
	}
}

void UGLAmbientSubsystem::Step(float DeltaTime, const AActor* Zenny)
{
	if (!Zenny)
	{
		return;
	}
	const FVector Where = Zenny->GetActorLocation();
	UWorld* World = GetWorld();
	if (const UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>())
	{
		for (const FGLDiscoverySite& Site : Placements->GetDiscoveries())
		{
			if (!Discovered.Contains(Site.Placement) && FVector::Dist(Where, Site.Location) <= Site.RadiusCm)
			{
				Discovered.Add(Site.Placement);
				// The specific line (this place) before the generic one (something was learned).
				Emit(TEXT("Event.Discovery.Found"), Site.Knowledge);
				World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(Site.Knowledge);
			}
		}
	}
	SinceAmbient += DeltaTime;
	if (SinceAmbient >= AmbientSeconds)
	{
		SinceAmbient = 0.0;
		Emit(TEXT("Event.Ambient.Tick"), NAME_None);
	}
	StillFor = FVector::Dist(Where, LastPosition) < 5.0 ? StillFor + DeltaTime : 0.0;
	LastPosition = Where;
	if (StillFor < SilenceSeconds)
	{
		bSilenceAnnounced = false;
	}
	else if (!bSilenceAnnounced)
	{
		bSilenceAnnounced = true;
		Emit(TEXT("Event.Player.Silent"), NAME_None);
	}
}

void UGLAmbientSubsystem::Emit(const TCHAR* Tag, FName Subject)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
	Event.Subject = Subject;
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
}
