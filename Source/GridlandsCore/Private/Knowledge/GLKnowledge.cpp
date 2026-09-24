#include "Knowledge/GLKnowledge.h"

bool FGLKnowledge::KnowsAll(const TArray<FName>& UnlockedBy, TArray<FName>* OutMissing) const
{
	bool bAll = true;
	for (const FName& Id : UnlockedBy)
	{
		if (!Known.Contains(Id))
		{
			bAll = false;
			if (OutMissing)
			{
				OutMissing->Add(Id);
			}
		}
	}
	return bAll;
}
