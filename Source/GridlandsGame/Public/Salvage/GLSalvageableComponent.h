#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Building/GLConstructionRules.h"
#include "Interaction/GLInteractable.h"
#include "GLSalvageableComponent.generated.h"

/** Salvage finished: yields paid to By (P6: structures react to losing a part). */
DECLARE_MULTICAST_DELEGATE_OneParam(FGLOnSalvaged, AActor* /*By*/);

/**
 * Makes its owner salvageable by a definition (salvage.*). Each Interact.Salvage is one hit
 * (GLSalvageRules); when integrity runs out, yields go to the interactor's inventory scaled by
 * the world's settings (ADR-0016), events fire, and the owner (and any linked visual actor) is removed.
 * P11: the yields are those of its recovery path (careful by default; debris: collapse), and the last hit is refused,
 * not paid partly, when the whole result would not fit (personal inventory, then base storage inside a claim).
 */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLSalvageableComponent : public UActorComponent, public IGLInteractable
{
	GENERATED_BODY()

public:
	/** Initializes from a salvage definition; returns false if SalvageId is not one. */
	bool Setup(FName InSalvageId, AActor* InLinkedVisual = nullptr, EGLSalvagePath InPath = EGLSalvagePath::Careful);
	/** P11: items delivered with the yield (a collapsed storage piece's contents). */
	void SetExtraYield(const TMap<FName, int32>& Items) { ExtraYield = Items; }
	/** P11: further salvage definitions recovered by the same path (a piece's finish layers), scaled like its own. */
	void SetLayerSalvage(const TArray<FName>& SalvageIds) { LayerSalvage = SalvageIds; }
	EGLSalvagePath GetPath() const { return Path; }
	/** Everything completing would deliver, after world settings (ADR-0016). */
	TMap<FName, int32> CompletionYield() const;

	virtual void GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const override;
	virtual bool Interact(AActor* Interactor, FGameplayTag Verb) override;

	FName GetSalvageId() const { return SalvageId; }
	double GetIntegrity() const { return Integrity; }
	bool IsSalvaged() const { return bSalvaged; }
	/** Restores a salvaged node from a save: hidden, no yields, no events. */
	void RestoreSalvaged();
	/** P8: its placement's cell streamed out: refuses any salvage and its owner is hidden with no collision (the linked visual is left alone). */
	void Retire();

	/** Fires once when salvage completes (after yields and events). */
	FGLOnSalvaged OnSalvaged;

private:
	void Complete(AActor* Interactor);

	UPROPERTY(VisibleAnywhere) FName SalvageId;
	UPROPERTY(VisibleAnywhere) double Integrity = 0.0;
	UPROPERTY(VisibleAnywhere) bool bSalvaged = false;
	EGLSalvagePath Path = EGLSalvagePath::Careful;
	TMap<FName, int32> ExtraYield;
	TArray<FName> LayerSalvage;
	/** The anchored visual actor this salvage stands for (a fence, a wall); hidden when salvaged. */
	TWeakObjectPtr<AActor> LinkedVisual;
};
