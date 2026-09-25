#include "Pehlichi/GLScanComponent.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchRules.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLCapabilityRules.h"
#include "World/GLStabilitySubsystem.h"

FGLScanResult UGLScanComponent::Scan()
{
	FGLScanResult Result;
	const AActor* Owner = GetOwner();
	const UGLCapabilityComponent* Capabilities = Owner ? Owner->FindComponentByClass<UGLCapabilityComponent>() : nullptr;
	const FGLCapabilityDef* Capability = GLContent::Get().Find<FGLCapabilityDef>(ScanCapability);
	UGLGlitchSubsystem* Glitches = GetWorld() ? GetWorld()->GetSubsystem<UGLGlitchSubsystem>() : nullptr;
	if (!Capabilities || !Capability || !Glitches)
	{
		return Result;
	}
	const int32 Level = Capabilities->Level(ScanCapability);
	const double RangeCm = GLCapabilityRules::EffectValue(*Capability, Level, TEXT("scan_range")) * 100.0;
	for (AGLGlitch* Glitch : Glitches->GlitchesNear(Owner->GetActorLocation(), RangeCm))
	{
		UGLGlitchComponent* Component = Glitch->GetGlitch();
		const FGLGlitchDef* Def = GLContent::Get().Find<FGLGlitchDef>(Component->GetGlitchId());
		const double Distance = FVector::Dist(Glitch->GetActorLocation(), Owner->GetActorLocation());
		if (!Def || !GLGlitchRules::CanDetect(*Def, *Capability, Level, Distance))
		{
			continue;
		}
		if (Component->GetState() == EGLGlitchState::Latent && Component->Reveal(FGLScanAuthority()))
		{
			++Result.NewlyRevealed;
		}
		if (FGLGlitchLifecycle::IsVisibleToPlayer(Component->GetState()))
		{
			// Interference blurs what Pehlichi reads (WORLD-AND-PROGRESSION section 4).
			const UGLStabilitySubsystem* Stability = GetWorld()->GetSubsystem<UGLStabilitySubsystem>();
			const double Confidence = Stability ? 1.0 - Stability->InterferenceAt(Glitch->GetActorLocation()) : 1.0;
			Result.Findings.Add({ TEXT("Glitch"), Component->GetGlitchId(), Glitch->GetActorLocation(), Confidence });
		}
	}
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Pehlichi.Scanned"));
	Event.Instigator = GetOwner();
	Event.Numbers.Add(TEXT("found"), Result.Findings.Num());
	Event.Numbers.Add(TEXT("revealed"), Result.NewlyRevealed);
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
	return Result;
}
