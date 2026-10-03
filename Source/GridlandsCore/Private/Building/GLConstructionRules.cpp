#include "Building/GLConstructionRules.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Knowledge/GLKnowledge.h"

FName GLConstructionRules::PhaseOf(const FGLContentRegistry& Content, FName Layer)
{
	const FGLFinishDef* Finish = Content.Find<FGLFinishDef>(Layer);
	return Finish ? Finish->Phase : NAME_None;
}

FGLInstallCheck GLConstructionRules::CanInstall(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece, FName Layer, const FGLKnowledge* Knowledge)
{
	FGLInstallCheck Check;
	const FGLBuildPieceDef* Form = Content.Find<FGLBuildPieceDef>(Piece.Def);
	const FGLFinishDef* Finish = Content.Find<FGLFinishDef>(Layer);
	if (!Form || !Finish)
	{
		Check.Refusal = EGLInstallRefusal::UnknownLayer;
		Check.Reason = FString::Printf(TEXT("unknown layer %s"), *Layer.ToString());
		return Check;
	}
	const FGLPhaseDef* Phase = Content.Find<FGLPhaseDef>(Finish->Phase);
	if (!Phase)
	{
		Check.Refusal = EGLInstallRefusal::UnknownPhase;
		Check.Reason = TEXT("that layer belongs to no construction phase");
		return Check;
	}
	if (!Phase->Implemented)
	{
		Check.Refusal = EGLInstallRefusal::PhaseNotImplemented;
		Check.Reason = FString::Printf(TEXT("%s is not built yet"), *Phase->DisplayName);
		return Check;
	}
	if (!Form->AcceptsPhase(Finish->Phase))
	{
		Check.Refusal = EGLInstallRefusal::FormRefusesPhase;
		Check.Reason = Form->Layers.Num() == 0 ? TEXT("it is complete as built") : TEXT("that does not go on this");
		return Check;
	}
	if (!Finish->FitsRoles.Contains(Form->Role))
	{
		Check.Refusal = EGLInstallRefusal::RoleMismatch;
		Check.Reason = FString::Printf(TEXT("%s does not fit a %s"), *Finish->DisplayName, *Form->Role.ToString());
		return Check;
	}
	for (const FName& Installed : Piece.Layers)
	{
		const FName InstalledPhase = PhaseOf(Content, Installed);
		if (InstalledPhase == Finish->Phase)
		{
			Check.Refusal = EGLInstallRefusal::AlreadyInstalled;
			Check.Reason = TEXT("it already has that");
			return Check;
		}
		const FGLPhaseDef* Other = Content.Find<FGLPhaseDef>(InstalledPhase);
		if (Other && Other->Order > Phase->Order)
		{
			Check.Refusal = EGLInstallRefusal::OutOfOrder;
			Check.Reason = FString::Printf(TEXT("%s goes on before %s"), *Phase->DisplayName, *Other->DisplayName);
			return Check;
		}
	}
	TArray<FName> Missing;
	if (Knowledge && !Knowledge->KnowsAll(Finish->UnlockedBy, &Missing))
	{
		Check.Refusal = EGLInstallRefusal::NotKnown;
		Check.Reason = TEXT("Zenny doesn't know how to do that yet");
	}
	return Check;
}

FGLPlacedPiece GLConstructionRules::WithLayer(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece, FName Layer)
{
	FGLPlacedPiece Out = Piece;
	Out.Layers.Add(Layer);
	auto OrderOf = [&Content](FName L)
	{
		const FGLPhaseDef* Phase = Content.Find<FGLPhaseDef>(PhaseOf(Content, L));
		return Phase ? Phase->Order : MAX_int32;
	};
	Out.Layers.StableSort([&OrderOf](const FName& A, const FName& B) { return OrderOf(A) < OrderOf(B); });
	return Out;
}

bool GLConstructionRules::ShowsFrame(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece)
{
	const FGLBuildPieceDef* Form = Content.Find<FGLBuildPieceDef>(Piece.Def);
	if (!Form || Form->FrameShapes.Num() == 0)
	{
		return false;
	}
	// It shows its frame until it has a layer of the last implemented phase it accepts (its finish).
	FName Last;
	for (const FName& Accepted : Form->Layers)
	{
		const FGLPhaseDef* Phase = Content.Find<FGLPhaseDef>(Accepted);
		if (Phase && Phase->Implemented)
		{
			Last = Accepted;
		}
	}
	return !Last.IsNone() && !Piece.Layers.ContainsByPredicate([&](const FName& L) { return PhaseOf(Content, L) == Last; });
}

const TArray<FGLSalvageYieldDef>& GLConstructionRules::YieldsFor(const FGLSalvageDef& Salvage, EGLSalvagePath Path)
{
	switch (Path)
	{
	case EGLSalvagePath::Destructive: return Salvage.YieldsByPath.Destructive.Num() ? Salvage.YieldsByPath.Destructive : Salvage.Yields;
	case EGLSalvagePath::Collapse: return Salvage.YieldsByPath.Collapse.Num() ? Salvage.YieldsByPath.Collapse : Salvage.Yields;
	case EGLSalvagePath::Careful:
	default: return Salvage.YieldsByPath.Careful.Num() ? Salvage.YieldsByPath.Careful : Salvage.Yields;
	}
}

TArray<FGLSalvageYieldDef> GLConstructionRules::PieceYields(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece, EGLSalvagePath Path)
{
	TArray<FGLSalvageYieldDef> Out;
	auto AddFrom = [&](FName SalvageId)
	{
		if (const FGLSalvageDef* Salvage = Content.Find<FGLSalvageDef>(SalvageId))
		{
			for (const FGLSalvageYieldDef& Yield : YieldsFor(*Salvage, Path))
			{
				FGLSalvageYieldDef* Same = Out.FindByPredicate([&Yield](const FGLSalvageYieldDef& Y) { return Y.Item == Yield.Item && Y.YieldCategory == Yield.YieldCategory; });
				if (Same)
				{
					Same->Count += Yield.Count;
				}
				else
				{
					Out.Add(Yield);
				}
			}
		}
	};
	if (const FGLBuildPieceDef* Form = Content.Find<FGLBuildPieceDef>(Piece.Def))
	{
		AddFrom(Form->Salvage);
	}
	for (const FName& Layer : Piece.Layers)
	{
		if (const FGLFinishDef* Finish = Content.Find<FGLFinishDef>(Layer))
		{
			AddFrom(Finish->Salvage);
		}
	}
	return Out;
}
