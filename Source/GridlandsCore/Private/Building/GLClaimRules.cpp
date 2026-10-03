#include "Building/GLClaimRules.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"

bool FGLClaim::Contains(const FVector2D& Point) const
{
	return Areas.ContainsByPredicate([&Point](const FGLClaimArea& Area) { return FVector2D::DistSquared(Point, Area.Centre) <= FMath::Square(Area.RadiusCm); });
}

bool FGLClaim::Overlaps(const FGLClaim& Other) const
{
	for (const FGLClaimArea& A : Areas)
	{
		for (const FGLClaimArea& B : Other.Areas)
		{
			if (FVector2D::Distance(A.Centre, B.Centre) < A.RadiusCm + B.RadiusCm)
			{
				return true;
			}
		}
	}
	return false;
}

TArray<FGLClaim> GLClaimRules::ClaimsFrom(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces, double RadiusCm)
{
	TArray<FGLClaim> Claims;
	for (const FGLPlacedPiece& Piece : Pieces)
	{
		const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Piece.Def);
		if (Piece.Origin != EGLPieceOrigin::Player || !Def || Def->Role != FName(RoleBaseCore))
		{
			continue;
		}
		FGLClaim& Claim = Claims.AddDefaulted_GetRef();
		Claim.Id = FName(*FString::Printf(TEXT("claim.%s.%d"), *Piece.Cell.ToString(), Piece.Id));
		Claim.Cell = Piece.Cell;
		Claim.Areas.Add({ FVector2D(Piece.Location), RadiusCm, Piece.Id });
	}
	Claims.Sort([](const FGLClaim& A, const FGLClaim& B) { return A.Areas[0].SourcePiece < B.Areas[0].SourcePiece; });
	return Claims;
}

const FGLClaim* GLClaimRules::ClaimAt(TConstArrayView<FGLClaim> Claims, const FVector2D& Point)
{
	for (const FGLClaim& Claim : Claims)
	{
		if (Claim.Contains(Point))
		{
			return &Claim;
		}
	}
	return nullptr;
}

bool GLClaimRules::MayRenew(EGLPieceOrigin Origin, const FVector2D& Location, TConstArrayView<FGLClaim> Claims)
{
	if (Origin == EGLPieceOrigin::Player)
	{
		return false;
	}
	return ClaimAt(Claims, Location) == nullptr;
}
