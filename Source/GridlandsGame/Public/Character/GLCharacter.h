#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GLCharacter.generated.h"

class UCameraComponent;
class UGLFabricatorComponent;
class UGLInteractorComponent;
class UGLInventoryComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * Zenny: the silent, third-person player character.
 *
 * Input actions and key mappings are created in C++ (BuildInput) rather than as .uasset
 * files, so agents can read and change bindings as text (ADR-0002 spirit).
 */
UCLASS()
class GRIDLANDSGAME_API AGLCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGLCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	/** Builds the input actions and mapping context if they do not exist yet (idempotent). */
	void BuildInput();

	UGLInteractorComponent* GetInteractor() const { return Interactor; }
	UGLInventoryComponent* GetInventory() const { return Inventory; }
	UGLFabricatorComponent* GetFabricator() const { return Fabricator; }
	const UInputMappingContext* GetMappingContext() const { return MappingContext; }
	const UInputAction* FindInputAction(FName Name) const;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Interact();
	/** Temporary until crafting UI exists: makes the first craftable recipe. */
	void FabricateFirstAvailable();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLInteractorComponent> Interactor;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGLFabricatorComponent> Fabricator;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputAction>> Actions;
};
