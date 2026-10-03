#include "Building/GLPendingCollapse.h"

namespace
{
	bool SameBits(double A, double B)
	{
		return FMemory::Memcmp(&A, &B, sizeof(double)) == 0;
	}

	bool SameBits(const FVector& A, const FVector& B)
	{
		return SameBits(A.X, B.X) && SameBits(A.Y, B.Y) && SameBits(A.Z, B.Z);
	}

	bool SameBits(const FTransform& A, const FTransform& B)
	{
		const FQuat QA = A.GetRotation(), QB = B.GetRotation();
		return SameBits(A.GetLocation(), B.GetLocation()) && SameBits(A.GetScale3D(), B.GetScale3D())
			&& SameBits(QA.X, QB.X) && SameBits(QA.Y, QB.Y) && SameBits(QA.Z, QB.Z) && SameBits(QA.W, QB.W);
	}
}

FGLSavedCollapse GLPendingCollapse::Capture(FName Placement, FName Part, const FGLCollapseOutcome& Outcome, double ElapsedSeconds,
	FName Material, FName Cause, FName Credit)
{
	FGLSavedCollapse S;
	S.Placement = Placement;
	S.Part = Part;
	S.ElapsedSeconds = ElapsedSeconds;
	S.Motion = static_cast<uint8>(Outcome.Motion);
	S.StartLocation = Outcome.Start.GetLocation();
	S.StartRotation = Outcome.Start.GetRotation();
	S.RestLocation = Outcome.Rest.GetLocation();
	S.RestRotation = Outcome.Rest.GetRotation();
	S.StartSeconds = Outcome.StartSeconds;
	S.ImpactSeconds = Outcome.ImpactSeconds;
	S.ImpactCentre = Outcome.Impact.Centre;
	S.ImpactAxisX = Outcome.Impact.Axis[0];
	S.ImpactAxisY = Outcome.Impact.Axis[1];
	S.ImpactAxisZ = Outcome.Impact.Axis[2];
	S.ImpactHalfExtent = Outcome.Impact.HalfExtent;
	S.Damage = Outcome.Damage;
	S.Severity = Outcome.Severity;
	S.FallMetres = Outcome.FallMetres;
	S.Pivot = Outcome.Pivot;
	S.TiltAxis = Outcome.TiltAxis;
	S.ToppleDropCm = Outcome.ToppleDropCm;
	S.ToppleHeightCm = Outcome.ToppleHeightCm;
	S.ToppleGravityCmS2 = Outcome.ToppleGravityCmS2;
	S.ToppleStartRadians = Outcome.ToppleStartRadians;
	S.Material = Material;
	S.Cause = Cause;
	S.Credit = Credit;
	return S;
}

bool GLPendingCollapse::Reconstruct(const FGLSavedCollapse& S, FGLCollapseOutcome& Out, FString* OutProblem)
{
	if (S.Motion > static_cast<uint8>(EGLCollapseMotion::Topple))
	{
		if (OutProblem)
		{
			*OutProblem = FString::Printf(TEXT("unknown collapse motion %d"), S.Motion);
		}
		return false;
	}
	Out = FGLCollapseOutcome();
	Out.Motion = static_cast<EGLCollapseMotion>(S.Motion);
	Out.Start = FTransform(S.StartRotation, S.StartLocation);
	Out.Rest = FTransform(S.RestRotation, S.RestLocation);
	Out.StartSeconds = S.StartSeconds;
	Out.ImpactSeconds = S.ImpactSeconds;
	Out.Impact.Centre = S.ImpactCentre;
	Out.Impact.Axis[0] = S.ImpactAxisX;
	Out.Impact.Axis[1] = S.ImpactAxisY;
	Out.Impact.Axis[2] = S.ImpactAxisZ;
	Out.Impact.HalfExtent = S.ImpactHalfExtent;
	Out.Damage = S.Damage;
	Out.Severity = S.Severity;
	Out.FallMetres = S.FallMetres;
	Out.Pivot = S.Pivot;
	Out.TiltAxis = S.TiltAxis;
	Out.ToppleDropCm = S.ToppleDropCm;
	Out.ToppleHeightCm = S.ToppleHeightCm;
	Out.ToppleGravityCmS2 = S.ToppleGravityCmS2;
	Out.ToppleStartRadians = S.ToppleStartRadians;
	if (Out.Motion == EGLCollapseMotion::Topple)
	{
		const double Seconds = GLCollapseRules::IntegrateTopple(S.ToppleHeightCm, S.ToppleGravityCmS2, S.ToppleStartRadians, Out.ToppleAngles);
		// The plan computed ImpactSeconds as StartSeconds + the same integration: anything else is not that plan.
		if (!SameBits(Out.StartSeconds + Seconds, Out.ImpactSeconds))
		{
			if (OutProblem)
			{
				*OutProblem = FString::Printf(TEXT("topple re-integrates to %.17g s, the record says %.17g s"), Out.StartSeconds + Seconds, Out.ImpactSeconds);
			}
			return false;
		}
	}
	return true;
}

bool GLPendingCollapse::Identical(const FGLCollapseOutcome& A, const FGLCollapseOutcome& B, FString* OutFirstDifference)
{
	auto Differ = [OutFirstDifference](const TCHAR* Field)
	{
		if (OutFirstDifference)
		{
			*OutFirstDifference = Field;
		}
		return false;
	};
	if (A.Motion != B.Motion) return Differ(TEXT("Motion"));
	if (!SameBits(A.Start, B.Start)) return Differ(TEXT("Start"));
	if (!SameBits(A.Rest, B.Rest)) return Differ(TEXT("Rest"));
	if (!SameBits(A.StartSeconds, B.StartSeconds)) return Differ(TEXT("StartSeconds"));
	if (!SameBits(A.ImpactSeconds, B.ImpactSeconds)) return Differ(TEXT("ImpactSeconds"));
	if (!SameBits(A.Impact.Centre, B.Impact.Centre)) return Differ(TEXT("Impact.Centre"));
	for (int32 I = 0; I < 3; ++I)
	{
		if (!SameBits(A.Impact.Axis[I], B.Impact.Axis[I])) return Differ(TEXT("Impact.Axis"));
	}
	if (!SameBits(A.Impact.HalfExtent, B.Impact.HalfExtent)) return Differ(TEXT("Impact.HalfExtent"));
	if (!SameBits(A.Damage, B.Damage)) return Differ(TEXT("Damage"));
	if (!SameBits(A.Severity, B.Severity)) return Differ(TEXT("Severity"));
	if (!SameBits(A.FallMetres, B.FallMetres)) return Differ(TEXT("FallMetres"));
	if (!SameBits(A.Pivot, B.Pivot)) return Differ(TEXT("Pivot"));
	if (!SameBits(A.TiltAxis, B.TiltAxis)) return Differ(TEXT("TiltAxis"));
	if (!SameBits(A.ToppleDropCm, B.ToppleDropCm)) return Differ(TEXT("ToppleDropCm"));
	if (!SameBits(A.ToppleHeightCm, B.ToppleHeightCm)) return Differ(TEXT("ToppleHeightCm"));
	if (!SameBits(A.ToppleGravityCmS2, B.ToppleGravityCmS2)) return Differ(TEXT("ToppleGravityCmS2"));
	if (!SameBits(A.ToppleStartRadians, B.ToppleStartRadians)) return Differ(TEXT("ToppleStartRadians"));
	if (A.ToppleAngles.Num() != B.ToppleAngles.Num()) return Differ(TEXT("ToppleAngles.Num"));
	for (int32 I = 0; I < A.ToppleAngles.Num(); ++I)
	{
		if (!SameBits(A.ToppleAngles[I], B.ToppleAngles[I])) return Differ(TEXT("ToppleAngles"));
	}
	return true;
}
