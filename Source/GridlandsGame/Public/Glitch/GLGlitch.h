#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Glitch/GLGlitchLifecycle.h"
#include "GLGlitch.generated.h"

class UGLGlitchComponent;
class UStaticMeshComponent;

/** A glitch spawned from a glitch placement. Invisible while Latent (VISUAL-DIRECTION: the Grid shows once revealed). */
UCLASS()
class GRIDLANDSGAME_API AGLGlitch : public AActor
{
	GENERATED_BODY()

public:
	AGLGlitch();
	/** Binds the visual to state changes on every spawn (BeginPlay is not guaranteed to have run). */
	virtual void PostInitializeComponents() override;

	UGLGlitchComponent* GetGlitch() const { return Glitch; }
	/** The spot Pehlichi must reach to repair it. */
	FVector GetRepairPoint() const { return GetActorLocation(); }

private:
	void UpdateVisual(EGLGlitchState From, EGLGlitchState To);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLGlitchComponent> Glitch;
};
