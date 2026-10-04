// DEV ONLY (P11, ADR-0039): the WINCHESTER / REAL HOUSE-0 architectural proof (see GLWinchesterHouse.h). Not production
// art and not authored content: built through the player's own transactions so that every rule is the real one.

#include "World/GLWinchesterHouse.h"

#if !UE_BUILD_SHIPPING

#include "Building/GLBuildPiece.h"
#include "Building/GLBuildingSubsystem.h"
#include "Building/GLConstructionRules.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Fabrication/GLFabricatorComponent.h"
#include "GameFramework/Actor.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Misc/Crc.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLWorldSave.h"
#include "Terrain/GLTerrainSubsystem.h"

namespace
{
	const FName WFloor(TEXT("buildpiece.modern.timber_foundation"));
	const FName WStud(TEXT("buildpiece.modern.timber_wall"));
	const FName WDoor(TEXT("buildpiece.modern.timber_doorway"));
	const FName WRoof(TEXT("buildpiece.modern.timber_roof"));
	const FName WLog(TEXT("buildpiece.modern.log_wall"));
	const FName WColumn(TEXT("buildpiece.roman.column"));
	const FName WPost(TEXT("buildpiece.modern.porch_post"));
	const FName WPorch(TEXT("buildpiece.modern.porch_roof"));
	const FName WBayWall(TEXT("buildpiece.modern.bay_wall"));
	const FName WPostL(TEXT("buildpiece.modern.angle_post_l45"));
	const FName WPostR(TEXT("buildpiece.modern.angle_post_r45"));
	const FName WCore(TEXT("buildpiece.modern.base_core"));
	const FName WCrate(TEXT("buildpiece.modern.storage_crate"));
	const FName WSawhorse(TEXT("buildpiece.modern.sawhorse"));
	const FName WClapboard(TEXT("finish.victorian.clapboard"));
	const FName WShingles(TEXT("finish.modern.shingles"));
	const FName WLogItem(TEXT("item.material.timber_log"));
	const FName WStudItem(TEXT("item.component.stud"));
	const FName WScrapItem(TEXT("item.material.scrap_timber"));
	const FName WPlankItem(TEXT("item.material.timber_plank"));
	constexpr int32 Q = GLStructureRules::QuarterTurnSteps;

	struct FWPiece { const TCHAR* Role; FName Def; double X, Y, Z; int32 Yaw; };

	/** Every frame except the bay (which is snapped). Room A x [-300, 300], room B x [300, 700], y [-200, 200]. */
	const TArray<FWPiece>& Frames()
	{
		static const TArray<FWPiece> Plan = {
			{ TEXT("a_f1"), WFloor, -200, -100, 0, 0 }, { TEXT("a_f2"), WFloor, 0, -100, 0, 0 }, { TEXT("a_f3"), WFloor, 200, -100, 0, 0 },
			{ TEXT("a_f4"), WFloor, -200, 100, 0, 0 }, { TEXT("a_f5"), WFloor, 0, 100, 0, 0 }, { TEXT("a_f6"), WFloor, 200, 100, 0, 0 },
			{ TEXT("b_f1"), WFloor, 400, -100, 0, 0 }, { TEXT("b_f2"), WFloor, 600, -100, 0, 0 }, { TEXT("b_f3"), WFloor, 400, 100, 0, 0 }, { TEXT("b_f4"), WFloor, 600, 100, 0, 0 },
			// room A: modern stud frames; the south middle is a doorway into the bay
			{ TEXT("a_n1"), WStud, -200, 200, 30, 0 }, { TEXT("a_n2"), WStud, 0, 200, 30, 0 }, { TEXT("a_n3"), WStud, 200, 200, 30, 0 },
			{ TEXT("a_s1"), WStud, -200, -200, 30, 0 }, { TEXT("a_door"), WDoor, 0, -200, 30, 0 }, { TEXT("a_s3"), WStud, 200, -200, 30, 0 },
			{ TEXT("a_w1"), WStud, -300, -100, 30, Q }, { TEXT("a_w2"), WStud, -300, 100, 30, Q },
			{ TEXT("ab_door"), WDoor, 300, -100, 30, Q }, { TEXT("ab_wall"), WStud, 300, 100, 30, Q },
			// room B: the log-cabin wing
			{ TEXT("b_n1"), WLog, 400, 200, 30, 0 }, { TEXT("b_n2"), WLog, 600, 200, 30, 0 },
			{ TEXT("b_s1"), WLog, 400, -200, 30, 0 }, { TEXT("b_s2"), WLog, 600, -200, 30, 0 },
			{ TEXT("b_e1"), WLog, 700, -100, 30, Q }, { TEXT("b_e2"), WLog, 700, 100, 30, Q },
			// gables: rafters resting on the long walls, meeting at the ridge
			{ TEXT("a_r1"), WRoof, -200, -100, 280, 0 }, { TEXT("a_r2"), WRoof, 0, -100, 280, 0 }, { TEXT("a_r3"), WRoof, 200, -100, 280, 0 },
			{ TEXT("a_r4"), WRoof, -200, 100, 280, 2 * Q }, { TEXT("a_r5"), WRoof, 0, 100, 280, 2 * Q }, { TEXT("a_r6"), WRoof, 200, 100, 280, 2 * Q },
			{ TEXT("b_r1"), WRoof, 400, -100, 280, 0 }, { TEXT("b_r2"), WRoof, 600, -100, 280, 0 },
			{ TEXT("b_r3"), WRoof, 400, 100, 280, 2 * Q }, { TEXT("b_r4"), WRoof, 600, 100, 280, 2 * Q },
			// the porch: a timber roof on a Roman column and a timber post (deliberately mismatched)
			{ TEXT("column"), WColumn, 400, -400, 0, 0 }, { TEXT("porch_post"), WPost, 600, -400, 0, 0 }, { TEXT("porch_roof"), WPorch, 500, -300, 280, 0 },
		};
		return Plan;
	}

	/** Bay: angle posts and short stud walls; only the first post is placed by hand, the rest snap. */
	const FName BaySequence[] = { WPostR, WBayWall, WPostL, WBayWall, WPostL, WBayWall, WPostR };

	const TArray<const TCHAR*>& ClapboardRoles()
	{
		static const TArray<const TCHAR*> Roles = { TEXT("a_n1"), TEXT("a_n2"), TEXT("a_n3"), TEXT("a_s1"), TEXT("a_door"), TEXT("a_s3"), TEXT("a_w1"), TEXT("a_w2"),
			TEXT("ab_door"), TEXT("ab_wall") };
		return Roles;
	}

	const TArray<const TCHAR*>& ShingleRoles()
	{
		static const TArray<const TCHAR*> Roles = { TEXT("a_r1"), TEXT("a_r2"), TEXT("a_r3"), TEXT("a_r4"), TEXT("a_r5"), TEXT("a_r6"),
			TEXT("b_r1"), TEXT("b_r2"), TEXT("b_r3"), TEXT("b_r4"), TEXT("porch_roof") };
		return Roles;
	}

	void AddCost(TMap<FName, int32>& Out, const TArray<FGLItemStackDef>& Cost, int32 Times = 1)
	{
		for (const FGLItemStackDef& Stack : Cost)
		{
			Out.FindOrAdd(Stack.Item) += Stack.Count * Times;
		}
	}

	FString CountsText(const TMap<FName, int32>& Items)
	{
		TArray<FName> Keys;
		Items.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		FString Out;
		for (const FName& Key : Keys)
		{
			Out += FString::Printf(TEXT("%s%s %d"), Out.IsEmpty() ? TEXT("") : TEXT(", "), *Key.ToString().RightChop(Key.ToString().Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1), Items[Key]);
		}
		return Out;
	}

	TMap<FName, int32> Snapshot(const FGLInventory* Inventory)
	{
		TMap<FName, int32> Out;
		if (Inventory)
		{
			for (const FGLInventoryStack& Stack : Inventory->GetStacks())
			{
				Out.FindOrAdd(Stack.Item) += Stack.Count;
			}
		}
		return Out;
	}

	void Stand(AActor* Zenny, const FVector& Where)
	{
		Zenny->SetActorLocation(Where + FVector(0, 0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

TMap<FName, int32> FGLWinchesterHouse::Materials()
{
	const FGLContentRegistry& Content = GLContent::Get();
	TMap<FName, int32> Total;
	for (const FWPiece& Piece : Frames())
	{
		AddCost(Total, Content.Find<FGLBuildPieceDef>(Piece.Def)->Cost);
	}
	for (const FName& Def : BaySequence)
	{
		AddCost(Total, Content.Find<FGLBuildPieceDef>(Def)->Cost);
	}
	AddCost(Total, Content.Find<FGLFinishDef>(WClapboard)->Cost, ClapboardRoles().Num() + 3); // + the three bay walls
	AddCost(Total, Content.Find<FGLFinishDef>(WShingles)->Cost, ShingleRoles().Num());
	return Total;
}

void FGLWinchesterHouse::Note(const FString& Name, bool bPass, const FString& Detail)
{
	Checks.Add({ Name, bPass, Detail });
	UE_LOG(LogGridlands, Log, TEXT("gl.Building.Proof: %s %s%s%s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Name, Detail.IsEmpty() ? TEXT("") : TEXT(": "), *Detail);
}

bool FGLWinchesterHouse::PreparePad(UWorld* World)
{
	UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
	const double Target = Terrain->HeightAt(FVector2D(Anchor));
	Anchor.Z = Target;
	// Ordinary flatten strokes over the footprint of the base and the house (with margin), until the pad is level.
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		for (double X = -1300.0; X <= 1100.0; X += 300.0)
		{
			for (double Y = -800.0; Y <= 600.0; Y += 300.0)
			{
				FGLTerrainEdit Edit;
				Edit.Op = EGLTerrainOp::Flatten;
				Edit.Centre = FVector2D(Anchor.X + X, Anchor.Y + Y);
				Edit.RadiusCm = 400.0;
				Edit.AmountCm = 500.0;
				Edit.TargetHeightCm = Target;
				Terrain->ApplyEdit(Edit);
			}
		}
	}
	double Worst = 0.0;
	for (double X = -1200.0; X <= 1000.0; X += 50.0)
	{
		for (double Y = -700.0; Y <= 500.0; Y += 50.0)
		{
			Worst = FMath::Max(Worst, FMath::Abs(Terrain->HeightAt(FVector2D(Anchor.X + X, Anchor.Y + Y)) - Target));
		}
	}
	Note(TEXT("pad levelled by ordinary flatten strokes"), Worst < 5.0, FString::Printf(TEXT("worst %.2f cm from %.0f cm"), Worst, Target));
	return Worst < 5.0;
}

int32 FGLWinchesterHouse::Place(UWorld* World, AActor* Zenny, FName Def, const FVector& Location, int32 YawStep, FName Role)
{
	UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	FGLPlacedPiece Candidate;
	Candidate.Def = Def;
	Candidate.Location = Location;
	Candidate.YawStep = YawStep;
	const FGLBuildCheck Preview = Building->Check(Zenny, Candidate);
	const TMap<FName, int32> ZennyBefore = Snapshot(&Zenny->FindComponentByClass<UGLInventoryComponent>()->GetInventory());
	const FGLBuildCheck Result = Building->Place(Zenny, Candidate);
	++Placements;
	int32 Id = 0;
	const TArray<FGLPlacedPiece> Pieces = Building->GetPieces();
	for (const FGLPlacedPiece& Piece : Pieces)
	{
		if (Piece.Def == Def && Piece.Location.Equals(Location, 0.1) && Piece.YawStep == GLStructureRules::NormalizeYawStep(YawStep))
		{
			Id = Piece.Id;
		}
	}
	// PREVIEW == REALITY: refused exactly when previewed RED; when built, its committed support gives the previewed colour.
	FGLBuildCheck Committed = Result;
	if (Id)
	{
		const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
		Committed.Support = GLStructureRules::ComputeSupport(GLContent::Get(), Structures->PlayerStructureOf(Id), [World](const FVector2D& P) { return World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(P); }).FindRef(Id);
	}
	Committed.Preview = Id ? GLStructureRules::PreviewOf(Committed) : EGLPreview::Red;
	const bool bAgree = Committed.Preview == Preview.Preview && (Id != 0) == Preview.IsAllowed();
	PreviewMismatches += bAgree ? 0 : 1;
	(Preview.Preview == EGLPreview::Green ? Green : Preview.Preview == EGLPreview::Yellow ? Yellow : Red) += 1;
	const TMap<FName, int32> ZennyAfter = Snapshot(&Zenny->FindComponentByClass<UGLInventoryComponent>()->GetInventory());
	const FGLBuildPieceDef* PieceDef = GLContent::Get().Find<FGLBuildPieceDef>(Def);
	if (Id && PieceDef)
	{
		for (const FGLItemStackDef& Cost : PieceDef->Cost)
		{
			const int32 Personal = ZennyBefore.FindRef(Cost.Item) - ZennyAfter.FindRef(Cost.Item);
			FromZenny.FindOrAdd(Cost.Item) += Personal;
			FromStorage.FindOrAdd(Cost.Item) += Cost.Count - Personal;
		}
	}
	if (!Id)
	{
		Note(FString::Printf(TEXT("place %s"), Role.IsNone() ? *Def.ToString() : *Role.ToString()), false, Result.Reason);
	}
	else if (!Role.IsNone())
	{
		Ids.Add(Role, Id);
	}
	return Id;
}

int32 FGLWinchesterHouse::SnapPlace(UWorld* World, AActor* Zenny, FName Def, const FVector& Aim, FName Role)
{
	FGLPlacedPiece Candidate;
	if (!World->GetSubsystem<UGLBuildingSubsystem>()->Snap(Def, Aim, 0, Candidate))
	{
		Note(FString::Printf(TEXT("snap %s"), *Def.ToString()), false, TEXT("no socket in reach"));
		return 0;
	}
	return Place(World, Zenny, Def, Candidate.Location, Candidate.YawStep, Role);
}

bool FGLWinchesterHouse::EstablishBase(UWorld* World, AActor* Zenny)
{
	World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(TEXT("knowledge.style.modern_timber_frame"));
	World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(TEXT("knowledge.style.roman_masonry"));
	UGLInventoryComponent* Inventory = Zenny->FindComponentByClass<UGLInventoryComponent>();
	Inventory->AddItem(WPlankItem, 2 + 2 * 2 + 4 + 4 + 3); // what the base itself costs, carried
	Stand(Zenny, At(-700, 0));
	const int32 Core = Place(World, Zenny, WCore, At(-900, 0), 0, TEXT("core"));
	// Crates stand on floors (so a crate can lose its support like anything else).
	Place(World, Zenny, WFloor, At(-900, 250), 0, TEXT("crate_floor_a"));
	Place(World, Zenny, WFloor, At(-900, -250), 0, TEXT("crate_floor_b"));
	const int32 CrateA = Place(World, Zenny, WCrate, At(-900, 250, 30), 0, TEXT("crate_a"));
	const int32 CrateB = Place(World, Zenny, WCrate, At(-900, -250, 30), 0, TEXT("crate_b"));
	const int32 Saw = Place(World, Zenny, WSawhorse, At(-900, -650), 0, TEXT("sawhorse"));
	UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	const TArray<FGLClaim> Claims = Building->Claims();
	Note(TEXT("a base core establishes one claim"), Core && Claims.Num() == 1, FString::Printf(TEXT("%d claim(s), radius %.0f m"), Claims.Num(), Claims.Num() ? Claims[0].Areas[0].RadiusCm / 100.0 : 0.0));
	// The house's materials go into the crates (shared base storage); Zenny keeps a small share of the studs only.
	TMap<FName, int32> Need = Materials();
	Need.FindOrAdd(WLogItem) += 2; // logs for the sawhorse proof
	const int32 ZennyStuds = 12;
	Inventory->AddItem(WStudItem, ZennyStuds);
	Need.FindOrAdd(WStudItem) -= ZennyStuds;
	FGLInventory* A = Building->StorageOf(CrateA);
	FGLInventory* B = Building->StorageOf(CrateB);
	bool bStocked = A && B;
	TArray<FName> Items;
	Need.GetKeys(Items);
	Items.Sort(FNameLexicalLess());
	for (const FName& Item : Items)
	{
		int32 Left = Need[Item];
		Left -= A ? A->Add(GLContent::Get(), Item, Left) : 0;
		Left -= B ? B->Add(GLContent::Get(), Item, Left) : 0;
		bStocked &= Left == 0;
	}
	Note(TEXT("materials placed primarily in shared storage"), bStocked && Saw, FString::Printf(TEXT("crates hold %s; Zenny carries %d studs"), *CountsText(Need), ZennyStuds));
	return Core && CrateA && CrateB && Saw && bStocked;
}

bool FGLWinchesterHouse::Frame(UWorld* World, AActor* Zenny)
{
	const int32 Before = Placements - PreviewMismatches;
	Stand(Zenny, At(-500, 0));
	for (const FWPiece& Piece : Frames())
	{
		Place(World, Zenny, Piece.Def, At(Piece.X, Piece.Y, Piece.Z), Piece.Yaw, Piece.Role);
	}
	// The bay: the first angle post by hand, just outside the south doorway; every later piece snaps (yaw from data).
	BayIds.Reset();
	const int32 First = Place(World, Zenny, BaySequence[0], At(-146, -215), 0, TEXT("bay_p1"));
	BayIds.Add(First);
	const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
	for (int32 I = 1; I < UE_ARRAY_COUNT(BaySequence) && BayIds.Last(); ++I)
	{
		const FGLStructurePartRuntime* Prev = Structures->FindPlayerPiece(BayIds.Last());
		const FGLBuildPieceDef* PrevDef = Prev ? GLContent::Get().Find<FGLBuildPieceDef>(Prev->Piece.Def) : nullptr;
		const TCHAR* SocketName = BaySequence[I] == WBayWall ? TEXT("out") : TEXT("right");
		FVector Aim = FVector::ZeroVector;
		for (int32 S = 0; PrevDef && S < PrevDef->Sockets.Num(); ++S)
		{
			if (PrevDef->Sockets[S].Name == SocketName)
			{
				Aim = GLStructureRules::Sockets(*PrevDef, Prev->Piece)[S].Location;
			}
		}
		BayIds.Add(SnapPlace(World, Zenny, BaySequence[I], Aim, FName(*FString::Printf(TEXT("bay_%d"), I + 1))));
	}
	TArray<int32> BayYaws;
	for (const int32 Id : BayIds)
	{
		const FGLStructurePartRuntime* Part = Structures->FindPlayerPiece(Id);
		BayYaws.Add(Part ? Part->Piece.YawStep : -1);
	}
	const bool bBay = BayIds.Num() == UE_ARRAY_COUNT(BaySequence) && !BayIds.Contains(0);
	// Walls: -45, 0, +45 degrees, by the posts' socket data alone.
	const bool bAngles = bBay && BayYaws[1] == GLStructureRules::YawStepFromDegrees(-45.0) && BayYaws[3] == 0 && BayYaws[5] == GLStructureRules::YawStepFromDegrees(45.0);
	Note(TEXT("45 degree bay snapped from socket data"), bBay && bAngles, FString::Printf(TEXT("yaw steps %s"), *FString::JoinBy(BayYaws, TEXT(" "), [](int32 Y) { return FString::FromInt(Y); })));
	int32 Framed = 0, Complete = 0;
	UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	for (const FGLPlacedPiece& Piece : Building->GetPieces())
	{
		const FName Shown = Building->ShownPhaseOf(Piece.Id); // as presented (instanced or by its actor)
		Framed += Shown == FName(TEXT("frame")) ? 1 : 0;
		Complete += Shown == FName(TEXT("complete")) ? 1 : 0;
	}
	Note(TEXT("FRAME first: stud walls, doorways and rafters visibly framed"), Framed == 10 + 3 + 11, FString::Printf(TEXT("%d showing framing, %d complete as built"), Framed, Complete));
	Note(TEXT("every frame placed, PREVIEW == REALITY"), PreviewMismatches == 0 && Placements - PreviewMismatches - Before == Frames().Num() + UE_ARRAY_COUNT(BaySequence),
		FString::Printf(TEXT("%d placements so far, %d mismatches (GREEN %d, YELLOW %d, RED %d)"), Placements, PreviewMismatches, Green, Yellow, Red));
	const int32 PersonalStuds = FromZenny.FindRef(WStudItem), StorageStuds = FromStorage.FindRef(WStudItem);
	Note(TEXT("storage supplied the construction (before Zenny's own)"), StorageStuds > 0 && PersonalStuds * 5 < StorageStuds + PersonalStuds,
		FString::Printf(TEXT("studs from storage %d, from Zenny %d"), StorageStuds, PersonalStuds));
	return bBay && bAngles;
}

bool FGLWinchesterHouse::Finish(UWorld* World, AActor* Zenny)
{
	UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	const TMap<int32, double> SupportBefore = Building->Support();
	int32 Installed = 0, Wanted = 0;
	auto Install = [&](int32 Id, FName Finish)
	{
		++Wanted;
		Installed += Id && Building->InstallFinish(Zenny, Id, Finish).IsAllowed() ? 1 : 0;
	};
	for (const TCHAR* Role : ClapboardRoles())
	{
		Install(Ids.FindRef(Role), WClapboard);
	}
	for (int32 I = 1; I < BayIds.Num(); I += 2)
	{
		Install(BayIds[I], WClapboard);
	}
	for (const TCHAR* Role : ShingleRoles())
	{
		Install(Ids.FindRef(Role), WShingles);
	}
	int32 Framed = 0, Finished = 0;
	for (const FGLPlacedPiece& Piece : Building->GetPieces())
	{
		const FName Shown = Building->ShownPhaseOf(Piece.Id);
		Framed += Shown == FName(TEXT("frame")) ? 1 : 0;
		Finished += Shown == FName(TEXT("finish")) ? 1 : 0;
	}
	Note(TEXT("FINISH applied afterwards (clapboard, shingles)"), Installed == Wanted && Framed == 0, FString::Printf(TEXT("%d/%d installed; %d finished, %d still framed"), Installed, Wanted, Finished, Framed));
	Note(TEXT("finishing changed no support"), Building->Support().OrderIndependentCompareEqual(SupportBefore));
	// Mixed vocabularies, deliberately: modern stud framing, a Victorian finish, a log wing, a Roman column.
	TSet<FName> Eras;
	const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
	for (const FGLPlacedPiece& Piece : Building->GetPieces())
	{
		Eras.Add(GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def)->Era);
		for (const FName& Layer : Piece.Layers)
		{
			Eras.Add(GLContent::Get().Find<FGLFinishDef>(Layer)->Era);
		}
	}
	const bool bLog = Ids.Contains(TEXT("b_n1")), bColumn = Ids.Contains(TEXT("column"));
	Note(TEXT("architectural vocabularies mixed on purpose"), Eras.Contains(TEXT("era.memory.victorian")) && Eras.Contains(TEXT("era.memory.roman")) && Eras.Contains(TEXT("era.memory.modern_day")) && bLog && bColumn,
		FString::Printf(TEXT("eras %s; log wing %s; Roman column %s"), *FString::JoinBy(Eras, TEXT(" "), [](FName E) { return E.ToString(); }), bLog ? TEXT("yes") : TEXT("no"), bColumn ? TEXT("yes") : TEXT("no")));
	return Installed == Wanted && Framed == 0;
}

bool FGLWinchesterHouse::Saw(UWorld* World, AActor* Zenny)
{
	UGLFabricatorComponent* Fabricator = Zenny->FindComponentByClass<UGLFabricatorComponent>();
	UGLInventoryComponent* Inventory = Zenny->FindComponentByClass<UGLInventoryComponent>();
	if (!Fabricator)
	{
		Note(TEXT("raw log -> sawing -> studs"), false, TEXT("no fabricator"));
		return false;
	}
	Stand(Zenny, At(-700, -650));
	const int32 Studs = Inventory->CountOf(WStudItem), Logs = Inventory->CountOf(WLogItem);
	const FGLCraftCheck Result = Fabricator->Fabricate(TEXT("recipe.component.stud"));
	const bool bOk = Result.CanCraft() && Inventory->CountOf(WStudItem) == Studs + 4 && Inventory->CountOf(WLogItem) == Logs;
	Note(TEXT("raw log -> sawing at the sawhorse -> studs (the log from base storage)"), bOk,
		FString::Printf(TEXT("studs %d -> %d, Zenny's logs %d -> %d"), Studs, Inventory->CountOf(WStudItem), Logs, Inventory->CountOf(WLogItem)));
	return bOk;
}

bool FGLWinchesterHouse::PorchCollapse(UWorld* World, AActor* Zenny)
{
	UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	const int32 Post = Ids.FindRef(TEXT("porch_post")), Column = Ids.FindRef(TEXT("column")), Roof = Ids.FindRef(TEXT("porch_roof"));
	Stand(Zenny, At(500, -700));
	const TArray<int32> PostPrediction = Building->PreviewRemoval(Post);
	const FGLDemolishResult PostOut = Building->Dismantle(Zenny, Post);
	Note(TEXT("porch post out: predicted to stand, and it stands"), PostOut.IsDone() && PostPrediction.Num() == 0 && PostOut.Collapsed.Num() == 0 && Building->IsPresented(Roof),
		FString::Printf(TEXT("predicted %d falling, %d fell"), PostPrediction.Num(), PostOut.Collapsed.Num()));
	const TArray<int32> ColumnPrediction = Building->PreviewRemoval(Column);
	const FGLDemolishResult ColumnOut = Building->Dismantle(Zenny, Column);
	const FGLStructurePartRuntime* Fallen = World->GetSubsystem<UGLStructureSubsystem>()->FindPlayerPiece(Roof);
	const bool bPass = ColumnOut.IsDone() && ColumnPrediction == TArray<int32>{ Roof } && ColumnOut.Collapsed == ColumnPrediction && Fallen && Fallen->State == EGLStructurePartState::Debris;
	Note(TEXT("Roman column out: the predicted roof falls, by the canonical collapse"), bPass,
		FString::Printf(TEXT("predicted [%s], collapsed [%s]"), *FString::JoinBy(ColumnPrediction, TEXT(","), [](int32 I) { return FString::FromInt(I); }),
			*FString::JoinBy(ColumnOut.Collapsed, TEXT(","), [](int32 I) { return FString::FromInt(I); })));
	return bPass;
}

bool FGLWinchesterHouse::Salvage(UWorld* World, AActor* Zenny)
{
	UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	Stand(Zenny, At(100, 0, 30));
	const int32 Interior = Ids.FindRef(TEXT("ab_wall"));
	const TArray<int32> Predicted = Building->PreviewRemoval(Interior);
	const FGLDemolishResult Careful = Building->Dismantle(Zenny, Interior);
	const int32 CarefulStuds = Careful.Recovered.FindRef(WStudItem);
	Note(TEXT("careful dismantle of the interior wall: intact studs (and its clapboard back as boards)"), Careful.IsDone() && Predicted.Num() == 0 && CarefulStuds >= 5 && Careful.Recovered.FindRef(WPlankItem) >= 2,
		CountsText(Careful.Recovered));
	const int32 Centre = BayIds.IsValidIndex(3) ? BayIds[3] : 0;
	Stand(Zenny, At(0, -500));
	const FGLDemolishResult Smashed = Building->Smash(Zenny, Centre);
	Note(TEXT("smashing a bay wall: fewer studs, more scrap"), Smashed.IsDone() && Smashed.Recovered.FindRef(WStudItem) < CarefulStuds && Smashed.Recovered.FindRef(WScrapItem) > Careful.Recovered.FindRef(WScrapItem),
		CountsText(Smashed.Recovered));
	return Careful.IsDone() && Smashed.IsDone();
}

TArray<FGLPlacedPiece> FGLWinchesterHouse::DensePlayerBase(const FVector& Origin, int32 Units, TFunctionRef<double(const FVector2D&)> Ground, int32 FirstId, FName Cell)
{
	const FGLContentRegistry& Content = GLContent::Get();
	const FGLBuildPieceDef* FloorDef = Content.Find<FGLBuildPieceDef>(WFloor);
	TArray<FGLPlacedPiece> Out;
	int32 Id = FirstId;
	const int32 Columns = FMath::CeilToInt(FMath::Sqrt(static_cast<double>(Units)));
	auto Add = [&](FName Def, const FVector& At, int32 Yaw, TArray<FName> Layers = {})
	{
		FGLPlacedPiece& P = Out.AddDefaulted_GetRef();
		P.Id = Id++;
		P.Def = Def;
		P.Location = At;
		P.YawStep = GLStructureRules::NormalizeYawStep(Yaw);
		P.Cell = Cell;
		P.Origin = EGLPieceOrigin::Player;
		P.Layers = MoveTemp(Layers);
	};
	for (int32 U = 0; U < Units; ++U)
	{
		const FVector2D C(Origin.X + (U % Columns) * 450.0, Origin.Y + (U / Columns) * 450.0);
		const int32 Yaw = U % 4 == 3 ? 18 : 0; // every 4th unit at 45 degrees
		const bool bFinished = U % 2 == 0;
		FGLPlacedPiece Floor;
		Floor.Def = WFloor;
		Floor.Location = FVector(C, Ground(C));
		Floor.YawStep = Yaw;
		Add(WFloor, Floor.Location, Yaw);
		// Walls on the floor's four edges (the floor's own top sockets), north/south along it, east/west across.
		const struct { double X, Y; int32 Turn; } Edges[] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, Q }, { -1, 0, Q } };
		for (const auto& Edge : Edges)
		{
			const FVector At = GLStructureRules::ToWorld(Floor, { Edge.X, Edge.Y, 0.3 });
			Add(WStud, At, Yaw + Edge.Turn, bFinished ? TArray<FName>{ U % 4 == 0 ? WClapboard : FName(TEXT("finish.modern.timber_board_wall")) } : TArray<FName>{});
		}
		Add(WRoof, Floor.Location + FVector(0, 0, 280.0), Yaw, bFinished ? TArray<FName>{ WShingles } : TArray<FName>{});
		if (U % 6 == 0)
		{
			Add(WCrate, Floor.Location + FVector(0, 0, 30.0), Yaw);
		}
	}
	(void)FloorDef;
	return Out;
}

TArray<FGLSavedPiece> FGLWinchesterHouse::DensePlayerBaseSaved(const FVector& Origin, int32 Units, TFunctionRef<double(const FVector2D&)> Ground, int32 FirstId, FName Cell)
{
	TArray<FGLSavedPiece> Saved;
	for (const FGLPlacedPiece& Piece : DensePlayerBase(Origin, Units, Ground, FirstId, Cell))
	{
		FGLSavedPiece& S = Saved.AddDefaulted_GetRef();
		S.Id = Piece.Id;
		S.Def = Piece.Def;
		S.Location = Piece.Location;
		S.YawStep = Piece.YawStep;
		S.Origin = static_cast<uint8>(EGLPieceOrigin::Player);
		S.Layers = Piece.Layers;
		if (Piece.Def == WCrate)
		{
			S.Contents = { { WPlankItem, 60 }, { WStudItem, 40 } };
		}
	}
	return Saved;
}

FString FGLWinchesterHouse::Fingerprint(UWorld* World) const
{
	const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
	TArray<FGLPlacedPiece> Pieces = Structures->PlayerPieces(NAME_None, true);
	FString Text;
	int32 Debris = 0, Layers = 0, Owned = 0;
	for (const FGLPlacedPiece& Piece : Pieces)
	{
		const FGLStructurePartRuntime* Part = Structures->FindPlayerPiece(Piece.Id);
		const bool bDebris = Part && Part->State == EGLStructurePartState::Debris;
		Debris += bDebris ? 1 : 0;
		Layers += Piece.Layers.Num();
		Owned += Piece.Origin == EGLPieceOrigin::Player ? 1 : 0;
		Text += FString::Printf(TEXT("%d %s %d %.0f %.0f %.0f %s %s|"), Piece.Id, *Piece.Def.ToString(), Piece.YawStep, Piece.Location.X, Piece.Location.Y, Piece.Location.Z,
			*FString::JoinBy(Piece.Layers, TEXT("+"), [](FName L) { return L.ToString(); }), bDebris ? TEXT("debris") : TEXT("intact"));
		if (Part)
		{
			for (const FGLInventoryStack& Stack : Part->Contents.GetStacks())
			{
				Text += FString::Printf(TEXT("%s=%d,"), *Stack.Item.ToString(), Stack.Count);
			}
		}
	}
	return FString::Printf(TEXT("%d pieces (%d debris, %d layers, %d player-owned) crc %08x"), Pieces.Num(), Debris, Layers, Owned, FCrc::StrCrc32(*Text));
}

#endif
