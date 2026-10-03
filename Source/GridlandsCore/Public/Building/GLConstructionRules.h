#pragma once

#include "Building/GLStructureRules.h"
#include "CoreMinimal.h"

class FGLContentRegistry;
class FGLKnowledge;
struct FGLSalvageDef;
struct FGLSalvageYieldDef;

/** P11: how material was recovered (distinct outcomes, ADR-0039). */
enum class EGLSalvagePath : uint8
{
	Careful,     // dismantled: the most intact reusable components
	Destructive, // smashed: fewer components, more scrap
	Collapse,    // salvaged from debris: predominantly scrap
};

enum class EGLInstallRefusal : uint8
{
	None,
	UnknownLayer,
	UnknownPhase,
	PhaseNotImplemented, // registered for the future (electrical), refused until built
	FormRefusesPhase,    // the form is complete as framed, or does not take this phase
	RoleMismatch,        // the finish does not fit this form's role
	AlreadyInstalled,    // that phase is already present
	OutOfOrder,          // a later phase is already installed
	NotKnown,
};

struct GRIDLANDSCORE_API FGLInstallCheck
{
	EGLInstallRefusal Refusal = EGLInstallRefusal::None;
	FString Reason;
	bool IsAllowed() const { return Refusal == EGLInstallRefusal::None; }
};

/**
 * Construction phases and recovery (P11, ADR-0039). FRAME is the placed piece; later phases are layers in canonical
 * order (FRAME -> ELECTRICAL (optional, not implemented) -> FINISH). Layers never change support.
 */
namespace GLConstructionRules
{
	/** The phase a layer definition belongs to (finish.* -> its Phase), or None. */
	GRIDLANDSCORE_API FName PhaseOf(const FGLContentRegistry& Content, FName Layer);

	/** May Layer be installed on Piece now? Knowledge is checked when given. */
	GRIDLANDSCORE_API FGLInstallCheck CanInstall(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece, FName Layer,
		const FGLKnowledge* Knowledge = nullptr);
	/** Piece with Layer added in phase order (call after CanInstall). */
	GRIDLANDSCORE_API FGLPlacedPiece WithLayer(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece, FName Layer);

	/** Is the piece still waiting for an accepted, implemented phase it does not have (its frame shows)? */
	GRIDLANDSCORE_API bool ShowsFrame(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece);

	/** The yields of a salvage definition for a recovery path (an empty path falls back to Yields). */
	GRIDLANDSCORE_API const TArray<FGLSalvageYieldDef>& YieldsFor(const FGLSalvageDef& Salvage, EGLSalvagePath Path);
	/**
	 * Everything a piece gives back by a path: its form's salvage and each layer's, summed per item before world
	 * settings (the caller applies ADR-0016 per yield category). Same item and category entries are summed.
	 */
	GRIDLANDSCORE_API TArray<FGLSalvageYieldDef> PieceYields(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece, EGLSalvagePath Path);
}
