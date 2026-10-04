#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"

/** One box a piece shows or collides with: its transform relative to the piece (the engine cube, 1 m, centred). */
struct GRIDLANDSGAME_API FGLPieceBox
{
	FTransform Local;
	FLinearColor Colour = FLinearColor::White;
	bool bVisible = true;
};

/**
 * What a placed piece presents (P11, ADR-0039), decided once from the fact so every presentation (a piece's own actor,
 * the instanced batch of a player structure, a ghost) shows the same thing:
 * - a frame still waiting for its finish shows its frame (studs);
 * - a finished piece shows its finish (the finish's look, or the shapes in its tint);
 * - a piece complete as built shows its own look.
 * Collision is always the piece's shapes (the authoritative envelope), whatever is shown.
 */
struct GRIDLANDSGAME_API FGLPieceLook
{
	/** "frame", "finish" or "complete". */
	FName Shown;
	/** The piece's shapes: they always collide (except a ghost's); visible only when nothing replaces them. */
	TArray<FGLPieceBox> Shapes;
	/** Visual-only boxes (the frame look). */
	TArray<FGLPieceBox> Frame;
	/** An authored look (a visual.* mesh) replacing the shapes, or None. */
	FName Visual;
};

namespace GLPiecePresentation
{
	/** The piece's world transform (location, yaw in 2.5 degree steps). */
	GRIDLANDSGAME_API FTransform PieceTransform(const FGLPlacedPiece& Piece);
	/** Describes Piece. bGhost: the placement preview (no frame, no finish, no look). False for an unknown piece. */
	GRIDLANDSGAME_API bool Describe(const FGLPlacedPiece& Piece, FGLPieceLook& Out, bool bGhost = false);
	/** A visual's transform relative to the piece (its data yaw, offset and scale). */
	GRIDLANDSGAME_API FTransform VisualTransform(FName Visual);
}
