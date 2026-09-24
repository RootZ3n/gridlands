#include "Glitch/GLGlitchRules.h"

#include "Content/GLContentDefinitions.h"
#include "Pehlichi/GLCapabilityRules.h"

namespace GLGlitchRules
{
	bool RequirementsMet(const FGLGlitchDef& Glitch, const TMap<FString, FName>& Bindings, const FGLRequirementFacts& Facts, TArray<FString>* OutUnmet)
	{
		bool bAll = true;
		for (const FGLGlitchRequirementDef& Requirement : Glitch.Requirements)
		{
			bool bMet = false;
			if (Requirement.Kind == TEXT("Requirement.ObjectSalvaged"))
			{
				const FName* Target = Bindings.Find(Requirement.Name);
				bMet = Target && Facts.IsSalvaged && Facts.IsSalvaged(*Target);
			}
			else if (Requirement.Kind == TEXT("Requirement.ItemDelivered"))
			{
				bMet = Facts.CarriedCount && Facts.CarriedCount(Requirement.Item) >= FMath::Max(1, Requirement.Count);
			}
			// Requirement.PuzzleSolved and future kinds: not met until their system exists.
			if (!bMet)
			{
				bAll = false;
				if (OutUnmet)
				{
					OutUnmet->Add(Requirement.Name);
				}
			}
		}
		return bAll;
	}

	bool CanDetect(const FGLGlitchDef& Glitch, const FGLCapabilityDef& ScanCapability, int32 ScanLevel, double DistanceCm)
	{
		if (Glitch.Detection.Capability != ScanCapability.Id || ScanLevel < Glitch.Detection.MinLevel)
		{
			return false;
		}
		const double RangeCm = GLCapabilityRules::EffectValue(ScanCapability, ScanLevel, TEXT("scan_range")) * 100.0;
		return DistanceCm <= RangeCm;
	}
}
