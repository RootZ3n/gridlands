#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLPehlichi.generated.h"

class UGLCapabilityComponent;
class UGLCompanionPositioningComponent;
class UGLPehlichiCommandComponent;
class UGLRepairComponent;
class UGLScanComponent;
class UStaticMeshComponent;

/**
 * Pehlichi: Zenny's companion, a former Neurolink scientist in a squirrel body, and the only
 * repairer of glitches (ADR-0005). Deals zero direct damage (ADR-0017). A game character only;
 * no connection to any real AI system (ADR-0006). Placeholder body until his model exists.
 */
UCLASS()
class GRIDLANDSGAME_API AGLPehlichi : public AActor
{
	GENERATED_BODY()

public:
	AGLPehlichi();
	virtual void BeginPlay() override;

	UGLPehlichiCommandComponent* GetCommands() const { return Commands; }
	UGLScanComponent* GetScan() const { return Scan; }
	UGLRepairComponent* GetRepair() const { return Repair; }
	UGLCapabilityComponent* GetCapabilities() const { return Capabilities; }
	UGLCompanionPositioningComponent* GetPositioning() const { return Positioning; }

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Tail;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLPehlichiCommandComponent> Commands;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLScanComponent> Scan;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLRepairComponent> Repair;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLCapabilityComponent> Capabilities;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLCompanionPositioningComponent> Positioning;
};
