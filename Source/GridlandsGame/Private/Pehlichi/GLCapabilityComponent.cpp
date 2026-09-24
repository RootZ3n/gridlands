#include "Pehlichi/GLCapabilityComponent.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Pehlichi/GLCapabilityRules.h"

int32 UGLCapabilityComponent::Raise(FName CapabilityId, int32 Delta)
{
	const FGLCapabilityDef* Def = GLContent::Get().Find<FGLCapabilityDef>(CapabilityId);
	if (!Def || Delta <= 0)
	{
		return 0;
	}
	const int32 Before = Level(CapabilityId);
	const int32 After = FMath::Min(Before + Delta, GLCapabilityRules::MaxLevel(*Def));
	if (After == Before)
	{
		return 0;
	}
	Levels.Add(CapabilityId, After);
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Pehlichi.CapabilityRaised"));
	Event.Subject = CapabilityId;
	Event.Instigator = GetOwner();
	Event.Numbers.Add(TEXT("level"), After);
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
	return After - Before;
}
