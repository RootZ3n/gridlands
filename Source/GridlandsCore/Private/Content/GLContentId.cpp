#include "Content/GLContentId.h"

namespace GLContentId
{
	namespace
	{
		constexpr int32 MinSegments = 3;
		constexpr int32 MaxSegments = 5;
		constexpr int32 MaxLength = 96;
		constexpr int32 MaxSegmentLength = 32;

		bool Fail(FString* OutProblem, FString Problem)
		{
			if (OutProblem)
			{
				*OutProblem = MoveTemp(Problem);
			}
			return false;
		}

		bool IsLower(TCHAR C) { return C >= TEXT('a') && C <= TEXT('z'); }
		bool IsDigit(TCHAR C) { return C >= TEXT('0') && C <= TEXT('9'); }
	}

	bool IsValid(FStringView Id, FString* OutProblem)
	{
		if (Id.Len() > MaxLength)
		{
			return Fail(OutProblem, FString::Printf(TEXT("id longer than %d characters"), MaxLength));
		}

		TArray<FStringView> Segments;
		int32 Start = 0;
		for (int32 Index = 0; Index <= Id.Len(); ++Index)
		{
			if (Index == Id.Len() || Id[Index] == TEXT('.'))
			{
				Segments.Add(Id.Mid(Start, Index - Start));
				Start = Index + 1;
			}
		}
		if (Segments.Num() < MinSegments || Segments.Num() > MaxSegments)
		{
			return Fail(OutProblem, FString::Printf(TEXT("id needs %d-%d dot-separated segments, has %d"), MinSegments, MaxSegments, Segments.Num()));
		}

		for (const FStringView Segment : Segments)
		{
			if (Segment.Len() == 0 || Segment.Len() > MaxSegmentLength || !IsLower(Segment[0]))
			{
				return Fail(OutProblem, FString::Printf(TEXT("segment '%.*s' must match [a-z][a-z0-9_]{0,31}"), Segment.Len(), Segment.GetData()));
			}
			for (int32 Index = 1; Index < Segment.Len(); ++Index)
			{
				const TCHAR C = Segment[Index];
				if (!IsLower(C) && !IsDigit(C) && C != TEXT('_'))
				{
					return Fail(OutProblem, FString::Printf(TEXT("segment '%.*s' must match [a-z][a-z0-9_]{0,31}"), Segment.Len(), Segment.GetData()));
				}
				if (C == TEXT('_') && Segment[Index - 1] == TEXT('_'))
				{
					return Fail(OutProblem, FString::Printf(TEXT("segment '%.*s' must not contain '__'"), Segment.Len(), Segment.GetData()));
				}
			}
			if (Segment[Segment.Len() - 1] == TEXT('_'))
			{
				return Fail(OutProblem, FString::Printf(TEXT("segment '%.*s' must not end with '_'"), Segment.Len(), Segment.GetData()));
			}
		}
		return true;
	}

	FString KindOf(FStringView Id)
	{
		int32 Dot = INDEX_NONE;
		return Id.FindChar(TEXT('.'), Dot) ? FString(Id.Left(Dot)) : FString(Id);
	}
}
