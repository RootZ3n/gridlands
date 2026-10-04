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

	double Component(const TArray<double>& V, int32 Index) { return V.IsValidIndex(Index) ? V[Index] : 0.0; }

	/** Degrees in [0, 360). */
	double Wrap360(double Degrees)
	{
		const double W = FMath::Fmod(Degrees, 360.0);
		return W < 0.0 ? W + 360.0 : W;
	}

	/** Smallest absolute difference between two angles, degrees. */
	double AngleBetween(double A, double B)
	{
		const double D = Wrap360(A - B);
		return FMath::Min(D, 360.0 - D);
	}

	struct FLink
	{
		int32 From = INDEX_NONE; // supporter (index into pieces)
		int32 To = INDEX_NONE;   // supported
		bool bVertical = true;
		double Metres = 0.0;
	};

	/** Do two coinciding side sockets connect? Facings, when both declare one, must face each other. */
	bool SidesConnect(const FGLWorldSocket& A, const FGLWorldSocket& B)
	{
		return !(A.bHasFacing && B.bHasFacing) || AngleBetween(A.Facing, B.Facing + 180.0) <= GLStructureRules::FacingToleranceDegrees;
	}

	FIntVector SocketCell(const FVector& At)
	{
		const double Cell = GLStructureRules::SocketToleranceCm * 2.0;
		return FIntVector(FMath::FloorToInt(At.X / Cell), FMath::FloorToInt(At.Y / Cell), FMath::FloorToInt(At.Z / Cell));
	}

	/**
	 * Connections between pieces from coinciding sockets. P11: sockets are hashed by position (cells twice the tolerance),
	 * so this is linear in the number of sockets rather than quadratic in pieces (a 300-piece house is recomputed per
	 * preview).
	 */
	TArray<FLink> Links(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces)
	{
		struct FEntry
		{
			int32 Piece;
			FGLWorldSocket Socket;
		};
		TArray<FEntry> Entries;
		TArray<FVector> Centres;
		TMap<FIntVector, TArray<int32>> Grid;
		for (int32 I = 0; I < Pieces.Num(); ++I)
		{
			const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Pieces[I].Def);
			Centres.Add(Def ? GLStructureRules::Footprint(*Def, Pieces[I]).Enclosing().GetCenter() : Pieces[I].Location);
			if (!Def)
			{
				continue;
			}
			for (const FGLWorldSocket& Socket : GLStructureRules::Sockets(*Def, Pieces[I]))
			{
				Grid.FindOrAdd(SocketCell(Socket.Location)).Add(Entries.Num());
				Entries.Add({ I, Socket });
			}
		}
		// One flag set per ordered pair: A rests on B, A and B side-linked.
		TSet<TPair<int32, int32>> Rests, Sides;
		for (const FEntry& A : Entries)
		{
			const FIntVector Home = SocketCell(A.Socket.Location);
			for (int32 DX = -1; DX <= 1; ++DX) for (int32 DY = -1; DY <= 1; ++DY) for (int32 DZ = -1; DZ <= 1; ++DZ)
			{
				const TArray<int32>* Near = Grid.Find(Home + FIntVector(DX, DY, DZ));
				if (!Near)
				{
					continue;
				}
				for (const int32 BIndex : *Near)
				{
					const FEntry& B = Entries[BIndex];
					if (B.Piece == A.Piece || FVector::Dist(A.Socket.Location, B.Socket.Location) > GLStructureRules::SocketToleranceCm)
					{
						continue;
					}
					if (A.Socket.Role == SocketBottom && B.Socket.Role == SocketTop)
					{
						Rests.Add({ A.Piece, B.Piece }); // A rests on B
					}
					if (A.Socket.Role == SocketSide && B.Socket.Role == SocketSide && SidesConnect(A.Socket, B.Socket))
					{
						Sides.Add({ A.Piece, B.Piece });
					}
				}
			}
		}
		// Deterministic order (by supported, then supporter), whatever the hash order was.
		TArray<FLink> Result;
		for (const TPair<int32, int32>& R : Rests)
		{
			Result.Add({ R.Value, R.Key, true, 0.0 });
		}
		for (const TPair<int32, int32>& S : Sides)
		{
			Result.Add({ S.Value, S.Key, false, FVector2D::Distance(FVector2D(Centres[S.Key]), FVector2D(Centres[S.Value])) / 100.0 });
		}
		Result.Sort([](const FLink& A, const FLink& B)
		{
			return A.To != B.To ? A.To < B.To : (A.From != B.From ? A.From < B.From : A.bVertical > B.bVertical);
		});
		return Result;
	}

	/** One vertical step of a piece's own material (0 when it is not structural). */
	double VerticalStepOf(const FGLContentRegistry& Content, const FGLBuildPieceDef& Def)
	{
		const FGLMaterialDef* Material = Content.Find<FGLMaterialDef>(Def.Material);
		return Material && Material->Support.MaxStack > 0 ? Material->Support.Strength / Material->Support.MaxStack : 0.0;
	}
}

bool FGLFootprint::ContainsXY(const FVector2D& Point, double MarginCm) const
{
	const FVector2D D = Point - Centre;
	return FMath::Abs(FVector2D::DotProduct(D, AxisX)) <= Half.X + MarginCm && FMath::Abs(FVector2D::DotProduct(D, AxisY)) <= Half.Y + MarginCm;
}

double FGLFootprint::ReachAlong(const FVector2D& Direction) const
{
	return FMath::Abs(FVector2D::DotProduct(Direction, AxisX)) * Half.X + FMath::Abs(FVector2D::DotProduct(Direction, AxisY)) * Half.Y;
}

bool FGLFootprint::OverlapsXY(const FGLFootprint& Other, double ShrinkCm) const
{
	FGLFootprint A = *this, B = Other;
	A.Half = FVector2D(FMath::Max(0.0, A.Half.X - ShrinkCm), FMath::Max(0.0, A.Half.Y - ShrinkCm));
	B.Half = FVector2D(FMath::Max(0.0, B.Half.X - ShrinkCm), FMath::Max(0.0, B.Half.Y - ShrinkCm));
	const FVector2D D = B.Centre - A.Centre;
	// Separating axes: the two axes of each rectangle. Touching edges (gap 0) do not overlap.
	for (const FVector2D& Axis : { A.AxisX, A.AxisY, B.AxisX, B.AxisY })
	{
		if (FMath::Abs(FVector2D::DotProduct(D, Axis)) >= A.ReachAlong(Axis) + B.ReachAlong(Axis) - 1e-6)
		{
			return false;
		}
	}
	return true;
}

bool FGLFootprint::Overlaps(const FGLFootprint& Other, double ShrinkCm) const
{
	return ZMin + ShrinkCm < Other.ZMax - ShrinkCm && Other.ZMin + ShrinkCm < ZMax - ShrinkCm && OverlapsXY(Other, ShrinkCm);
}

TArray<FVector2D> FGLFootprint::SamplePoints() const
{
	TArray<FVector2D> Points;
	for (double U : { -1.0, 0.0, 1.0 })
	{
		for (double V : { -1.0, 0.0, 1.0 })
		{
			Points.Add(Centre + AxisX * (Half.X * U) + AxisY * (Half.Y * V));
		}
	}
	return Points;
}

FBox FGLFootprint::Enclosing() const
{
	const double EX = FMath::Abs(AxisX.X) * Half.X + FMath::Abs(AxisY.X) * Half.Y;
	const double EY = FMath::Abs(AxisX.Y) * Half.X + FMath::Abs(AxisY.Y) * Half.Y;
	return FBox(FVector(Centre.X - EX, Centre.Y - EY, ZMin), FVector(Centre.X + EX, Centre.Y + EY, ZMax));
}

FGLFootprint FGLFootprint::FromBox(const FBox& Box)
{
	FGLFootprint F;
	F.Centre = FVector2D(Box.GetCenter());
	F.Half = FVector2D(Box.GetExtent());
	F.ZMin = Box.Min.Z;
	F.ZMax = Box.Max.Z;
	return F;
}

int32 GLStructureRules::NormalizeYawStep(int32 Step)
{
	return ((Step % YawSteps) + YawSteps) % YawSteps;
}

int32 GLStructureRules::YawStepFromDegrees(double Degrees)
{
	return NormalizeYawStep(FMath::RoundToInt(Wrap360(Degrees) / YawStepDegrees));
}

double GLStructureRules::YawDegrees(int32 Step)
{
	return NormalizeYawStep(Step) * YawStepDegrees;
}

bool GLStructureRules::IsWholeYawStep(double Degrees)
{
	const double Steps = Degrees / YawStepDegrees;
	return FMath::Abs(Steps - FMath::RoundToDouble(Steps)) < 1e-6;
}

FVector2D GLStructureRules::RotateXY(const FVector2D& V, int32 Step)
{
	const int32 S = NormalizeYawStep(Step);
	if (S % QuarterTurnSteps == 0) // quarter turns stay bit-exact (and identical to every pre-P11 result)
	{
		switch (S / QuarterTurnSteps)
		{
		case 1: return FVector2D(-V.Y, V.X);
		case 2: return FVector2D(-V.X, -V.Y);
		case 3: return FVector2D(V.Y, -V.X);
		default: return V;
		}
	}
	double Sin, Cos;
	FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(YawDegrees(S)));
	return FVector2D(V.X * Cos - V.Y * Sin, V.X * Sin + V.Y * Cos);
}

FVector GLStructureRules::ToWorld(const FGLPlacedPiece& Piece, const TArray<double>& LocalMetres)
{
	const FVector2D XY = RotateXY(FVector2D(Component(LocalMetres, 0), Component(LocalMetres, 1)) * 100.0, Piece.YawStep);
	return Piece.Location + FVector(XY.X, XY.Y, Component(LocalMetres, 2) * 100.0);
}

FGLFootprint GLStructureRules::Footprint(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece)
{
	FGLFootprint F;
	F.Centre = FVector2D(Piece.Location);
	F.AxisX = RotateXY(FVector2D(1.0, 0.0), Piece.YawStep);
	F.AxisY = RotateXY(FVector2D(0.0, 1.0), Piece.YawStep);
	F.Half = FVector2D(Component(Def.Size, 0) * 50.0, Component(Def.Size, 1) * 50.0);
	F.ZMin = Piece.Location.Z;
	F.ZMax = Piece.Location.Z + Component(Def.Size, 2) * 100.0;
	return F;
}

FBox GLStructureRules::Bounds(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece)
{
	return Footprint(Def, Piece).Enclosing();
}

TArray<FGLWorldSocket> GLStructureRules::Sockets(const FGLBuildPieceDef& Def, const FGLPlacedPiece& Piece)
{
	TArray<FGLWorldSocket> Result;
	for (const FGLBuildSocketDef& Socket : Def.Sockets)
	{
		FGLWorldSocket& World = Result.AddDefaulted_GetRef();
		World.Name = FName(*Socket.Name);
		World.Role = Socket.Role;
		World.Location = ToWorld(Piece, Socket.Offset);
		World.bHasFacing = Socket.Facing < NoFacing * 0.5;
		World.Facing = World.bHasFacing ? Wrap360(Socket.Facing + YawDegrees(Piece.YawStep)) : 0.0;
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

EGLPreview GLStructureRules::PreviewOf(const FGLBuildCheck& Check)
{
	if (!Check.IsAllowed())
	{
		return EGLPreview::Red;
	}
	return Check.Support <= Check.VerticalStep + 1e-9 ? EGLPreview::Yellow : EGLPreview::Green;
}

bool GLStructureRules::OverlapsAny(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing, const FGLBuildPieceDef& Def,
	const FGLPlacedPiece& Candidate, int32* OutBlocking)
{
	const FGLFootprint Mine = Footprint(Def, Candidate);
	for (const FGLPlacedPiece& Other : Existing)
	{
		const FGLBuildPieceDef* OtherDef = Content.Find<FGLBuildPieceDef>(Other.Def);
		if (OtherDef && Mine.Overlaps(Footprint(*OtherDef, Other), OverlapShrinkCm))
		{
			if (OutBlocking)
			{
				*OutBlocking = Other.Id;
			}
			return true;
		}
	}
	return false;
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
	Check.VerticalStep = VerticalStepOf(Content, *Def);
	Check.Material = Def->Material;
	int32 Blocking = 0;
	if (OverlapsAny(Content, Existing, *Def, Candidate, &Blocking))
	{
		Check.Refusal = EGLBuildRefusal::Overlaps;
		Check.BlockingPieceId = Blocking;
		Check.Reason = TEXT("something is already there");
		return Check;
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
	Check.Preview = PreviewOf(Check);
	return Check;
}

FGLBuildCheck GLStructureRules::CanPlace(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
	const FGLPlacedPiece& Candidate, FGroundHeight Ground, const FGLKnowledge& Knowledge, TFunctionRef<int32(FName Item)> Available)
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
		if (Available(Cost.Item) < Cost.Count)
		{
			Check.Refusal = EGLBuildRefusal::MissingItems;
			Check.MissingItem = Cost.Item;
			Check.MissingNeeded = Cost.Count;
			Check.MissingHave = Available(Cost.Item);
			Check.Material = Def->Material;
			Check.Reason = FString::Printf(TEXT("needs %d x %s"), Cost.Count, *Cost.Item.ToString());
			return Check;
		}
	}
	return CheckPlacement(Content, Existing, Candidate, Ground);
}

FGLBuildCheck GLStructureRules::CanPlace(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing,
	const FGLPlacedPiece& Candidate, FGroundHeight Ground, const FGLKnowledge& Knowledge, const FGLInventory& Inventory)
{
	return CanPlace(Content, Existing, Candidate, Ground, Knowledge, [&Inventory](FName Item) { return Inventory.CountOf(Item); });
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
	Falling.Sort();
	return Falling;
}

namespace
{
	/** How many of a candidate's bottom sockets sit on a top socket of the existing pieces (how many supports it rests on). */
	int32 RestingSockets(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing, const FGLBuildPieceDef& Def, const FGLPlacedPiece& Candidate)
	{
		constexpr double CoincideCm = 2.0;
		int32 Rests = 0;
		for (const FGLWorldSocket& Ours : GLStructureRules::Sockets(Def, Candidate))
		{
			if (Ours.Role != SocketBottom)
			{
				continue;
			}
			bool bOn = false;
			for (const FGLPlacedPiece& Other : Existing)
			{
				const FGLBuildPieceDef* OtherDef = Content.Find<FGLBuildPieceDef>(Other.Def);
				if (!OtherDef || FVector::Dist2D(Other.Location, Ours.Location) > 1000.0)
				{
					continue;
				}
				for (const FGLWorldSocket& Theirs : GLStructureRules::Sockets(*OtherDef, Other))
				{
					bOn |= Theirs.Role == SocketTop && FVector::Dist(Theirs.Location, Ours.Location) <= CoincideCm;
				}
				if (bOn)
				{
					break;
				}
			}
			Rests += bOn ? 1 : 0;
		}
		return Rests;
	}
}

bool GLStructureRules::Snap(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Existing, FName DefId,
	const FVector& Aim, int32 YawStep, FGroundHeight Ground, FGLPlacedPiece& OutCandidate, FGLSnapInfo* OutInfo, const FVector& AimDirection)
{
	const FVector2D Away = FVector2D(AimDirection).GetSafeNormal();
	FGLSnapInfo Info;
	const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(DefId);
	if (!Def)
	{
		return false;
	}
	FGLPlacedPiece Local;
	Local.Def = DefId;
	Local.YawStep = NormalizeYawStep(YawStep);

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
			// P12: when the piece has several bottom sockets that could rest on this socket (an upper floor's edge
			// midpoints on a wall top), the way it rests on the most supports wins (over the room, on both walls, not
			// hanging outside on one); then the one extending away from the viewer (along the aim); then the one whose
			// centre is nearest the aim: never the data's socket order. Side links keep the first match (angled
			// construction takes its yaw from data).
			int32 BestRests = -1;
			double BestAway = -2.0;
			double BestCentre = TNumericLimits<double>::Max();
			bool bThisSocket = false;
			for (const FGLBuildSocketDef& OursDef : Def->Sockets)
			{
				const bool bSide = OursDef.Role == SocketSide && Theirs.Role == SocketSide;
				const bool bCompatible = (OursDef.Role == SocketBottom && Theirs.Role == SocketTop) || bSide;
				if (!bCompatible || (bThisSocket && bSide))
				{
					continue;
				}
				FGLPlacedPiece Candidate = Local;
				// Angled construction comes from data: two facing side sockets decide the yaw exactly.
				const bool bOursFaces = OursDef.Facing < NoFacing * 0.5;
				if (bSide && bOursFaces && Theirs.bHasFacing)
				{
					Candidate.YawStep = YawStepFromDegrees(Theirs.Facing + 180.0 - OursDef.Facing);
				}
				Candidate.Location = Theirs.Location - (ToWorld(Candidate, OursDef.Offset) - Candidate.Location);
				const double Centre = FVector::Dist2D(Candidate.Location, Aim);
				const int32 Rests = bSide ? 0 : RestingSockets(Content, Existing, *Def, Candidate);
				const double AwayScore = Away.IsZero() ? 0.0 : FMath::RoundToDouble(FVector2D::DotProduct((FVector2D(Candidate.Location) - FVector2D(Theirs.Location)).GetSafeNormal(), Away) * 1000.0);
				const bool bBetter = Rests != BestRests ? Rests > BestRests : AwayScore != BestAway ? AwayScore > BestAway : Centre < BestCentre;
				if ((bThisSocket && !bBetter) || OverlapsAny(Content, Existing, *Def, Candidate)) // the overlap test alone (no support solve per candidate)
				{
					continue;
				}
				BestRests = Rests;
				BestAway = AwayScore;
				BestCentre = Centre;
				bThisSocket = true;
				Best = Distance;
				OutCandidate = Candidate;
				bFound = true;
				Info.bSnapped = true;
				Info.TargetPieceId = Other.Id;
				Info.TargetSocket = Theirs.Name;
				Info.TargetLocation = Theirs.Location;
				Info.OwnSocket = FName(*OursDef.Name);
				Info.bYawFromData = bSide && bOursFaces && Theirs.bHasFacing;
				if (bSide)
				{
					break; // side links: the first match, as before
				}
			}
		}
	}
	if (!bFound && Def->Grounded)
	{
		OutCandidate = Local;
		OutCandidate.Location = FVector(Aim.X, Aim.Y, Ground(FVector2D(Aim)));
		bFound = true;
	}
	if (OutInfo)
	{
		*OutInfo = Info;
	}
	return bFound;
}

bool GLStructureRules::IsUnderStructure(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Pieces, const FVector2D& World, double MarginCm)
{
	for (const FGLPlacedPiece& Piece : Pieces)
	{
		const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Piece.Def);
		if (Def && Def->Grounded && Footprint(*Def, Piece).ContainsXY(World, MarginCm))
		{
			return true;
		}
	}
	return false;
}
