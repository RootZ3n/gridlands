#include "Building/GLCollapseRules.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"

namespace
{
	constexpr double NoFallCm = 5.0;       // landing this close to where it was is not a fall (no damage)
	constexpr double SurfaceShrinkCm = 1.0; // touching edges are not "over" each other

	FTransform StartOf(const FGLPlacedPiece& Piece)
	{
		return FTransform(FRotator(0.0, 90.0 * Piece.YawQuarter, 0.0), Piece.Location);
	}

	bool OverlapsXY(const FBox& A, const FBox& B)
	{
		return A.Min.X < B.Max.X - SurfaceShrinkCm && B.Min.X < A.Max.X - SurfaceShrinkCm
			&& A.Min.Y < B.Max.Y - SurfaceShrinkCm && B.Min.Y < A.Max.Y - SurfaceShrinkCm;
	}

	/** The highest ground under a footprint (corners, edge midpoints and centre). */
	double GroundUnder(const FBox& Box, GLStructureRules::FGroundHeight Ground)
	{
		double Highest = -1e12;
		for (double U : { 0.0, 0.5, 1.0 })
		{
			for (double V : { 0.0, 0.5, 1.0 })
			{
				Highest = FMath::Max(Highest, Ground(FVector2D(FMath::Lerp(Box.Min.X, Box.Max.X, U), FMath::Lerp(Box.Min.Y, Box.Max.Y, V))));
			}
		}
		return Highest;
	}

	/** World bounds of a piece's local box (bottom-centre origin) under a transform. */
	FBox BoxUnder(const FGLBuildPieceDef& Def, const FTransform& Transform)
	{
		const FVector Half(Def.Size.IsValidIndex(0) ? Def.Size[0] * 50.0 : 0.0, Def.Size.IsValidIndex(1) ? Def.Size[1] * 50.0 : 0.0, 0.0);
		const double Height = Def.Size.IsValidIndex(2) ? Def.Size[2] * 100.0 : 0.0;
		FBox Box(ForceInit);
		for (double X : { -Half.X, Half.X })
		{
			for (double Y : { -Half.Y, Half.Y })
			{
				for (double Z : { 0.0, Height })
				{
					Box += Transform.TransformPosition(FVector(X, Y, Z));
				}
			}
		}
		return Box;
	}

	double DamageFor(double FallMetres, double Scale, const FGLCollapseTuningDef& Tuning)
	{
		if (FallMetres * 100.0 < NoFallCm)
		{
			return 0.0;
		}
		return FMath::Clamp((Tuning.DamageBase + Tuning.DamagePerMetreFallen * FallMetres) * Scale, 0.0, Tuning.DamageMax);
	}

	FVector2D ToppleDirection(const FGLCollapseRequest& Request, const FBox& Box, const FVector& Instigator)
	{
		const FVector2D Forward = FVector2D(FRotator(0.0, 90.0 * Request.Piece.YawQuarter, 0.0).Vector());
		switch (Request.Direction)
		{
		case EGLToppleDirection::PieceForward: return Forward;
		case EGLToppleDirection::PieceBackward: return -Forward;
		case EGLToppleDirection::AwayFromInstigator:
		default:
		{
			const FVector2D Away = FVector2D(Box.GetCenter()) - FVector2D(Instigator);
			return Away.IsNearlyZero() ? Forward : Away.GetSafeNormal();
		}
		}
	}
}

bool FGLImpactVolume::Touches(const FVector& Point, double RadiusCm) const
{
	const FVector Local = Point - Centre;
	FVector Closest = Centre;
	for (int32 I = 0; I < 3; ++I)
	{
		const double Along = FMath::Clamp(FVector::DotProduct(Local, Axis[I]), -HalfExtent[I], HalfExtent[I]);
		Closest += Axis[I] * Along;
	}
	return FVector::DistSquared(Point, Closest) <= RadiusCm * RadiusCm;
}

EGLCollapseMotion GLCollapseRules::MotionFromData(FName Motion)
{
	return Motion == TEXT("topple") ? EGLCollapseMotion::Topple : EGLCollapseMotion::Drop;
}

EGLToppleDirection GLCollapseRules::DirectionFromData(FName Direction)
{
	if (Direction == TEXT("pieceForward"))
	{
		return EGLToppleDirection::PieceForward;
	}
	if (Direction == TEXT("pieceBackward"))
	{
		return EGLToppleDirection::PieceBackward;
	}
	return EGLToppleDirection::AwayFromInstigator;
}

TArray<int32> GLCollapseRules::Unsupported(const FGLContentRegistry& Content, TConstArrayView<FGLPlacedPiece> Remaining,
	GLStructureRules::FGroundHeight Ground)
{
	const TMap<int32, double> Support = GLStructureRules::ComputeSupport(Content, Remaining, Ground);
	TArray<int32> Out;
	for (const FGLPlacedPiece& Piece : Remaining)
	{
		if (Support.FindRef(Piece.Id) <= 1e-6)
		{
			Out.Add(Piece.Id);
		}
	}
	Out.Sort();
	return Out;
}

FGLCollapsePlan GLCollapseRules::Plan(const FGLContentRegistry& Content, TConstArrayView<FGLCollapseRequest> Collapsing,
	TConstArrayView<FGLPlacedPiece> Standing, GLStructureRules::FGroundHeight Ground, const FVector& Instigator,
	const FGLCollapseTuningDef& Tuning)
{
	const double G = FMath::Max(1.0, Tuning.Gravity * 100.0); // cm/s^2
	const double Margin = Tuning.ImpactMarginMetres * 100.0;
	const double Delay = FMath::Max(0.0, Tuning.StartDelaySeconds);

	// Surfaces debris can land on: what stays standing, then each settled piece in turn.
	TArray<FBox> Surfaces;
	for (const FGLPlacedPiece& Piece : Standing)
	{
		if (const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Piece.Def))
		{
			Surfaces.Add(GLStructureRules::Bounds(*Def, Piece));
		}
	}

	// Lowest first, so a higher piece lands on a lower one that fell before it. Ties by id.
	TArray<FGLCollapseRequest> Ordered(Collapsing.GetData(), Collapsing.Num());
	Ordered.Sort([](const FGLCollapseRequest& A, const FGLCollapseRequest& B)
	{
		return A.Piece.Location.Z != B.Piece.Location.Z ? A.Piece.Location.Z < B.Piece.Location.Z : A.Piece.Id < B.Piece.Id;
	});

	FGLCollapsePlan Plan;
	for (const FGLCollapseRequest& Request : Ordered)
	{
		const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Request.Piece.Def);
		if (!Def)
		{
			continue;
		}
		const FBox Box = GLStructureRules::Bounds(*Def, Request.Piece);
		FGLCollapseOutcome Out;
		Out.PieceId = Request.Piece.Id;
		Out.Def = Request.Piece.Def;
		Out.Motion = Request.Motion;
		Out.Start = StartOf(Request.Piece);
		Out.StartSeconds = Delay;

		// The surface under it: the ground, or anything standing (or already fallen) beneath its centre of
		// mass. A sliver of overlap (an awning's edge over a wall top) does not hold a falling piece.
		double Landing = GroundUnder(Box, Ground);
		const FVector2D Centre(Box.GetCenter());
		for (const FBox& Surface : Surfaces)
		{
			const bool bUnderCentre = Centre.X >= Surface.Min.X && Centre.X <= Surface.Max.X && Centre.Y >= Surface.Min.Y && Centre.Y <= Surface.Max.Y;
			if (bUnderCentre && OverlapsXY(Box, Surface) && Surface.Max.Z <= Box.Min.Z + SurfaceShrinkCm)
			{
				Landing = FMath::Max(Landing, Surface.Max.Z);
			}
		}
		Landing = FMath::Min(Landing, Box.Min.Z); // never rises
		if (Request.Motion == EGLCollapseMotion::Drop)
		{
			const double Fall = Box.Min.Z - Landing;
			Out.Rest = Out.Start;
			Out.Rest.AddToTranslation(FVector(0.0, 0.0, -Fall));
			Out.ImpactSeconds = Delay + FMath::Sqrt(2.0 * Fall / G);
			Out.FallMetres = Fall / 100.0;
			// What is under it when it lands is hit: its footprint, from where it lands up to where it was.
			Out.Impact.Centre = FVector(Box.GetCenter().X, Box.GetCenter().Y, (Landing + Box.Max.Z) * 0.5);
			Out.Impact.HalfExtent = FVector(Box.GetExtent().X + Margin, Box.GetExtent().Y + Margin, FMath::Max(1.0, (Box.Max.Z - Landing) * 0.5));
			Surfaces.Add(Box.ShiftBy(FVector(0.0, 0.0, -Fall)));
		}
		else
		{
			const FVector2D Direction = ToppleDirection(Request, Box, Instigator);
			const FVector D(Direction, 0.0);
			const FVector Across(-Direction.Y, Direction.X, 0.0);
			const FVector Extent = Box.GetExtent();
			const double AlongHalf = FMath::Abs(Direction.X) * Extent.X + FMath::Abs(Direction.Y) * Extent.Y;
			const double AcrossHalf = FMath::Abs(Across.X) * Extent.X + FMath::Abs(Across.Y) * Extent.Y;
			const double Height = FMath::Max(1.0, Box.Max.Z - Box.Min.Z);
			// It comes down onto what is beneath it (what held it is gone), then tips about its base edge there.
			Out.ToppleDropCm = Box.Min.Z - Landing;
			Out.Pivot = FVector(FVector2D(Box.GetCenter()) + Direction * AlongHalf, Landing);
			// Rotating +90 degrees about Up x D takes Up to D: the top falls toward D.
			Out.TiltAxis = FVector::CrossProduct(FVector::UpVector, D).GetSafeNormal();

			// A uniform rod pivoting at its base: theta'' = (3 g / 2 L) sin(theta). Fixed-step, deterministic.
			const double Omega2 = 3.0 * G / (2.0 * Height);
			double Theta = FMath::DegreesToRadians(FMath::Clamp(Tuning.ToppleStartDegrees, 0.1, 45.0));
			double Rate = 0.0, Time = 0.0, NextSample = 0.0;
			constexpr double Step = 0.0005;
			while (Theta < UE_HALF_PI && Time < 30.0)
			{
				if (Time >= NextSample)
				{
					Out.ToppleAngles.Add(Theta);
					NextSample += ToppleSampleSeconds;
				}
				Rate += Omega2 * FMath::Sin(Theta) * Step;
				Theta += Rate * Step;
				Time += Step;
			}
			Out.ToppleAngles.Add(UE_HALF_PI);
			Out.ImpactSeconds = Delay + Time;

			const FQuat Tilt(Out.TiltAxis, UE_HALF_PI);
			const FVector Lowered = Out.Start.GetLocation() - FVector(0.0, 0.0, Out.ToppleDropCm);
			FTransform Rest(Tilt * Out.Start.GetRotation(), Out.Pivot + Tilt.RotateVector(Lowered - Out.Pivot));
			// Lying down, it rests on the highest ground beneath it (never sinks into a slope, never floats).
			const FBox Lying = BoxUnder(*Def, Rest);
			const double Lift = GroundUnder(Lying, Ground) - Lying.Min.Z;
			Rest.AddToTranslation(FVector(0.0, 0.0, Lift));
			Out.Rest = Rest;
			Out.FallMetres = Height / 200.0; // its centre of mass comes down from half its height
			const FBox Landed = Lying.ShiftBy(FVector(0.0, 0.0, Lift));
			const double ImpactHeight = FMath::Max(Tuning.ImpactHeightMetres * 100.0, Landed.Max.Z - Landed.Min.Z);
			Out.Impact.Axis[0] = D;
			Out.Impact.Axis[1] = Across;
			Out.Impact.Axis[2] = FVector::UpVector;
			Out.Impact.HalfExtent = FVector(Height * 0.5 + Margin, AcrossHalf + Margin, ImpactHeight * 0.5);
			Out.Impact.Centre = FVector(FVector2D(Out.Pivot) + Direction * Height * 0.5, Landed.Min.Z + ImpactHeight * 0.5);
			Surfaces.Add(Landed);
		}
		Out.Damage = DamageFor(Out.FallMetres, Request.DamageScale, Tuning);
		Plan.Outcomes.Add(MoveTemp(Out));
	}
	return Plan;
}

FTransform GLCollapseRules::Motion(const FGLCollapseOutcome& Outcome, double Seconds)
{
	if (Seconds <= Outcome.StartSeconds)
	{
		return Outcome.Start;
	}
	if (Seconds >= Outcome.ImpactSeconds)
	{
		return Outcome.Rest;
	}
	const double Moving = Seconds - Outcome.StartSeconds;
	if (Outcome.Motion == EGLCollapseMotion::Drop)
	{
		const double Total = Outcome.Start.GetLocation().Z - Outcome.Rest.GetLocation().Z;
		const double Span = FMath::Max(UE_SMALL_NUMBER, Outcome.ImpactSeconds - Outcome.StartSeconds);
		FTransform Pose = Outcome.Start;
		Pose.AddToTranslation(FVector(0.0, 0.0, -Total * FMath::Square(Moving / Span))); // uniform acceleration
		return Pose;
	}
	const double Index = Moving / ToppleSampleSeconds;
	const int32 Low = FMath::Clamp(FMath::FloorToInt(Index), 0, Outcome.ToppleAngles.Num() - 1);
	const int32 High = FMath::Min(Low + 1, Outcome.ToppleAngles.Num() - 1);
	const double Angle = Outcome.ToppleAngles.Num() ? FMath::Lerp(Outcome.ToppleAngles[Low], Outcome.ToppleAngles[High], Index - Low) : 0.0;
	// It settles onto its pivot surface over the first fifth of the topple (no pop).
	const double Span = FMath::Max(UE_SMALL_NUMBER, Outcome.ImpactSeconds - Outcome.StartSeconds);
	const FVector Lowered = Outcome.Start.GetLocation() - FVector(0.0, 0.0, Outcome.ToppleDropCm * FMath::Min(1.0, Moving / (0.2 * Span)));
	const FVector Pivot = Outcome.Pivot + FVector(0.0, 0.0, Outcome.ToppleDropCm * (1.0 - FMath::Min(1.0, Moving / (0.2 * Span))));
	const FQuat Tilt(Outcome.TiltAxis, Angle);
	return FTransform(Tilt * Outcome.Start.GetRotation(), Pivot + Tilt.RotateVector(Lowered - Pivot));
}
