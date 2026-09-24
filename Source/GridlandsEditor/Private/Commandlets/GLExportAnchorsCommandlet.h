#pragma once

#include "Commandlets/Commandlet.h"
#include "GLExportAnchorsCommandlet.generated.h"

/** Writes Data/anchor/<cell>.generated.json for every cell whose definition names a level (ADR-0018). */
UCLASS()
class UGLExportAnchorsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
