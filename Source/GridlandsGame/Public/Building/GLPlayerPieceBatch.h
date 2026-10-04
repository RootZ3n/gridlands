#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLPlayerPieceBatch.generated.h"

class UInstancedStaticMeshComponent;
class UPrimitiveComponent;

/**
 * The instanced presentation of a player structure's static pieces (P11 scaling, ADR-0039 §11): GAMEPLAY MODEL !=
 * PRESENTATION. Each piece stays an individually authoritative record in UGLStructureSubsystem (identity, ownership,
 * layers, support, previews, salvage, collapse, save); this actor only draws and collides for the quiescent ones, so a
 * 300-piece house is one actor and a handful of components instead of hundreds of actors.
 *
 * - Batching key: what the instances actually share: the blockout box's colour, or the authored visual. One hidden
 *   collision set holds every piece's shapes (the authoritative envelope: blocking, navigation reads it).
 * - Identity: every component keeps an owner table (instance index -> piece id) mirrored on every add and removal
 *   (instances are removed in order, never swapped). A hit's instance index is translated through it; renderer order is
 *   never gameplay identity, and nothing about it is ever saved.
 * - Built entirely from the model: on stream-in the model presents into it; on stream-out it is retired whole.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLPlayerPieceBatch : public AActor
{
	GENERATED_BODY()

public:
	AGLPlayerPieceBatch();

	/**
	 * Whether a piece can be presented here: a known, intact-presentable piece without interaction or dynamic
	 * presentation (storage pieces keep their actor for Store / Take; a look with lights or corruption cubes too).
	 */
	static bool CanInstance(const FGLPlacedPiece& Piece);
	/** Adds Piece's instances (its look, its collision). False if it is already here or cannot be instanced. */
	bool Add(const FGLPlacedPiece& Piece);
	/** Removes every instance of PieceId (every other piece keeps its identity). */
	bool Remove(int32 PieceId);
	bool Contains(int32 PieceId) const { return Shown.Contains(PieceId); }
	/** "frame", "finish" or "complete"; None when the piece is not here. */
	FName ShownPhaseOf(int32 PieceId) const;
	/** The piece a hit on Component's instance Item belongs to (0: none). */
	int32 PieceIdAt(const UPrimitiveComponent* Component, int32 Item) const;
	/** The removal preview: these pieces' envelopes shown red (an overlay; their own look is unchanged). */
	void SetHighlighted(const TArray<int32>& PieceIds);
	const TArray<int32>& GetHighlighted() const { return Highlighted; }
	/** World transforms of PieceId's collision boxes (the engine cube scaled), in shape order. */
	TArray<FTransform> CollisionOf(int32 PieceId) const;
	/** World transforms of PieceId's visible instances (tests: a rotated piece is drawn where it is). */
	TArray<FTransform> VisibleOf(int32 PieceId) const;
	/** The collision set (tests translate its instance indices through PieceIdAt). */
	const UInstancedStaticMeshComponent* GetCollisionSet() const { return Collision; }
	int32 NumPieces() const { return Shown.Num(); }
	int32 NumInstances() const;
	int32 NumComponents() const;
	/**
	 * Stream-out (ADR-0033): invisible at once, and out of gameplay at once (its pieces left the model, so nothing it
	 * holds resolves to a piece any more); one actor whatever the piece count. Its collision is torn down over the next
	 * frames by RetireStep within the retirement budget (a cell unloads only when Zenny is beyond the unload margin).
	 */
	void Retire();
	bool IsRetired() const { return bRetired; }
	/** The player structure it presents (a retired batch keeps it, so a stream-in can find what is still retiring). */
	FName StructureKey;
	/** Tears down up to MaxBodies collision instances (from the end). True when nothing is left to tear down. */
	bool RetireStep(int32 MaxBodies);

private:
	UInstancedStaticMeshComponent* NewSet(class UStaticMesh* Mesh, bool bCollides);
	UInstancedStaticMeshComponent* BoxSet(const FLinearColor& Colour);
	UInstancedStaticMeshComponent* VisualSet(FName Visual);
	void AddInstance(UInstancedStaticMeshComponent* Set, const FTransform& World, int32 PieceId);

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Collision;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Highlight;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Sets;
	/** Per component: instance index -> piece id (presentation bookkeeping only). */
	TMap<const UInstancedStaticMeshComponent*, TArray<int32>> Owners;
	TMap<int32, FName> Shown;
	TArray<int32> Highlighted;
	bool bRetired = false;
};
