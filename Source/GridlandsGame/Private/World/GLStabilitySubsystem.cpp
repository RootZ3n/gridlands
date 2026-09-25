#include "World/GLStabilitySubsystem.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr double MetresToCm = 100.0;
	constexpr double DefaultInfluenceMetres = 40.0;
	/** Planning cell size (WORLD-AND-PROGRESSION section 2): a cell's centre is coord * 1 km. */
	constexpr double CellSizeCm = 1000.0 * MetresToCm;
}

void UGLStabilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGLGlitchSubsystem>();
	if (UGLEventSubsystem* Bus = Collection.InitializeDependency<UGLEventSubsystem>())
	{
		Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Glitch.Repaired")),
			FGLGameplayEventDelegate::CreateUObject(this, &UGLStabilitySubsystem::HandleGlitchRepaired));
	}
}

void UGLStabilitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (TActorIterator<AExponentialHeightFog> It(&InWorld); It; ++It)
	{
		Fog = *It;
		return;
	}
	Fog = InWorld.SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator);
}

TArray<FGLStabilitySample> UGLStabilitySubsystem::Samples() const
{
	TArray<FGLStabilitySample> Out;
	const UGLGlitchSubsystem* Glitches = GetWorld()->GetSubsystem<UGLGlitchSubsystem>();
	for (const TWeakObjectPtr<AGLGlitch>& Actor : Glitches->GetAll())
	{
		if (!Actor.IsValid())
		{
			continue;
		}
		const FGLGlitchDef* Def = GLContent::Get().Find<FGLGlitchDef>(Actor->GetGlitch()->GetGlitchId());
		FGLStabilitySample& Sample = Out.AddDefaulted_GetRef();
		Sample.Location = FVector2D(Actor->GetActorLocation());
		Sample.Weight = Def ? Def->StabilityWeight : 1.0;
		Sample.InfluenceRadiusCm = (Def && Def->InfluenceRadius > 0.0 ? Def->InfluenceRadius : DefaultInfluenceMetres) * MetresToCm;
		Sample.bRepaired = Actor->GetGlitch()->GetState() == EGLGlitchState::Repaired;
	}
	return Out;
}

double UGLStabilitySubsystem::BaselineAt(const FVector& Location) const
{
	double Baseline = -1.0;
	int32 InsideDepth = -1;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLCellDef* Cell = Entry.Definition.GetPtr<FGLCellDef>();
		const FGLBandDef* Band = Cell ? GLContent::Get().Find<FGLBandDef>(Cell->Band) : nullptr;
		if (!Band || Baseline >= 0.0)
		{
			return;
		}
		const FVector2D Centre(Cell->Coord.X * CellSizeCm, Cell->Coord.Y * CellSizeCm);
		const FVector2D Offset = FVector2D(Location) - Centre;
		const double Half = Cell->PlayableHalfExtent > 0.0 ? Cell->PlayableHalfExtent * MetresToCm : CellSizeCm * 0.5;
		if (FMath::Abs(Offset.X) <= Half && FMath::Abs(Offset.Y) <= Half)
		{
			Baseline = Band->BaselineInterference;
		}
		else if (FMath::Abs(Offset.X) <= CellSizeCm * 0.5 && FMath::Abs(Offset.Y) <= CellSizeCm * 0.5)
		{
			InsideDepth = Band->Depth; // in the cell but beyond its playable area
		}
	});
	if (Baseline >= 0.0)
	{
		return Baseline;
	}
	// Beyond the playable area: the next band out toward NICE (or the deepest known).
	double Next = 1.0;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLBandDef* Band = Entry.Definition.GetPtr<FGLBandDef>();
		if (Band && Band->Depth == FMath::Max(InsideDepth, 0) + 1)
		{
			Next = Band->BaselineInterference;
		}
	});
	return Next;
}

double UGLStabilitySubsystem::InterferenceAt(const FVector& Location) const
{
	return GLStabilityModel::InterferenceAt(FVector2D(Location), BaselineAt(Location), Samples());
}

double UGLStabilitySubsystem::NiceComposure() const
{
	return GLStabilityModel::NiceComposure(Samples());
}

void UGLStabilitySubsystem::HandleGlitchRepaired(const FGLGameplayEvent& Event)
{
	FGLGameplayEvent Stabilized;
	Stabilized.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.World.Stabilized"));
	Stabilized.Subject = Event.Subject;
	Stabilized.Numbers.Add(TEXT("composure"), NiceComposure());
	UGLEventSubsystem::Emit(this, MoveTemp(Stabilized));
}

void UGLStabilitySubsystem::Tick(float DeltaTime)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		return;
	}
	const double Interference = InterferenceAt(Player->GetActorLocation());
	if (AExponentialHeightFog* FogActor = Fog.Get())
	{
		FogActor->GetComponent()->SetFogDensity(0.002f + static_cast<float>(Interference) * 0.08f);
	}
	const EGLInterferenceTier Tier = GLStabilityModel::TierFor(Interference);
	if (bHavePlayerTier && Tier != PlayerTier)
	{
		FGLGameplayEvent Changed;
		Changed.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.World.InterferenceTierChanged"));
		Changed.Subject = FName(GLStabilityModel::TierName(Tier));
		Changed.Numbers.Add(TEXT("interference"), Interference);
		Changed.Numbers.Add(TEXT("rising"), Tier > PlayerTier ? 1.0 : 0.0);
		UGLEventSubsystem::Emit(this, MoveTemp(Changed));
	}
	PlayerTier = Tier;
	bHavePlayerTier = true;
}
