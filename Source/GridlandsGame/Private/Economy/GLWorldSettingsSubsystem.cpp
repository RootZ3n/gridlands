#include "Economy/GLWorldSettingsSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"

bool UGLWorldSettingsSubsystem::SetPreset(FName Id)
{
	if (!GLContent::Get().Find<FGLSettingsPresetDef>(Id))
	{
		return false;
	}
	PresetId = Id;
	return true;
}

const FGLSettingsPresetDef* UGLWorldSettingsSubsystem::GetPreset() const
{
	return GLContent::Get().Find<FGLSettingsPresetDef>(PresetId);
}
