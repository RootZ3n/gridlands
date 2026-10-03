// P10 (ADR-0038): structural environmental resolution, in the P9 dev proof room with the ordinary P6 carport.
// SUPPORT FAILED decides the fall (trajectory, rest, impact time and volume), never who it affects; IMPACT decides
// from where everything is then: a susceptible creature under a severe enough impact is NEUTRALIZED (Pinned: no
// damage, no death, no kill), anything else is damaged through the health system. Save/restart and streaming never
// change an outcome: every timeline here runs once straight through and again broken by a save/restart or an unload
// at a chosen moment, and the two must agree, with nothing duplicated.
//
// Targets are moved by script (a teleport, written through to the model as the creature's own step would): the
// question is the impact's authority over where a target is, not how the AI chooses to move.

#include "Building/GLCollapseRules.h"
#include "Character/GLCharacter.h"
#include "Combat/GLCreature.h"
#include "Combat/GLCreatureRules.h"
#include "Combat/GLHealthComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLStructuralTests
{
	const FName SRLots(TEXT("cell.outer.diner_lots"));
	const FString SRSlot(TEXT("gl-structural-test"));
	const FVector SRFarAway(0, -1200, 100); // the origin: the lots stream out
	constexpr int32 SRGremlin = 21; // a patrol gremlin: 60 hp, not susceptible to Neutralize.Pinned
	constexpr int32 SRWarden = 25;  // 240 hp, neutralizableBy Contained and Pinned
	constexpr int32 SRCarport = 29;
	constexpr double SRDt = 1.0 / 60.0;
	const FName SRDeckEast(TEXT("deck_east"));

	FName SRId(int32 Index) { return UGLPlacementSubsystem::DungeonProofId(Index); }

	/** Does any of a part's boxes collide (debris at rest is solid; a falling part is not)? */
	bool SRSolid(const AActor* Part)
	{
		TInlineComponentArray<UPrimitiveComponent*> Boxes(Part);
		return Boxes.ContainsByPredicate([](const UPrimitiveComponent* C) { return C->IsCollisionEnabled(); });
	}

	struct FSRScene
	{
		GLTestUtils::FTestWorld Test;
		AGLCharacter* Zenny = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLPlacementSubsystem* Placements = nullptr;
		UGLStructureSubsystem* Structures = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		UGLNoiseSubsystem* Noise = nullptr;
		TMap<FName, int32> Events;
		double TargetDamage = 0.0;
		TWeakObjectPtr<AGLCreature> Watched;

		FSRScene(const TCHAR* Name, bool bStartFar = false) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { ++Events.FindOrAdd(Event.Tag.GetTagName()); }));
			Placements = World->GetSubsystem<UGLPlacementSubsystem>();
			Placements->AddDungeonProof(); // before any cell streams in
			Structures = World->GetSubsystem<UGLStructureSubsystem>();
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			Noise = World->GetSubsystem<UGLNoiseSubsystem>();
			Zenny = World->SpawnActor<AGLCharacter>(SRFarAway, FRotator::ZeroRotator);
			World->GetSubsystem<UGLGlitchSubsystem>()->SetCommander(Zenny);
			Grid = World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false);
			GoTo(bStartFar ? SRFarAway : Salvager());
		}

		FVector Room(double X, double Y, double Up = 100.0) const
		{
			const FVector2D At(102400.0 + UGLPlacementSubsystem::DungeonProofOrigin.X + X, UGLPlacementSubsystem::DungeonProofOrigin.Y + Y);
			return FVector(At, (Terrain && Terrain->HasGroundAt(At) ? Terrain->HeightAt(At) : 0.0) + Up);
		}

		/** Where Zenny stands to take the posts out: west of them, outside every impact volume. */
		FVector Salvager() const { return Room(450, 500, 90); }

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Grid->Advance(Where);
			Grid->FlushAll();
		}

		/** The live part's piece (its world bottom centre). */
		FVector PartAt(FName Part) const
		{
			const FGLStructureRuntime* R = Structures->Find(SRId(SRCarport));
			const FGLStructurePartRuntime* P = R ? R->Parts.FindByPredicate([Part](const FGLStructurePartRuntime& X) { return X.Name == Part; }) : nullptr;
			return P ? P->Piece.Location : FVector::ZeroVector;
		}

		/** A ground point (creature feet) relative to the east deck's centre. */
		FVector EastDeck(double DX, double DY) const
		{
			const FVector Deck = PartAt(SRDeckEast);
			const FVector2D At(Deck.X + DX, Deck.Y + DY);
			return FVector(At, Terrain->HeightAt(At));
		}

		AGLCreature* Creature(int32 Index) const { return Placements->FindCreature(SRId(Index)); }
		const FGLActorPlacement* Model(int32 Index) const { return Placements->FindActorModel(SRId(Index)); }

		/** A target stands at Feet: its actor there, written through to its model (as its own step writes it). */
		void Put(int32 Index, const FVector& Feet)
		{
			if (AGLCreature* C = Creature(Index))
			{
				C->SetActorLocation(Feet + FVector(0, 0, 70), false, nullptr, ETeleportType::TeleportPhysics);
				Placements->SyncCreatureFromActor(*C);
				if (Watched.Get() != C)
				{
					Watched = C;
					C->GetHealth()->OnDamaged.AddLambda([this](double Taken, AActor*) { TargetDamage += Taken; });
				}
			}
		}

		/** The ordinary salvage pipeline: hit until it comes away (Zenny's interaction). */
		bool Salvage(FName Part)
		{
			AGLStructurePart* Actor = Structures->FindPart(SRId(SRCarport), Part);
			for (int32 Hit = 0; Actor && Hit < 50 && !Actor->GetSalvageable()->IsSalvaged(); ++Hit)
			{
				Actor->GetSalvageable()->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			}
			return Actor && Actor->GetSalvageable()->IsSalvaged();
		}

		/** The east deck's collapse in flight (null before its support fails and after its impact). */
		const FGLActiveCollapse* Falling() const
		{
			return Structures->GetActive().FindByPredicate([](const FGLActiveCollapse& C) { return C.Part == SRDeckEast && C.Placement == SRId(SRCarport); });
		}

		void Advance(double Dt)
		{
			Test.World->TimeSeconds += Dt;
			Structures->Advance(Dt);
		}

		int32 Count(const TCHAR* Tag) const { return Events.FindRef(FName(Tag)); }
		int32 Residue() const { return Zenny->GetInventory()->CountOf(TEXT("item.material.static_residue")); }
		bool Save() const { return Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(SRSlot); }
		bool Load(TArray<FString>& Problems) { return Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(SRSlot, &Problems); }
	};

	/** A target's path: where its feet are at T seconds since the support failed (T < 0: before). */
	using FSRPath = TFunction<FVector(const FSRScene&, double)>;

	enum class ESRBreak : uint8 { None, SaveRestart, Unload };

	/** Everything a timeline produced, summed over every world it ran in. */
	struct FSRResult
	{
		EGLCreatureOutcome Outcome = EGLCreatureOutcome::None;
		FName How;
		double Health = -2.0;
		FVector HeldAt = FVector::ZeroVector;
		/** Where the path had the target at the end (the impact decided on this, not on where it was at failure). */
		FVector FinalPathAt = FVector::ZeroVector;
		double TargetDamage = 0.0;
		double ZennyHealth = -1.0;
		int32 Residue = 0;
		int32 Collapsed = 0, Impacts = 0, Neutralized = 0, Defeated = 0, Resolved = 0, CollapseNoise = 0;
		int32 Problems = 0;
		bool bBroke = false;

		FString Describe() const
		{
			return FString::Printf(TEXT("outcome %d %s, health %.1f, damage %.1f, Zenny %.0f, residue %d; collapsed %d, impacts %d, neutralized %d, defeated %d, resolved %d, collapse noise %d"),
				static_cast<int32>(Outcome), *How.ToString(), Health, TargetDamage, ZennyHealth, Residue, Collapsed, Impacts, Neutralized, Defeated, Resolved, CollapseNoise);
		}

		bool SameAs(const FSRResult& O) const
		{
			return Outcome == O.Outcome && How == O.How && Health == O.Health && HeldAt.Equals(O.HeldAt, 0.001) && TargetDamage == O.TargetDamage
				&& ZennyHealth == O.ZennyHealth && Residue == O.Residue && Collapsed == O.Collapsed && Impacts == O.Impacts && Neutralized == O.Neutralized
				&& Defeated == O.Defeated && Resolved == O.Resolved && CollapseNoise == O.CollapseNoise;
		}
	};

	void SRTally(FSRResult& R, const FSRScene& S)
	{
		R.Collapsed += S.Count(TEXT("Event.Structure.Collapsed"));
		R.Impacts += S.Count(TEXT("Event.Structure.Impact"));
		R.Neutralized += S.Count(TEXT("Event.Creature.Neutralized"));
		R.Defeated += S.Count(TEXT("Event.Creature.Defeated"));
		R.Resolved += S.Count(TEXT("Event.Encounter.Resolved"));
		R.CollapseNoise += S.Noise->CountOf(TEXT("Noise.Structure.Collapse"));
		R.TargetDamage += S.TargetDamage;
	}

	/**
	 * Runs one timeline: the target follows Path; Zenny takes the first post (the carport stands: redundancy), then the
	 * last one (support fails). If Break is set it happens once the fall's elapsed time reaches BreakAt (< 0: before the
	 * support is touched; past the impact: after it), with a step that lands exactly there. Baselines run with the same
	 * BreakAt and no break, so both step through the same instants.
	 */
	FSRResult SRRun(FAutomationTestBase& T, const FString& Name, int32 Target, const FSRPath& Path, double BreakAt, ESRBreak Break, double PreWound = 0.0)
	{
		FSRResult Result;
		TUniquePtr<FSRScene> Live = MakeUnique<FSRScene>(*Name);
		bool bBroken = false;
		auto BreakNow = [&]()
		{
			bBroken = true;
			Result.bBroke = true;
			if (Break == ESRBreak::SaveRestart)
			{
				T.TestTrue(TEXT("saves"), Live->Save());
				SRTally(Result, *Live);
				Live.Reset(); // the old world is gone before the restart
				Live = MakeUnique<FSRScene>(*(Name + TEXT("Restart")), true);
				TArray<FString> Problems;
				T.TestTrue(TEXT("restart loads"), Live->Load(Problems));
				Result.Problems += Problems.Num();
				Live->GoTo(Live->Salvager());
			}
			else if (Break == ESRBreak::Unload)
			{
				Live->GoTo(SRFarAway);
				T.TestFalse(TEXT("the lots streamed out"), Live->Grid->IsLoaded(SRLots));
				for (int32 I = 0; I < 300; ++I) // 5 s of world time away: nothing there moves, the fall included
				{
					Live->Advance(SRDt);
				}
				Live->GoTo(Live->Salvager());
			}
		};
		Live->Put(Target, Path(*Live, -1.0));
		if (PreWound > 0.0 && Live->Creature(Target))
		{
			Live->Creature(Target)->GetHealth()->ApplyDamage(PreWound, Live->Zenny); // an earlier fight (its model keeps the wound)
		}
		if (Break != ESRBreak::None && BreakAt < 0.0)
		{
			BreakNow();
		}
		Live->Zenny->SetActorLocation(Live->Salvager());
		Live->Put(Target, Path(*Live, -1.0));
		T.TestTrue(TEXT("the first post comes away"), Live->Salvage(TEXT("post_north")));
		T.TestNull(TEXT("  and the carport still stands (redundancy)"), Live->Falling());
		Live->Put(Target, Path(*Live, 0.0));
		T.TestTrue(TEXT("the last post comes away"), Live->Salvage(TEXT("post_south")));
		T.TestNotNull(TEXT("  and the decks lose their support"), Live->Falling());
		double Last = 0.0;
		for (int32 Frame = 0, After = 0; Frame < 2000 && After < 30; ++Frame)
		{
			const FGLActiveCollapse* F = Live->Falling();
			const double Elapsed = F ? F->Elapsed : -1.0;
			if (Break != ESRBreak::None && !bBroken && (Elapsed < 0.0 || Elapsed >= BreakAt))
			{
				BreakNow();
				continue;
			}
			Last = F ? Elapsed : Last;
			Live->Put(Target, Path(*Live, Last));
			double Dt = SRDt;
			if (F && !bBroken && BreakAt > Elapsed && BreakAt < Elapsed + SRDt)
			{
				Dt = BreakAt - Elapsed; // land exactly on the break instant (and the baseline does the same)
			}
			Live->Advance(Dt);
			After += F ? 0 : 1;
		}
		SRTally(Result, *Live);
		const FGLActorPlacement* M = Live->Model(Target);
		const AGLCreature* C = Live->Creature(Target);
		Result.Outcome = M ? M->Creature.Outcome : EGLCreatureOutcome::None;
		Result.How = M ? M->Creature.NeutralizedHow : NAME_None;
		Result.HeldAt = M ? M->Creature.HeldAt : FVector::ZeroVector;
		Result.FinalPathAt = Path(*Live, Last);
		Result.Health = M ? M->Creature.Health : -2.0;
		Result.ZennyHealth = Live->Zenny->GetHealth()->GetCurrent();
		Result.Residue = Live->Residue();
		T.TestEqual(TEXT("no load problems"), Result.Problems, 0);
		T.TestEqual(TEXT("nothing left in flight"), Live->Structures->ActiveCollapses(), 0);
		if (C && Result.Outcome == EGLCreatureOutcome::Neutralized)
		{
			T.TestFalse(TEXT("a neutralized creature is presented inert"), C->IsActorTickEnabled() || C->IsHidden());
		}
		IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SRSlot));
		return Result;
	}

	// The paths. The east deck's impact volume is its 2 x 2 m footprint plus 0.3 m, from the ground up.
	FVector SRUnder(const FSRScene& S, double) { return S.EastDeck(0, 0); }
	FVector SROutside(const FSRScene& S, double) { return S.EastDeck(400, 0); }
	FVector SREscapes(const FSRScene& S, double T) { return T < 0.5 ? S.EastDeck(0, 0) : S.EastDeck(400, 0); }
	FVector SREntersLate(const FSRScene& S, double T) { return T < 0.6 ? S.EastDeck(400, 0) : S.EastDeck(0, 0); }

	/** Runs Path straight through, then broken at every requested moment, and checks they all agree. Returns the baseline. */
	FSRResult SRSame(FAutomationTestBase& T, const TCHAR* Label, int32 Target, const FSRPath& Path, TArray<double> BreakAts, TArray<ESRBreak> Breaks, double PreWound = 0.0)
	{
		FSRResult Baseline;
		for (const double BreakAt : BreakAts)
		{
			const FSRResult Straight = SRRun(T, FString::Printf(TEXT("SR%sBase"), Label), Target, Path, BreakAt, ESRBreak::None, PreWound);
			Baseline = Straight;
			for (const ESRBreak Break : Breaks)
			{
				const FSRResult Broken = SRRun(T, FString::Printf(TEXT("SR%sBroken"), Label), Target, Path, BreakAt, Break, PreWound);
				T.TestTrue(*FString::Printf(TEXT("%s: %s at %.3f s gives the same outcome as no break\n    straight: %s\n    broken:   %s"), Label,
					Break == ESRBreak::SaveRestart ? TEXT("save/restart") : TEXT("unload"), BreakAt, *Straight.Describe(), *Broken.Describe()),
					Broken.bBroke && Broken.SameAs(Straight));
			}
		}
		return Baseline;
	}

	/** The east deck's plan as decided (read from a fall, once): its impact time and its damage. */
	FGLCollapseOutcome SRPlan(FAutomationTestBase& T)
	{
		FSRScene S(TEXT("SRPlanWorld"));
		S.Salvage(TEXT("post_north"));
		S.Salvage(TEXT("post_south"));
		const FGLActiveCollapse* F = S.Falling();
		T.TestNotNull(TEXT("the east deck falls"), F);
		return F ? F->Outcome : FGLCollapseOutcome();
	}

	double SRImpactSeconds(FAutomationTestBase& T) { return SRPlan(T).ImpactSeconds; }

	/** A wound that leaves the 60 hp gremlin with less than the impact's damage (an earlier fight). */
	constexpr double SRGremlinWound = 20.0;
}

using GLStructuralTests::FSRScene;
using GLStructuralTests::ESRBreak;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRCarportStands, "Gridlands.Game.Structural.TheOrdinaryCarportStandsFailsAndFallsByItsOwnRules", GLTestUtils::Flags)
bool FGLSRCarportStands::RunTest(const FString& Parameters)
{
	FSRScene S(TEXT("SRCarportWorld"));
	const FGLStructureRuntime* R = S.Structures->Find(GLStructuralTests::SRId(GLStructuralTests::SRCarport));
	if (!TestNotNull(TEXT("the carport is in the proof room"), R))
	{
		return false;
	}
	TestEqual(TEXT("it is the unchanged P6 structure"), R->Def, FName(TEXT("structure.modern.carport")));
	TArray<FGLPlacedPiece> Pieces;
	for (const FGLStructurePartRuntime& P : R->Parts)
	{
		Pieces.Add(P.Piece);
	}
	TestEqual(TEXT("it stands where it is placed"), GLCollapseRules::Unsupported(GLContent::Get(), Pieces, [&S](const FVector2D& At) { return S.Terrain->HeightAt(At); }).Num(), 0);
	TestTrue(TEXT("the first post comes away"), S.Salvage(TEXT("post_north")));
	TestEqual(TEXT("redundancy: nothing falls"), S.Structures->ActiveCollapses(), 0);
	TestTrue(TEXT("the last post comes away"), S.Salvage(TEXT("post_south")));
	TestEqual(TEXT("support fails: both decks fall"), S.Structures->ActiveCollapses(), 2);
	TestEqual(TEXT("one collapse event, at failure"), S.Count(TEXT("Event.Structure.Collapsed")), 1);
	const GLStructuralTests::FSRScene& C = S;
	const FGLActiveCollapse* F = C.Falling();
	if (TestNotNull(TEXT("the east deck is in flight"), F))
	{
		TestTrue(TEXT("its impact pins at the provisional threshold (2.5 m of timber >= 2.0)"), GLCollapseRules::Pins(F->Outcome, GLContent::Tuning().Collapse));
		TestTrue(TEXT("a delay, then about a second's fall"), F->Outcome.StartSeconds > 0.0 && F->Outcome.ImpactSeconds > 0.8 && F->Outcome.ImpactSeconds < 1.3);
		TestEqual(TEXT("caused and credited: Zenny"), F->Cause, FName(TEXT("Zenny")));
	}
	for (const FGLActiveCollapse& Any : S.Structures->GetActive())
	{
		TestFalse(TEXT("Zenny is outside every impact volume"), Any.Outcome.Impact.TouchesCapsule(S.Zenny->GetActorLocation(), 34, 90));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRPinned, "Gridlands.Game.Structural.ASusceptibleTargetUnderTheImpactIsPinnedNotDamaged", GLTestUtils::Flags)
bool FGLSRPinned::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	const FSRResult R = SRRun(*this, TEXT("SRPinnedWorld"), SRWarden, &SRUnder, 99.0, ESRBreak::None);
	AddInfo(R.Describe());
	TestEqual(TEXT("the warden is NEUTRALIZED"), R.Outcome, EGLCreatureOutcome::Neutralized);
	TestEqual(TEXT("  Pinned"), R.How, FName(TEXT("Neutralize.Pinned")));
	TestEqual(TEXT("  no damage at all"), R.TargetDamage, 0.0);
	TestEqual(TEXT("  health untouched (full)"), R.Health, -1.0);
	TestEqual(TEXT("  no death event"), R.Defeated, 0);
	TestEqual(TEXT("  one neutralization"), R.Neutralized, 1);
	TestEqual(TEXT("  the encounter resolves once"), R.Resolved, 1);
	TestEqual(TEXT("  its reward, once (no kill drops)"), R.Residue, 5);
	TestEqual(TEXT("Zenny, clear of it, is untouched"), R.ZennyHealth, 100.0);
	TestEqual(TEXT("one collapse, two impacts (two decks), two impact noises"), R.Collapsed * 100 + R.Impacts * 10 + R.CollapseNoise, 122);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRControl, "Gridlands.Game.Structural.ANonSusceptibleTargetTakesOrdinaryDamageAndCanBeDefeated", GLTestUtils::Flags)
bool FGLSRControl::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	// The same impact on a creature that lists no Pinned susceptibility: the ordinary environmental damage path.
	const double Damage = SRPlan(*this).Damage; // the P6 rule (10 + 20 per metre fallen), from the plan as decided
	TestTrue(TEXT("the plan does real damage"), Damage > 40.0 && Damage < 60.0);
	const FSRResult Full = SRRun(*this, TEXT("SRControlFullWorld"), SRGremlin, &SRUnder, 99.0, ESRBreak::None);
	AddInfo(Full.Describe());
	TestEqual(TEXT("at full health: not neutralized, wounded by the structure's damage"), Full.TargetDamage, Damage);
	TestEqual(TEXT("  it survives with what is left"), Full.Health, 60.0 - Damage);
	TestEqual(TEXT("  no outcome (still in the fight)"), Full.Outcome, EGLCreatureOutcome::None);
	TestEqual(TEXT("  no neutralization"), Full.Neutralized, 0);
	const FSRResult R = SRRun(*this, TEXT("SRControlWorld"), SRGremlin, &SRUnder, 99.0, ESRBreak::None, SRGremlinWound);
	AddInfo(R.Describe());
	TestEqual(TEXT("already wounded: DEFEATED through its health"), R.Outcome, EGLCreatureOutcome::Defeated);
	TestEqual(TEXT("  by the structure's real damage (after the earlier wound)"), R.TargetDamage, SRGremlinWound + (60.0 - SRGremlinWound));
	TestEqual(TEXT("  one death event"), R.Defeated, 1);
	TestEqual(TEXT("  no neutralization"), R.Neutralized, 0);
	TestEqual(TEXT("  its drops credited to Zenny, who brought the structure down"), R.Residue, 2);
	TestEqual(TEXT("  (it is no encounter target: nothing resolves)"), R.Resolved, 0);
	// Both at once: one impact, one outcome per creature.
	FSRScene S(TEXT("SRBothWorld"));
	S.Put(SRWarden, S.EastDeck(10, -40));
	S.Put(SRGremlin, S.EastDeck(50, 40));
	S.Creature(SRGremlin)->GetHealth()->ApplyDamage(SRGremlinWound, S.Zenny);
	S.Salvage(TEXT("post_north"));
	S.Salvage(TEXT("post_south"));
	for (int32 I = 0; I < 120; ++I)
	{
		S.Advance(SRDt);
	}
	TestTrue(TEXT("the same impact pins the susceptible warden"), S.Placements->IsCreatureNeutralized(SRId(SRWarden)));
	TestTrue(TEXT("and defeats the gremlin through its health"), S.Placements->IsCreatureDefeated(SRId(SRGremlin)));
	const FGLImpactRecord* East = S.Structures->GetImpacts().FindByPredicate([](const FGLImpactRecord& I) { return I.Part == SRDeckEast; });
	for (const FGLImpactRecord& I : S.Structures->GetImpacts())
	{
		AddInfo(FString::Printf(TEXT("impact %s: hit %d, pinned [%s], damaged [%s]"), *I.Part.ToString(), I.Hit.Num(),
			*FString::JoinBy(I.Pinned, TEXT(","), [](FName N) { return N.ToString(); }), *FString::JoinBy(I.Damaged, TEXT(","), [](FName N) { return N.ToString(); })));
	}
	TestTrue(TEXT("its record: the warden pinned, the gremlin damaged"), East && East->Pinned == TArray<FName>{ SRId(SRWarden) } && East->Damaged == TArray<FName>{ SRId(SRGremlin) });
	int32 WardenOutcomes = 0, GremlinOutcomes = 0;
	for (const FGLImpactRecord& I : S.Structures->GetImpacts())
	{
		WardenOutcomes += I.Pinned.Contains(SRId(SRWarden)) + I.Damaged.Contains(SRId(SRWarden));
		GremlinOutcomes += I.Pinned.Contains(SRId(SRGremlin)) + I.Damaged.Contains(SRId(SRGremlin));
	}
	TestEqual(TEXT("one outcome for the warden over every impact"), WardenOutcomes, 1);
	TestEqual(TEXT("one outcome for the gremlin over every impact"), GremlinOutcomes, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRMovement, "Gridlands.Game.Structural.TheImpactDecidesFromWhereTargetsAreAtImpact", GLTestUtils::Flags)
bool FGLSRMovement::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	const FSRResult Escaped = SRRun(*this, TEXT("SREscapeWorld"), SRWarden, &SREscapes, 99.0, ESRBreak::None);
	TestEqual(TEXT("under it at failure, out before impact: NOT affected"), Escaped.Outcome, EGLCreatureOutcome::None);
	TestEqual(TEXT("  no damage either"), Escaped.TargetDamage, 0.0);
	const FSRResult Late = SRRun(*this, TEXT("SRLateWorld"), SRWarden, &SREntersLate, 99.0, ESRBreak::None);
	TestEqual(TEXT("outside at failure, in before impact: affected (pinned)"), Late.Outcome, EGLCreatureOutcome::Neutralized);
	TestTrue(TEXT("  held where it was at the impact (not where it was at failure)"), FVector::Dist2D(Late.HeldAt, Late.FinalPathAt) < 0.01);
	const FSRResult Outside = SRRun(*this, TEXT("SROutsideWorld"), SRWarden, &SROutside, 99.0, ESRBreak::None);
	TestEqual(TEXT("outside at impact: NOT affected"), Outside.Outcome, EGLCreatureOutcome::None);
	TestEqual(TEXT("  and the encounter is still open"), Outside.Resolved, 0);
	// The control the other way: late entry by a non-susceptible creature is damage, not pinning.
	const FSRResult LateGremlin = SRRun(*this, TEXT("SRLateGremlinWorld"), SRGremlin, &SREntersLate, 99.0, ESRBreak::None, SRGremlinWound);
	TestEqual(TEXT("a late-entering (wounded) gremlin is defeated, not pinned"), LateGremlin.Outcome, EGLCreatureOutcome::Defeated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRSaves, "Gridlands.Game.Structural.SaveRestartAtAnyMomentNeverChangesTheOutcome", GLTestUtils::Flags)
bool FGLSRSaves::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	const double Impact = SRImpactSeconds(*this);
	// 1 before support removal, 2 right after failure, 3 mid-fall, 4 one millisecond before impact, 5 after impact.
	const TArray<double> Moments = { -1.0, 0.0, 0.5, Impact - 0.001, Impact + 0.25 };
	const FSRResult Pinned = SRSame(*this, TEXT("Pinned"), SRWarden, &SRUnder, Moments, { ESRBreak::SaveRestart });
	TestEqual(TEXT("(the pinned timeline pins)"), Pinned.How, FName(TEXT("Neutralize.Pinned")));
	SRSame(*this, TEXT("Escape"), SRWarden, &SREscapes, { 0.0, 0.3, 0.7, Impact - 0.001 }, { ESRBreak::SaveRestart });
	SRSame(*this, TEXT("Late"), SRWarden, &SREntersLate, { 0.0, 0.3, 0.7, Impact - 0.001 }, { ESRBreak::SaveRestart });
	SRSame(*this, TEXT("Outside"), SRWarden, &SROutside, { 0.5, Impact - 0.001 }, { ESRBreak::SaveRestart });
	const FSRResult Control = SRSame(*this, TEXT("Control"), SRGremlin, &SRUnder, { -1.0, 0.5, Impact - 0.001, Impact + 0.25 }, { ESRBreak::SaveRestart }, SRGremlinWound);
	SRSame(*this, TEXT("ControlWounds"), SRGremlin, &SRUnder, { 0.5, Impact - 0.001 }, { ESRBreak::SaveRestart });
	TestEqual(TEXT("(the control timeline defeats)"), Control.Outcome, EGLCreatureOutcome::Defeated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRStreams, "Gridlands.Game.Structural.UnloadingMidFallFreezesTheFallAndNeverChangesTheOutcome", GLTestUtils::Flags)
bool FGLSRStreams::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	const double Impact = SRImpactSeconds(*this);
	SRSame(*this, TEXT("UnloadPinned"), SRWarden, &SRUnder, { 0.0, 0.5, Impact - 0.001 }, { ESRBreak::Unload });
	SRSame(*this, TEXT("UnloadEscape"), SRWarden, &SREscapes, { 0.3, 0.7 }, { ESRBreak::Unload });
	SRSame(*this, TEXT("UnloadLate"), SRWarden, &SREntersLate, { 0.3, 0.7 }, { ESRBreak::Unload });
	SRSame(*this, TEXT("UnloadControl"), SRGremlin, &SRUnder, { 0.5 }, { ESRBreak::Unload }, SRGremlinWound);

	// The frozen fall itself: while the lots are dormant nothing advances; back, it resumes where it stopped.
	FSRScene S(TEXT("SRFrozenWorld"));
	S.Put(SRWarden, S.EastDeck(400, 0));
	S.Salvage(TEXT("post_north"));
	S.Salvage(TEXT("post_south"));
	for (int32 I = 0; I < 30; ++I)
	{
		S.Advance(SRDt);
	}
	if (!TestNotNull(TEXT("half a second in, the east deck is still in flight"), S.Falling()) || !TestNotNull(TEXT("  and presented"), S.Structures->FindPart(SRId(SRCarport), SRDeckEast)))
	{
		return false;
	}
	const double Before = S.Falling()->Elapsed;
	const FTransform PoseBefore = S.Structures->FindPart(SRId(SRCarport), SRDeckEast)->GetActorTransform();
	S.GoTo(SRFarAway);
	const FGLSavedCell* Away = S.Test.World->GetSubsystem<UGLSaveSubsystem>()->PeekDormant(SRLots);
	TestTrue(TEXT("the two falls wait in the lots' record"), Away && Away->Collapses.Num() == 2);
	for (int32 I = 0; I < 600; ++I)
	{
		S.Advance(SRDt); // 10 s away
	}
	TestTrue(TEXT("  frozen there (elapsed unchanged)"), Away && Away->Collapses.Num() == 2 && Away->Collapses.ContainsByPredicate([Before](const FGLSavedCollapse& C) { return C.Part == SRDeckEast && C.ElapsedSeconds == Before; }));
	S.GoTo(S.Salvager());
	TestNotNull(TEXT("back: still in flight"), S.Falling());
	TestEqual(TEXT("  from exactly where it stopped"), S.Falling() ? S.Falling()->Elapsed : -1.0, Before);
	AGLStructurePart* East = S.Structures->FindPart(SRId(SRCarport), SRDeckEast);
	TestTrue(TEXT("  presented again"), East != nullptr);
	TestTrue(*FString::Printf(TEXT("  at the same pose (before %s, after %s)"), *PoseBefore.ToString(), East ? *East->GetActorTransform().ToString() : TEXT("-")), East && East->GetActorTransform().Equals(PoseBefore, 0.001));
	TestFalse(TEXT("  not solid while it falls"), East && SRSolid(East));
	TestEqual(TEXT("  no collapse event replayed"), S.Count(TEXT("Event.Structure.Collapsed")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRHold, "Gridlands.Game.Structural.AFallWaitsUntilTheCellsCreaturesCanMove", GLTestUtils::Flags)
bool FGLSRHold::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	FSRScene S(TEXT("SRHoldWorld"));
	S.Put(SRWarden, S.EastDeck(0, 0));
	S.Salvage(TEXT("post_north"));
	S.Salvage(TEXT("post_south"));
	for (int32 I = 0; I < 30; ++I)
	{
		S.Advance(SRDt);
	}
	if (!TestNotNull(TEXT("half a second in, the east deck is still in flight"), S.Falling()))
	{
		return false;
	}
	const double Elapsed = S.Falling()->Elapsed;
	TestTrue(TEXT("saves mid-fall"), S.Save());
	FSRScene R(TEXT("SRHoldRestartWorld"), true);
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Load(Problems));
	// Back with creature presentation held: the models are authoritative, the actors are not there yet.
	R.Grid->PresentationBudgetMs = -1.f;
	R.Grid->PresentationNearM = 0.f;
	R.Zenny->SetActorLocation(R.Salvager());
	for (int32 F = 0; F < 20000 && !R.Grid->IsRuntimeReady(SRLots); ++F)
	{
		R.Grid->Advance(R.Salvager());
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("the lots' authoritative layer is in"), R.Grid->IsRuntimeReady(SRLots));
	TestNull(TEXT("the warden is not presented yet"), R.Creature(SRWarden));
	TestTrue(TEXT("its creatures wait for presentation"), R.Placements->HasPendingCreatures(SRLots));
	for (int32 I = 0; I < 180; ++I)
	{
		R.Advance(SRDt); // 3 s: more than the rest of the fall
	}
	TestNotNull(TEXT("the fall waited: still in flight"), R.Falling());
	TestEqual(TEXT("  not a moment older"), R.Falling() ? R.Falling()->Elapsed : -1.0, Elapsed);
	TestEqual(TEXT("  no impact yet"), R.Count(TEXT("Event.Structure.Impact")), 0);
	R.Grid->PresentationBudgetMs = 0.f;
	R.Grid->FlushAll();
	TestFalse(TEXT("every creature can move now"), R.Placements->HasPendingCreatures(SRLots));
	R.Put(SRWarden, R.EastDeck(400, 0)); // and the warden uses its chance: it steps out before the impact
	for (int32 I = 0; I < 120; ++I)
	{
		R.Advance(SRDt);
	}
	TestEqual(TEXT("then it resumed and hit, once per deck"), R.Count(TEXT("Event.Structure.Impact")), 2);
	TestFalse(TEXT("the warden had its chance to move, and escaped"), R.Placements->IsEncounterResolved(SRId(SRWarden)));
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SRSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRFinal, "Gridlands.Game.Structural.APinnedCreatureStaysPinnedImmuneAndResolvedOnce", GLTestUtils::Flags)
bool FGLSRFinal::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	FSRScene S(TEXT("SRFinalWorld"));
	const FVector Feet = S.EastDeck(0, 0);
	S.Put(SRWarden, Feet);
	S.Salvage(TEXT("post_north"));
	S.Salvage(TEXT("post_south"));
	for (int32 I = 0; I < 120; ++I)
	{
		S.Advance(SRDt);
	}
	TestTrue(TEXT("pinned"), S.Placements->IsCreatureNeutralized(SRId(SRWarden)));
	// A defeated gremlin lies beside it (the direct route's kind of outcome).
	S.Put(SRGremlin, S.EastDeck(60, 50));
	S.Placements->DamageCreature(SRId(SRGremlin), 60.0, S.Zenny);
	TestTrue(TEXT("a defeated gremlin beside it"), S.Placements->IsCreatureDefeated(SRId(SRGremlin)));
	// A second, ordinary structure collapses on both (P9 finality: a neutralized creature takes no further damage).
	const FName Second(TEXT("placement.diner_lots.test_carport_b"));
	const FVector2D Origin(Feet.X - 200.0, Feet.Y); // its east deck over the pinned warden
	TestTrue(TEXT("a second carport stands over it"), S.Structures->SpawnStructure(Second, TEXT("structure.modern.carport"), SRLots, FVector(Origin, 0.0), 0));
	for (const TCHAR* Post : { TEXT("post_north"), TEXT("post_south") })
	{
		AGLStructurePart* Actor = S.Structures->FindPart(Second, Post);
		for (int32 Hit = 0; Actor && Hit < 50 && !Actor->GetSalvageable()->IsSalvaged(); ++Hit)
		{
			Actor->GetSalvageable()->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
		}
	}
	for (int32 I = 0; I < 120; ++I)
	{
		S.Advance(SRDt);
	}
	const FGLImpactRecord* Hit = S.Structures->GetImpacts().FindByPredicate([Second](const FGLImpactRecord& I) { return I.Placement == Second && I.Part == SRDeckEast; });
	TestTrue(TEXT("the second impact landed"), Hit != nullptr);
	TestTrue(TEXT("  and neither pinned nor damaged anything"), Hit && Hit->Pinned.Num() == 0 && Hit->Damaged.Num() == 0);
	TestTrue(TEXT("  the neutralized and the defeated are not even considered (skipped, not merely immune)"), Hit
		&& !Hit->Hit.ContainsByPredicate([&S](const TWeakObjectPtr<AActor>& A) { return A.Get() && (A.Get() == S.Creature(SRWarden) || A.Get() == S.Creature(SRGremlin)); }));
	TestEqual(TEXT("still full health"), S.Model(SRWarden)->Creature.Health, -1.0);
	TestEqual(TEXT("still Pinned"), S.Model(SRWarden)->Creature.NeutralizedHow, FName(TEXT("Neutralize.Pinned")));
	TestEqual(TEXT("one neutralization, one resolution"), S.Count(TEXT("Event.Creature.Neutralized")) * 10 + S.Count(TEXT("Event.Encounter.Resolved")), 11);
	TestEqual(TEXT("one death (the gremlin's, before the second collapse), none after"), S.Count(TEXT("Event.Creature.Defeated")), 1);
	// Restart: still pinned, inert, nothing replays.
	TestTrue(TEXT("saves"), S.Save());
	FSRScene R(TEXT("SRFinalRestartWorld"), true);
	TArray<FString> Problems;
	// The second carport was this test's own, not a placement: the restarted world rightly reports its four saved parts.
	AddExpectedMessagePlain(TEXT("saved structure part placement.diner_lots.test_carport_b/"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 4);
	TestTrue(TEXT("restart loads"), R.Load(Problems));
	R.GoTo(R.Salvager());
	TestEqual(TEXT("no other load problem (the 4 carport warnings come when the lots stream in, counted exactly above)"), Problems.Num(), 0);
	TestTrue(TEXT("after restart: Neutralized, Pinned"), R.Placements->IsCreatureNeutralized(SRId(SRWarden)) && R.Model(SRWarden)->Creature.NeutralizedHow == FName(TEXT("Neutralize.Pinned")));
	const AGLCreature* W = R.Creature(SRWarden);
	TestTrue(TEXT("  presented inert where it was pinned"), W && !W->IsActorTickEnabled() && FVector::Dist2D(W->GetActorLocation(), Feet) < 1.0);
	TestEqual(TEXT("  no event at all replays"), R.Events.Num(), 0);
	TestEqual(TEXT("  the reward is not paid again (5, and the gremlin's 2 drops)"), R.Residue(), 7);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SRSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRModels, "Gridlands.Game.Structural.ImpactsQueryCreatureModelsAndDamageThroughTheOneHealthPath", GLTestUtils::Flags)
bool FGLSRModels::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	FSRScene S(TEXT("SRModelsWorld"));
	S.Put(SRGremlin, S.EastDeck(0, 0));
	S.GoTo(SRFarAway);
	// Back with presentation held: the gremlin is a model only.
	S.Grid->PresentationBudgetMs = -1.f;
	S.Grid->PresentationNearM = 0.f;
	S.Zenny->SetActorLocation(S.Salvager());
	for (int32 F = 0; F < 20000 && !S.Grid->IsRuntimeReady(SRLots); ++F)
	{
		S.Grid->Advance(S.Salvager());
		FPlatformProcess::Sleep(0.001f);
	}
	TestNull(TEXT("the gremlin has no actor"), S.Creature(SRGremlin));
	FGLImpactVolume Volume;
	Volume.Centre = S.EastDeck(0, 0) + FVector(0, 0, 130);
	Volume.HalfExtent = FVector(130, 130, 130);
	TestTrue(TEXT("an unpresented creature is still found under an impact (its model)"), S.Placements->ActiveCreaturesTouching(Volume).Contains(SRId(SRGremlin)));
	FVector Feet;
	S.Placements->CreatureLocation(SRId(SRGremlin), Feet);
	Volume.Centre = Feet + FVector(0, 0, 130 + GLCreatureRules::CapsuleHalfHeightCm * 2); // the box's bottom exactly at the top of its head
	TestTrue(TEXT("  by the shared capsule (touching the top of its head: found)"), S.Placements->ActiveCreaturesTouching(Volume).Contains(SRId(SRGremlin)));
	Volume.Centre.Z += 1.0;
	TestFalse(TEXT("  (1 cm above its head: not found)"), S.Placements->ActiveCreaturesTouching(Volume).Contains(SRId(SRGremlin)));
	const int32 Before = S.Residue();
	TestTrue(TEXT("environmental damage reaches it"), S.Placements->DamageCreature(SRId(SRGremlin), 60.0, S.Zenny));
	TestTrue(TEXT("  defeated through the health system"), S.Placements->IsCreatureDefeated(SRId(SRGremlin)));
	TestEqual(TEXT("  its death event, like any defeat"), S.Count(TEXT("Event.Creature.Defeated")), 1);
	TestEqual(TEXT("  its drops credited to the instigator"), S.Residue(), Before + 2);
	TestFalse(TEXT("a defeated creature is not damaged again"), S.Placements->DamageCreature(SRId(SRGremlin), 60.0, S.Zenny));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSRNoReplan, "Gridlands.Game.Structural.ARestoredFallIsTheDecidedFallNotAReplan", GLTestUtils::Flags)
bool FGLSRNoReplan::RunTest(const FString& Parameters)
{
	using namespace GLStructuralTests;
	const FGLCollapseOutcome Decided = SRPlan(*this);
	FSRScene S(TEXT("SRNoReplanWorld"));
	S.Put(SRWarden, S.EastDeck(300, 0)); // 1.7 m clear of the decided volume, inside a 2 m-margin re-plan's
	S.Salvage(TEXT("post_north"));
	S.Salvage(TEXT("post_south"));
	for (int32 I = 0; I < 20; ++I)
	{
		S.Advance(SRDt);
	}
	TestTrue(TEXT("saves mid-fall"), S.Save());
	// The world the fall resumes in is not the world it was decided in: other physics, other damage.
	FGLCollapseTuningDef& Tuning = const_cast<FGLCollapseTuningDef&>(GLContent::Tuning().Collapse);
	const FGLCollapseTuningDef Kept = Tuning;
	Tuning.Gravity = 2.0;
	Tuning.StartDelaySeconds = 2.0;
	Tuning.DamageBase = 90.0;
	Tuning.ImpactMarginMetres = 2.0;
	{
		FSRScene R(TEXT("SRNoReplanRestartWorld"), true);
		TArray<FString> Problems;
		TestTrue(TEXT("restart loads"), R.Load(Problems));
		R.GoTo(R.Salvager());
		const FGLActiveCollapse* F = R.Falling();
		TestTrue(TEXT("the resumed fall is the decided one (impact time, damage, volume, rest)"), F && F->Outcome.ImpactSeconds == Decided.ImpactSeconds
			&& F->Outcome.Damage == Decided.Damage && F->Outcome.Impact.HalfExtent == Decided.Impact.HalfExtent && F->Outcome.Rest.Equals(Decided.Rest, 0.0));
		for (int32 I = 0; I < 120; ++I)
		{
			R.Advance(SRDt);
		}
		const FGLImpactRecord* East = R.Structures->GetImpacts().FindByPredicate([](const FGLImpactRecord& I) { return I.Part == SRDeckEast; });
		TestTrue(TEXT("it hit with the decided damage, once"), East && East->Damage == Decided.Damage && R.Count(TEXT("Event.Structure.Impact")) == 2);
		AGLStructurePart* Deck = R.Structures->FindPart(SRId(SRCarport), SRDeckEast);
		TestTrue(TEXT("and rests where it was decided"), Deck && Deck->GetActorLocation().Equals(Decided.Rest.GetLocation(), 0.01));
		TestFalse(TEXT("the warden, outside the decided volume, is untouched (a 2 m margin re-plan would have caught it)"), R.Placements->IsEncounterResolved(SRId(SRWarden)));
	}
	Tuning = Kept;
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SRSlot));
	return true;
}

#endif
