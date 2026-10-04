#pragma once

#include "Building/GLBuildCatalog.h"
#include "Building/GLBuildingSubsystem.h"
#include "Building/GLClaimRules.h"
#include "Building/GLConstructionRules.h"
#include "Building/GLStructureRules.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLBuildModeComponent.generated.h"

class AGLBuildPiece;

UENUM()
enum class EGLToolMode : uint8
{
	None,
	Build,
	Dig,
	Raise,
	Flatten,
};

/** P12 (ADR-0040): build mode's sub-states. One is active; the primary action means what the state says, nothing else. */
UENUM()
enum class EGLBuildState : uint8
{
	Place,
	Browse,
	Finish,
	Remove,
};

/** P12 accessibility: a held control, or a press that toggles (the build camera; the collapse confirmation). */
UENUM()
enum class EGLHoldMode : uint8
{
	Hold,
	Toggle,
};

/**
 * P12 (ADR-0040): everything the build UI shows, computed by UGLBuildModeComponent from canonical results only (the
 * placement check, the snap, the cost plan, the removal prediction, the commit's yield, the claim). Widgets read it and
 * send intents; they decide nothing.
 */
struct GRIDLANDSGAME_API FGLBuildView
{
	EGLToolMode Mode = EGLToolMode::None;
	EGLBuildState State = EGLBuildState::Place;
	// Selection.
	FName Category;
	FString CategoryName;
	FName Piece;
	FString PieceName;
	/** Its architectural vocabulary, a label only (never a lock): "Roman, concrete". */
	FString Vocabulary;
	int32 YawStep = 0;
	// Aim (the camera's view: the same ray the commit uses).
	bool bAimed = false;
	FVector AimStart = FVector::ZeroVector;
	FVector AimEnd = FVector::ZeroVector;
	FVector AimPoint = FVector::ZeroVector;
	// PLACE.
	bool bHasCandidate = false;
	FGLPlacedPiece Candidate;
	FGLSnapInfo Snap;
	FGLBuildCheck Check;
	FString Reason;
	FGLCostView Cost;
	// FINISH.
	int32 FinishTarget = 0;
	FName FinishRole;
	TArray<FName> FinishChoices;
	FName Finish;
	FGLInstallCheck FinishCheck;
	FGLCostView FinishCost;
	// REMOVE.
	int32 RemoveTarget = 0;
	TArray<int32> Predicted;
	EGLSalvagePath Path = EGLSalvagePath::Careful;
	TMap<FName, int32> Yield;
	/** Both paths' yields, so the trade-off reads before choosing (each the commit's own yield for that path). */
	TMap<FName, int32> YieldCareful;
	TMap<FName, int32> YieldDestructive;
	bool bYieldFits = true;
	/** The canonical prediction says other pieces will collapse: the removal needs a confirmation (hold or second press). */
	bool bNeedsConfirm = false;
	float ConfirmProgress = 0.f;
	bool bArmed = false;
	bool bStorageNotEmpty = false;
	// Claim (the canonical claim Zenny is in or near; provisional size, never a maximum).
	bool bShowClaim = false;
	bool bInsideClaim = false;
	/** Every area of that claim (P11: one; the area list leaves room for expansion and connected areas). */
	TArray<FGLClaimArea> ClaimAreas;
	// BROWSE.
	int32 BrowseCategory = 0;
	int32 BrowseIndex = 0;
	FName EraFilter;
	// Camera.
	bool bBuildCamera = false;
	float CameraHeightCm = 0.f;
};

/**
 * Zenny's building and terraforming hands (M10; P12 build mode, ADR-0040). Owns build mode's state (sub-state, selection,
 * favorites, recents, the finish remembered per role, rotation, the collapse confirmation) and computes FGLBuildView from
 * canonical results every frame; the building and terrain subsystems own every rule and transaction. The UI reads the
 * view and calls the public intents below; every commit re-aims and re-checks at that moment (UI state is never authority).
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLBuildModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLBuildModeComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---- intents (keys, the browser, the proof all come through these) ----
	void ToggleBuild();
	void CycleTerraform();
	/** Tab: the piece browser (from PLACE), or back to placing. */
	void ToggleBrowser();
	void BrowseMove(int32 Delta);
	void BrowseCategory(int32 Delta);
	/** Selects the browser's highlighted piece and returns to PLACE. */
	void BrowseSelect();
	void SetEraFilter(FName Era);
	/** Esc / right mouse: one level back (BROWSE / FINISH / REMOVE -> PLACE -> out of build mode). */
	void Back();
	/** Mouse wheel in PLACE: the next piece in the current category; Shift+wheel: the next category. */
	void CycleVariant(int32 Delta);
	void CycleCategory(int32 Delta);
	void SelectPiece(FName Piece);
	void SelectFavorite(int32 Slot);
	void PinFavorite(int32 Slot);
	/** Z / Shift+Z: +-90 degrees; C: +15 degrees; Ctrl+wheel: +-2.5 degrees (one canonical step). */
	void RotateQuarter(int32 Direction);
	void Rotate15();
	void RotateFine(int32 Direction);
	/** Left mouse: place (PLACE), select (BROWSE), apply the finish (FINISH), remove (REMOVE; a collapse needs a confirmation). */
	void Primary();
	void PrimaryReleased();
	void ToggleFinishMode();
	void CycleFinish(int32 Delta);
	void ToggleRemoveMode();
	/** N in REMOVE: careful dismantle <-> destructive smash. */
	void TogglePath();
	/** Alt: the build camera (held, or toggled per the accessibility setting). */
	void BuildCamera(bool bPressed);
	void AdjustCameraHeight(float DeltaCm);

	// ---- reading ----
	const FGLBuildView& GetView() const { return View; }
	EGLToolMode GetMode() const { return Mode; }
	FName GetSelectedPiece() const { return Piece; }
	FString StatusLine() const { return Status; }
	const TArray<FGLCatalogCategory>& GetCatalog() const { return Catalog; }
	/** The pieces the browser lists for a category (the era filter applied). */
	TArray<FName> PiecesOf(int32 CategoryIndex) const;
	const TArray<FName>& GetFavorites() const { return Favorites; }
	const TArray<FName>& GetRecents() const { return Recents; }
	FName RememberedFinish(FName Role) const { return FinishByRole.FindRef(Role); }
	/** How many intents of each kind (and in all) the player has issued: the interaction counts of the daily-driver proof. */
	const TMap<FName, int32>& GetIntentCounts() const { return IntentCounts; }
	int32 GetIntentTotal() const { return IntentTotal; }
	/** The game-thread cost of this component's last tick (the view, ghost, markers' inputs): the build-mode perf measure. */
	double GetLastTickMs() const { return LastTickMs; }
	void ResetIntentCounts() { IntentCounts.Reset(); IntentTotal = 0; }

	// ---- settings (playtest profile, never world saves) ----
	EGLHoldMode CameraMode = EGLHoldMode::Hold;
	EGLHoldMode ConfirmMode = EGLHoldMode::Hold;
	void SetCameraMode(EGLHoldMode InMode);
	void SetConfirmMode(EGLHoldMode InMode);
	/** Where favorites, recents, the remembered finishes and the settings live (tests point it elsewhere). */
	void SetProfilePath(const FString& InPath);
	const FString& GetProfilePath() const { return ProfilePath; }
	void LoadProfile();

	UPROPERTY(EditAnywhere, Category = "Building") float ReachCm = 800.f;
	/** How long a collapsing removal must be held. */
	UPROPERTY(EditAnywhere, Category = "Building") float ConfirmHoldSeconds = 0.4f;
	/** A toggled confirmation must follow within this time, on the same piece and the same prediction. */
	UPROPERTY(EditAnywhere, Category = "Building") float ConfirmArmSeconds = 3.f;
	static constexpr int32 FavoriteSlots = 8;
	static constexpr int32 RecentCount = 8;
	/** Updates the view now (the tick does this every frame; a commit does it again at its moment). */
	void RefreshView();

private:
	double LastTickMs = 0.0;
	bool Aim(FHitResult& OutHit, FVector& OutStart, FVector& OutEnd) const;
	void Count(const TCHAR* Intent);
	void SetState(EGLBuildState InState);
	void RebuildCatalog();
	void ClearGhost();
	void UpdatePlacement(const FHitResult* Hit);
	void UpdateFinish(const FHitResult* Hit);
	void UpdateRemoval(const FHitResult* Hit);
	void UpdateClaim();
	void CommitRemoval();
	void SaveProfile() const;
	int32 CategoryIndexOf(FName InPiece) const;

	EGLToolMode Mode = EGLToolMode::None;
	EGLBuildState State = EGLBuildState::Place;
	FGLBuildView View;
	TArray<FGLCatalogCategory> Catalog;
	FName Piece;
	TMap<FName, FName> LastPieceByCategory;
	TArray<FName> Favorites;
	TArray<FName> Recents;
	TMap<FName, FName> FinishByRole;
	int32 YawStep = 0;
	EGLSalvagePath Path = EGLSalvagePath::Careful;
	FName EraFilter;
	int32 BrowseCategoryIndex = 0;
	int32 BrowseIndex = 0;
	bool bBuildCamera = false;
	float CameraHeightCm = 0.f;
	// The collapse confirmation: what it is confirming, so a different target or prediction never inherits it.
	bool bHolding = false;
	float HoldElapsed = 0.f;
	bool bArmed = false;
	float ArmElapsed = 0.f;
	int32 ConfirmTarget = 0;
	TArray<int32> ConfirmPrediction;
	TArray<int32> Highlighted;
	TMap<FName, int32> IntentCounts;
	int32 IntentTotal = 0;
	FString ProfilePath;
	bool bProfileLoaded = false;
	FString Status;
	UPROPERTY(Transient) TObjectPtr<AGLBuildPiece> Ghost;
};
