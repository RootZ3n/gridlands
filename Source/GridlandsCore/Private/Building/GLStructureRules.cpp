#include "Building/GLStructureRules.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Inventory/GLInventory.h"
#include "Knowledge/GLKnowledge.h"

namespace
{
	const FName SocketBottom(TEXT("bottom"));
	const FName SocketTop(TEXT("top"));
	const FName SocketSide(TEXT("side"));

	FVector2D RotateQuarter(const FVector2D& V, int32 Quarter)
	{
		switch (((Quarter % 4) + 4) % 4)
		{
		case 1: return FVector2D(-V.Y, V.X);
		case 2: return FVector2D(-V.X, -V.Y);
		case 3: return FVector2D(V.Y, -V.X);
		default: return V;
		}
	}

	double Component(const TArray<double>& V, int32 Index) { return V.IsValidIndex(Index) ? V[Index] : 0.0; }

	struct FLink
	{
		int32 From = INDEX_NONE; // supporter (index into pieces)
		int32 To = INDEX_NONE;   // supported
		bool bVertical = true;
		double Metres = 0.0;
	};

	/** Connections between pieces from coinciding sockets. */
	TArray<FLink> Links(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces)
	{
		TArray<TArray<FGLWorldSocket>> SocketsOf;
		TArray<FVector> Centres;
		for (const FGLPlacedPiece& Piece : Pieces)
		{
			const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Piece.Def);
			SocketsOf.Add(Def ? GLStructureRules::Sockets(*Def, Piece) : TArray<FGLWorldSocket>());
			Centres.Add(Def ? GLStructureRules::Bounds(*Def, Piece).GetCenter() : Piece.Location);
		}
		TArray<FLink> Result;
		for (int32 A = 0; A < Pieces.Num(); ++A)
		{
			for (int32 B = 0; B < Pieces.Num(); ++B)
			{
				if (A == B)
				{
					continue;
				}
				bool bRests = false, bSide = false;
				for (const FGLWorldSocket& SA : SocketsOf[A])
				{
					for (const FGLWorldSocket& SB : SocketsOf[B])
					{
						if (FVector::Dist(SA.Location, SB.Location) > GLStructureRules::SocketToleranceCm)
						{
							continue;
						}
						bRests |= SA.Role == SocketBottom && SB.Role == SocketTop; // A rests on B
						bSide |= SA.Role == SocketSide && SB.Role == SocketSide;
					}
				}
				if (bRests)
				{
					Result.Add({ B, A, true, 0.0 });
				}
				if (bSide)
				{
					Result.Add({ B, A, false, FVector2D::Distance(FVector2D(Centres[A]), FVector2D(Centres[B])) / 100.0 });
				}
			}
		}
		return Result;
	}

	bool Overlap(const FBox& A, const FBox& B)
	{
		const FBox SA = A.ExpandBy(-GLStructureRules::OverlapShrinkCm);
		const FBox SB = B.ExpandBy(-GLStructureRules::OverlapShrinkCm);
		return SA.Min.X < SB.Max.X && SB.Min.X < SA.Max.X && SA.Min.Y < SB.Max.Y && SB.Min.Y < SA.Max.Y && SA.Min.Z < SB.Max.Z && SB.Min.Z < SA.Max.Z;
	}
}

FVector GLStructureRules::ToWorld(const FGLPlacedPiece& Piece, const TArray<double>& LocalMetres)
{
	const FVector2D XY = RotateQuarter(FVector2D(Component(LocalMetres, 0), Component(LocalMetres, 1)) * 100.0, Piece.YawQuarter);
	return Piece.Location + FVector(XY.X, XY.Y, Component(LocalMetres, 2) * 100.0);
}

FBox GLStructureRules::Bounds(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece)
{
	FVector2D Half(Component(Def.Size, 0) * 50.0, Component(Def.Size, 1) * 50.0);
	if (Piece.YawQuarter % 2 != 0)
	{
		Half = FVector2D(Half.Y, Half.X);
	}
	return FBox(Piece.Location - FVector(Half.X, Half.Y, 0.0), Piece.Location + FVector(Half.X, Half.Y, Component(Def.Size, 2) * 100.0));
}

TArray<FGLWorldSocket> GLStructureRules::Sockets(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece)
{
	TArray<FGLWorldSocket> Result;
	for (const FGLBuildSocketDef& Socket : Def.Sockets)
	{
		Result.Add({ Socket.Role, ToWorld(Piece, Socket.Offset) });
	}
	return Result;
}

bool GLStructureRules::RestsOnGround(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece, FGroundHeight Ground)
{
	if (!Def.Grounded)
	{
		return false;
	}
	bool bAny = false;
	for (const FGLWorldSocket& Socket : Sockets(Def, Piece))
	{
		if (Socket.Role == SocketBottom)
		{
			bAny = true;
			if (FMath::Abs(Socket.Location.Z - Ground(FVector2D(Socket.Location))) > GroundToleranceCm)
			{
				return false;
			}
		}
	}
	return bAny;
}

TMap<int32, double> GLStructureRules::ComputeSupport(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces, FGroundHeight Ground)
{
	const int32 N = Pieces.Num();
	TArray<double> Support, Strength, VerticalLoss, LossPerMetre;
	Support.Init(0.0, N);
	for (int32 I = 0; I < N; ++I)
	{
		const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Pieces[I].Def);
		const FGLMaterialDef* Material = Def ? Content.Find<FGLMaterialDef>(Def->Material) : nullptr;
		const bool bStructural = Material && Material->IsStructural() && Material->Support.MaxStack > 0 && Material->Support.MaxHorizontalSpan > 0.0;
		Strength.Add(bStructural ? Material->Support.Strength : 0.0);
		VerticalLoss.Add(bStructural ? Material->Support.Strength / Material->Support.MaxStack : 0.0);
		LossPerMetre.Add(bStructural ? Material->Support.Strength / Material->Support.MaxHorizontalSpan : 0.0);
		if (bStructural && RestsOnGround(*Def, Pieces[I], Ground))
		{
			Support[I] = Strength[I];
		}
	}
	// Widest-path relaxation: support only flows from stronger to weaker, so this converges in <= N passes.
	const TArray<FLink> AllLinks = Links(Content, Pieces);
	for (int32 Pass = 0; Pass < N; ++Pass)
	{
		bool bChanged = false;
		for (const FLink& Link : AllLinks)
		{
			if (Support[Link.From] <= 0.0 || Strength[Link.To] <= 0.0)
			{
				continue;
			}
			const double Loss = Link.bVertical ? VerticalLoss[Link.To] : LossPerMetre[Link.To] * Link.Metres;
			const double Candidate = FMath::Min(Support[Link.From], Strength[Link.To]) - Loss;
			if (Candidate > Support[Link.To] + 1e-9)
			{
				Support[Link.To] = Candidate;
				bChanged = true;
			}
		}
		if (!bChanged)
		{
			break;
		}
	}
	TMap<int32, double> Result;
	for (int32 I = 0; I < N; ++I)
	{
		Result.Add(Pieces[I].Id, Support[I]);
	}
	return Result;
}

FGLBuildCheck GLStructureRules::CheckPlacement(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
	const FGLPlacedPiece& Candidate, FGroundHeight Ground)
{
	FGLBuildCheck Check;
	const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Candidate.Def);
	if (!Def)
	{
		Check.Refusal = EGLBuildRefusal::UnknownPiece;
		Check.Reason = FString::Printf(TEXT("unknown piece %s"), *Candidate.Def.ToString());
		return Check;
	}
	const FBox Box = Bounds(*Def, Candidate);
	for (const FGLPlacedPiece& Other : Existing)
	{
		const FGLBuildPieceDef* OtherDef = Content.Find<FGLBuildPieceDef>(Other.Def);
		if (OtherDef && Overlap(Box, Bounds(*OtherDef, Other)))
		{
			Check.Refusal = EGLBuildRefusal::Overlaps;
			Check.Reason = TEXT("something is already there");
			return Check;
		}
	}
	for (const FGLWorldSocket& Socket : Sockets(*Def, Candidate))
	{
		if (Socket.Role == SocketBottom && Socket.Location.Z < Ground(FVector2D(Socket.Location)) - GroundToleranceCm)
		{
			Check.Refusal = EGLBuildRefusal::Buried;
			Check.Reason = TEXT("the ground is in the way (flatten it first)");
			return Check;
		}
	}
	TArray<FGLPlacedPiece> With(Existing.GetData(), Existing.Num());
	FGLPlacedPiece Probe = Candidate;
	Probe.Id = INT32_MIN; // never collides with a real id
	With.Add(Probe);
	Check.Support = ComputeSupport(Content, With, Ground).FindRef(Probe.Id);
	if (Check.Support <= 1e-6)
	{
		Check.Refusal = EGLBuildRefusal::Unsupported;
		Check.Reason = Def->Grounded ? TEXT("not on firm, level ground and nothing holds it up") : TEXT("nothing holds it up");
	}
	return Check;
}

FGLBuildCheck GLStructureRules::CanPlace(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
	const FGLPlacedPiece& Candidate, FGroundHeight Ground, const FGLKnowledge& Knowledge, const FGLInventory& Inventory)
{
	FGLBuildCheck Check;
	const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Candidate.Def);
	if (!Def)
	{
		return CheckPlacement(Content, Existing, Candidate, Ground);
	}
	if (!Knowledge.KnowsAll(Def->UnlockedBy, &Check.MissingKnowledge))
	{
		Check.Refusal = EGLBuildRefusal::NotKnown;
		Check.Reason = TEXT("Zenny doesn't know how to build this yet");
		return Check;
	}
	for (const FGLItemStackDef& Cost : Def->Cost)
	{
		if (Inventory.CountOf(Cost.Item) < Cost.Count)
		{
			Check.Refusal = EGLBuildRefusal::MissingItems;
			Check.Reason = FString::Printf(TEXT("needs %d x %s"), Cost.Count, *Cost.Item.ToString());
			return Check;
		}
	}
	return CheckPlacement(Content, Existing, Candidate, Ground);
}

TArray<int32> GLStructureRules::CollapsesAfterRemoving(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces,
	int32 RemovedId, FGroundHeight Ground)
{
	TArray<FGLPlacedPiece> Remaining;
	for (const FGLPlacedPiece& Piece : Pieces)
	{
		if (Piece.Id != RemovedId)
		{
			Remaining.Add(Piece);
		}
	}
	const TMap<int32, double> Support = ComputeSupport(Content, Remaining, Ground);
	TArray<int32> Falling;
	for (const FGLPlacedPiece& Piece : Remaining)
	{
		if (Support.FindRef(Piece.Id) <= 1e-6)
		{
			Falling.Add(Piece.Id);
		}
	}
	return Falling;
}

bool GLStructureRules::Snap(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing, FName DefId,
	const FVector& Aim, int32 YawQuarter, FGroundHeight Ground, FGLPlacedPiece& OutCandidate)
{
	const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(DefId);
	if (!Def)
	{
		return false;
	}
	FGLPlacedPiece Local;
	Local.Def = DefId;
	Local.YawQuarter = YawQuarter;
	const TArray<FGLWorldSocket> Mine = Sockets(*Def, Local); // relative to a piece at the origin

	double Best = MaxSnapDistanceCm;
	bool bFound = false;
	for (const FGLPlacedPiece& Other : Existing)
	{
		const FGLBuildPieceDef* OtherDef = Content.Find<FGLBuildPieceDef>(Other.Def);
		if (!OtherDef)
		{
			continue;
		}
		for (const FGLWorldSocket& Theirs : Sockets(*OtherDef, Other))
		{
			const double Distance = FVector::Dist(Theirs.Location, Aim);
			if (Distance >= Best)
			{
				continue;
			}
			for (const FGLWorldSocket& Ours : Mine)
			{
				const bool bCompatible = (Ours.Role == SocketBottom && Theirs.Role == SocketTop) || (Ours.Role == SocketSide && Theirs.Role == SocketSide);
				if (!bCompatible)
				{
					continue;
				}
				FGLPlacedPiece Candidate = Local;
				Candidate.Location = Theirs.Location - Ours.Location;
				if (CheckPlacement(Content, Existing, Candidate, Ground).Refusal == EGLBuildRefusal::Overlaps)
				{
					continue;
				}
				Best = Distance;
				OutCandidate = Candidate;
				bFound = true;
				break;
			}
		}
	}
	if (!bFound && Def->Grounded)
	{
		OutCandidate = Local;
		OutCandidate.Location = FVector(Aim.X, Aim.Y, Ground(FVector2D(Aim)));
		bFound = true;
	}
	return bFound;
}

bool GLStructureRules::IsUnderStructure(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces, const FVector2D& World, double MarginCm)
{
	for (const FGLPlacedPiece& Piece : Pieces)
	{
		const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Piece.Def);
		if (Def && Def->Grounded)
		{
			const FBox Box = Bounds(*Def, Piece).ExpandBy(FVector(MarginCm, MarginCm, 0.0));
			if (World.X >= Box.Min.X && World.X <= Box.Max.X && World.Y >= Box.Min.Y && World.Y <= Box.Max.Y)
			{
				return true;
			}
		}
	}
	return false;
}
