// DEV ONLY: P6 real-game acceptance proofs (structural salvage, collapse, trees, noise). Each command
// runs a timed sequence in the running game (real ticking, navigation, streaming) and logs PASS/FAIL.
// Not in shipping builds.

#include "Character/GLCharacter.h"
#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameplayTagsManager.h"
#include "Engine/World.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "TimerManager.h"
#include "World/GLGridSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace GLStructureDemo
{
	const FName DCarport(TEXT("placement.origin.structure_carport_01"));
	const FName DCarportEdge(TEXT("placement.origin.structure_carport_edge"));
	const FName DPine(TEXT("placement.origin.structure_pine_01"));
	const FName DPineEdge(TEXT("placement.origin.structure_pine_edge"));
	const FName DOrigin(TEXT("cell.home.origin"));
	const FName DGremlin(TEXT("creature.drain.static_gremlin"));

	struct FStep
	{
		double At = 0.0;
		TFunction<void()> Run;
	};

	/** Runs steps at their times (seconds from now) on the world's timer. */
	void WhenReady(UWorld* World, TFunction<void()> Then, int32 Tries = 0);

	void RunSteps(UWorld* World, TArray<FStep> Steps)
	{
		WhenReady(World, [World, Steps = MoveTemp(Steps)]() mutable
		{
			for (FStep& Step : Steps)
			{
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(MoveTemp(Step.Run)), FMath::Max(0.01, Step.At), false);
			}
		});
	}

	/** Streaming is asynchronous (ADR-0028): commands given at startup wait for the origin's structures. */
	void WhenReady(UWorld* World, TFunction<void()> Then, int32 Tries)
	{
		const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
		if ((Structures && Structures->Find(DCarport)) || Tries >= 120)
		{
			Then();
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([World, Then, Tries]() { WhenReady(World, Then, Tries + 1); }), 0.25f, false);
	}

	AGLCharacter* ZennyOf(UWorld* World) { return Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)); }

	void Stand(UWorld* World, const FVector2D& At, double Yaw = 0.0)
	{
		if (AGLCharacter* Zenny = ZennyOf(World))
		{
			Zenny->SetActorLocation(FVector(At, World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(At) + 100.0), false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* C = Zenny->GetController())
			{
				C->SetControlRotation(FRotator(-10.0, Yaw, 0.0));
			}
		}
	}

	/** Zenny salvages a part through the ordinary salvage pipeline (hits until it comes away). */
	bool Salvage(UWorld* World, FName Placement, FName Part)
	{
		AGLStructurePart* Actor = World->GetSubsystem<UGLStructureSubsystem>()->FindPart(Placement, Part);
		AGLCharacter* Zenny = ZennyOf(World);
		for (int32 Hit = 0; Actor && Zenny && Hit < 50 && !Actor->GetSalvageable()->IsSalvaged(); ++Hit)
		{
			Actor->GetSalvageable()->Interact(Zenny, UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Salvage")));
		}
		return Actor && Actor->GetSalvageable()->IsSalvaged();
	}

	const TCHAR* StateName(EGLStructurePartState State)
	{
		switch (State)
		{
		case EGLStructurePartState::Intact: return TEXT("intact");
		case EGLStructurePartState::Removed: return TEXT("removed");
		case EGLStructurePartState::Debris: return TEXT("debris");
		case EGLStructurePartState::DebrisSalvaged: return TEXT("debris-salvaged");
		}
		return TEXT("?");
	}

	FString Describe(UWorld* World, FName Placement)
	{
		const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
		const FGLStructureRuntime* S = Structures->Find(Placement);
		if (!S)
		{
			return FString::Printf(TEXT("%s: not loaded"), *Placement.ToString());
		}
		FString Out = Placement.ToString() + TEXT(":");
		for (const FGLStructurePartRuntime& Part : S->Parts)
		{
			const AGLStructurePart* Actor = Part.Actor.Get();
			Out += FString::Printf(TEXT(" %s=%s"), *Part.Name.ToString(), StateName(Part.State));
			if (Part.State == EGLStructurePartState::Debris)
			{
				Out += FString::Printf(TEXT("@%s%s"), *Part.Rest.GetLocation().ToCompactString(),
					Actor && Actor->GetActorLocation().Equals(Part.Rest.GetLocation(), 1.0) ? TEXT("(actor at rest)") : TEXT("(actor NOT at rest)"));
			}
		}
		return Out;
	}

	double HealthOf(UWorld* World)
	{
		const AGLCharacter* Zenny = ZennyOf(World);
		return Zenny ? Zenny->GetHealth()->GetCurrent() : -1.0;
	}

	void Shot(const TCHAR* Name)
	{
		UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Structure: screenshot %s"), Name);
		GEngine->DeferredCommands.Add(TEXT("HighResShot 1920x1080"));
	}

	/** A: stand under the west deck; B/C/D: take the posts; E: report; kill: start at 50 health. */
	void Collapse(const TArray<FString>& Args, UWorld* World)
	{
		const bool bKill = Args.Contains(TEXT("kill"));
		RunSteps(World, {
			{ 0.5, [World, bKill]()
			{
				Stand(World, FVector2D(-3050, -3700), -90.0); // under the west deck's north edge, looking south at the carport
				if (bKill)
				{
					ZennyOf(World)->GetHealth()->Restore(50.0);
				}
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Collapse: A before: %s; Zenny health %.0f"), *Describe(World, DCarport), HealthOf(World));
				Stand(World, FVector2D(-3050, -3950), -90.0);
				Salvage(World, DCarport, TEXT("post_south"));
			} },
			{ 1.5, [World]()
			{
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Collapse: took post_south (not the last support): %s; active collapses %d"),
					*Describe(World, DCarport), World->GetSubsystem<UGLStructureSubsystem>()->ActiveCollapses());
				Salvage(World, DCarport, TEXT("post_north"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Collapse: B took post_north (the wrong one): decided at once: %s; active collapses %d"),
					*Describe(World, DCarport), World->GetSubsystem<UGLStructureSubsystem>()->ActiveCollapses());
			} },
			{ 2.05, [World]() { Stand(World, FVector2D(-3050, -3500), -90.0); Shot(TEXT("mid-collapse")); Stand(World, FVector2D(-3050, -3950), -90.0); } },
			{ 5.0, [World, bKill]()
			{
				const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
				const TArray<FGLImpactRecord>& Impacts = Structures->GetImpacts();
				FString Hits;
				for (const FGLImpactRecord& R : Impacts)
				{
					Hits += FString::Printf(TEXT(" %s(%.0f dmg, %d hit)"), *R.Part.ToString(), R.Damage, R.Hit.Num());
				}
				const double Health = HealthOf(World);
				const bool bDecksDown = Describe(World, DCarport).Contains(TEXT("deck_west=debris")) && Describe(World, DCarport).Contains(TEXT("deck_east=debris"));
				const bool bHurt = bKill ? Impacts.Num() == 2 && Impacts[0].Hit.Num() > 0 : FMath::IsNearlyEqual(Health, 40.0, 0.5);
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Collapse: C/D impacts:%s; Zenny health now %.0f%s"), *Hits, Health, bKill ? TEXT(" (started at 50: killed, then woke at the start)") : TEXT(""));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Collapse: E after: %s; noises: break %d, collapse %d"), *Describe(World, DCarport),
					World->GetSubsystem<UGLNoiseSubsystem>()->CountOf(TEXT("Noise.Structure.Break")), World->GetSubsystem<UGLNoiseSubsystem>()->CountOf(TEXT("Noise.Structure.Collapse")));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Collapse: %s"), bDecksDown && bHurt ? TEXT("PASS") : TEXT("FAIL"));
				Stand(World, FVector2D(-3050, -3300), -90.0);
				Shot(TEXT("after-collapse"));
			} },
		});
	}

	/** G/H: the same collapse with a creature in its impact area (it arrives under the east deck as it starts to fall). */
	void CollapseCreature(UWorld* World)
	{
		RunSteps(World, {
			{ 0.5, [World]()
			{
				Stand(World, FVector2D(-4600, -4000), 0.0); // 15 m west of the carport: out of any creature's sight here
				Salvage(World, DCarport, TEXT("post_south"));
				Salvage(World, DCarport, TEXT("post_north"));
				UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
				const FVector2D Under(-2750, -4000);
				AGLCreature* Gremlin = World->GetSubsystem<UGLPlacementSubsystem>()->SpawnProofCreature(DGremlin, FVector(Under, Terrain->HeightAt(Under)), 0.0, DOrigin);
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.CollapseCreature: G collapse decided (%d parts falling); a gremlin is under the east deck (health %.0f), facing away; Zenny 15 m clear"),
					World->GetSubsystem<UGLStructureSubsystem>()->ActiveCollapses(), Gremlin ? Gremlin->GetHealth()->GetCurrent() : -1.0);
			} },
			{ 4.0, [World]()
			{
				const AGLCreature* Gremlin = nullptr;
				for (TActorIterator<AGLCreature> It(World); It; ++It)
				{
					if (It->GetPlacementId() == FName(TEXT("placement.proof.creature")))
					{
						Gremlin = *It;
					}
				}
				const FGLImpactRecord* East = World->GetSubsystem<UGLStructureSubsystem>()->GetImpacts().FindByPredicate([](const FGLImpactRecord& R) { return R.Part == FName(TEXT("deck_east")); });
				const bool bHit = East && Gremlin && East->Hit.ContainsByPredicate([Gremlin](const TWeakObjectPtr<AActor>& A) { return A.Get() == Gremlin; });
				const int32 Residue = ZennyOf(World)->GetInventory()->CountOf(TEXT("item.material.static_residue"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.CollapseCreature: H the east deck hit the gremlin: %s (%.0f damage); defeated %s; its drops went to whoever brought it down (Zenny's static residue %d); Zenny health %.0f; %s"),
					bHit ? TEXT("yes") : TEXT("no"), East ? East->Damage : 0.0, Gremlin && Gremlin->IsDefeated() ? TEXT("yes") : TEXT("no"), Residue, HealthOf(World),
					bHit && Gremlin->IsDefeated() && Residue >= 2 ? TEXT("PASS") : TEXT("FAIL"));
			} },
		});
	}

	/** S: chop the pine (the trunk topples away from Zenny), gather the log. */
	void FellTree(UWorld* World)
	{
		RunSteps(World, {
			{ 0.5, [World]()
			{
				Stand(World, FVector2D(-1700, -4500), 0.0);
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.FellTree: before: %s"), *Describe(World, DPine));
				Salvage(World, DPine, TEXT("stump"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.FellTree: chopped (%d chop noises): %s"), World->GetSubsystem<UGLNoiseSubsystem>()->CountOf(TEXT("Noise.Gather.Chop")), *Describe(World, DPine));
			} },
			{ 1.6, [World]() { Stand(World, FVector2D(-2600, -4500), 0.0); Shot(TEXT("tree-falling")); } },
			{ 5.0, [World]()
			{
				const FGLStructureRuntime* Tree = World->GetSubsystem<UGLStructureSubsystem>()->Find(DPine);
				const FGLStructurePartRuntime* Trunk = Tree ? Tree->Parts.FindByPredicate([](const FGLStructurePartRuntime& P) { return P.Name == FName(TEXT("trunk")); }) : nullptr;
				const FVector Up = Trunk ? Trunk->Rest.GetRotation().GetUpVector() : FVector::ZeroVector;
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.FellTree: fell: %s; lies pointing %s"), *Describe(World, DPine), *Up.ToCompactString());
				Shot(TEXT("tree-fallen"));
			} },
			{ 6.0, [World]()
			{
				const int32 Before = ZennyOf(World)->GetInventory()->CountOf(TEXT("item.material.timber_plank"));
				Stand(World, FVector2D(-1400, -4700), 0.0);
				Salvage(World, DPine, TEXT("trunk"));
				const int32 After = ZennyOf(World)->GetInventory()->CountOf(TEXT("item.material.timber_plank"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.FellTree: S gathered the log: planks %d -> %d; %s; %s"), Before, After, *Describe(World, DPine),
					After > Before && Describe(World, DPine).Contains(TEXT("trunk=debris-salvaged")) ? TEXT("PASS") : TEXT("FAIL"));
			} },
		});
	}

	/** Reports every structure placement's facts (used after a restart: F, R, T). */
	void ReportNow(UWorld* World)
	{
		for (const FName& Placement : { DCarport, DCarportEdge, DPine, DPineEdge })
		{
			UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureReport: %s"), *Describe(World, Placement));
		}
		UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureReport: Zenny health %.0f; active collapses %d; impacts this session %d"), HealthOf(World),
			World->GetSubsystem<UGLStructureSubsystem>()->ActiveCollapses(), World->GetSubsystem<UGLStructureSubsystem>()->GetImpacts().Num());
	}

	void Report(UWorld* World)
	{
		RunSteps(World, { { 2.0, [World]() { ReportNow(World); } } });
	}

	void QuitIn(const TArray<FString>& Args, UWorld* World)
	{
		const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.f;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([]() { GEngine->DeferredCommands.Add(TEXT("quit")); }), FMath::Max(0.1f, Seconds), false);
	}

	void Move(UWorld* World, const FVector2D& To)
	{
		Stand(World, To);
		UGLGridSubsystem* Grid = World->GetSubsystem<UGLGridSubsystem>();
		Grid->Advance(FVector(To, 0.0));
		Grid->FlushAll();
		Stand(World, To);
	}

	/** O-R: structural changes at the boundary, crossing during and after, save in the neighbouring cell. */
	void Edge(UWorld* World)
	{
		RunSteps(World, {
			{ 0.5, [World]()
			{
				Move(World, FVector2D(48600, 1500));
				Salvage(World, DCarportEdge, TEXT("post_south"));
				Salvage(World, DCarportEdge, TEXT("post_north"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: O collapse decided 16 m from the boundary: %s"), *Describe(World, DCarportEdge));
			} },
			{ 0.9, [World]()
			{
				// P: cross away while the decks are still falling.
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: P crossing away mid-fall (active collapses %d)"), World->GetSubsystem<UGLStructureSubsystem>()->ActiveCollapses());
				Move(World, FVector2D(120000, 0));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: deep in the lots: origin loaded %d; active collapses %d"),
					World->GetSubsystem<UGLGridSubsystem>()->IsLoaded(DOrigin) ? 1 : 0, World->GetSubsystem<UGLStructureSubsystem>()->ActiveCollapses());
			} },
			{ 2.0, [World]() { Move(World, FVector2D(48600, 1500)); UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: back: %s"), *Describe(World, DCarportEdge)); } },
			{ 3.0, [World]()
			{
				Stand(World, FVector2D(49400, -1500), 0.0); // west of the edge pine
				Salvage(World, DPineEdge, TEXT("stump"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: chopped the edge pine: %s"), *Describe(World, DPineEdge));
			} },
			{ 4.0, [World]() { Move(World, FVector2D(120000, 0)); } },
			{ 5.0, [World]() { Move(World, FVector2D(48600, 1500)); } },
			{ 8.0, [World]()
			{
				Stand(World, FVector2D(48600, 1500));
				Salvage(World, DCarportEdge, TEXT("deck_east"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: salvaged one piece of debris: %s"), *Describe(World, DCarportEdge));
			} },
			{ 9.0, [World]() { Move(World, FVector2D(120000, 0)); } },
			{ 10.0, [World]() { Move(World, FVector2D(48600, 1500)); } },
			{ 11.0, [World]()
			{
				const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
				int32 Actors = 0;
				for (TActorIterator<AGLStructurePart> It(World); It; ++It)
				{
					Actors += It->StructurePlacement == DCarportEdge ? 1 : 0;
				}
				const FString Carport = Describe(World, DCarportEdge);
				const bool bOk = Carport.Contains(TEXT("post_south=removed")) && Carport.Contains(TEXT("post_north=removed")) && Carport.Contains(TEXT("deck_west=debris@"))
					&& Carport.Contains(TEXT("deck_east=debris-salvaged")) && !Carport.Contains(TEXT("NOT at rest")) && Actors == 1 && Structures->ActiveCollapses() == 0;
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: Q after 4 crossings: %s; carport actors %d; %s | %s"), *Carport, Actors, *Describe(World, DPineEdge), bOk ? TEXT("PASS") : TEXT("FAIL"));
				Move(World, FVector2D(120000, 0));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.StructureEdge: R standing in the lots (the origin streamed out); quitting saves"));
			} },
		});
	}

	/** I-N: proof creatures on open ground (each its own home, far apart): hearing in and out of range, memory after cover, noise as a distraction. */
	void Noise(UWorld* World)
	{
		static TWeakObjectPtr<AGLCreature> C[5];
		static const FVector2D H[5] = { FVector2D(-8000, 3000), FVector2D(-8000, 9000), FVector2D(-14000, 3000), FVector2D(-14000, 15000), FVector2D(-8000, 15000) };
		auto Spawn = [World](int32 I)
		{
			UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			C[I] = World->GetSubsystem<UGLPlacementSubsystem>()->SpawnProofCreature(DGremlin, FVector(H[I], Terrain->HeightAt(H[I])), 0.0, DOrigin); // facing +X
		};
		auto Dig = [World](const FVector2D& At, const TCHAR* Op)
		{
			AGLCharacter* Zenny = ZennyOf(World);
			Zenny->GetInventory()->AddItem(TEXT("item.tool.shovel"), 1);
			Zenny->GetInventory()->AddItem(TEXT("item.material.soil"), 5);
			World->GetSubsystem<UGLTerrainSubsystem>()->Terraform(Zenny, Op, At);
			return World->GetSubsystem<UGLNoiseSubsystem>()->GetRecent().Last();
		};
		auto State = [](int32 I) { return C[I].IsValid() ? GLCreatureRules::StateName(C[I]->GetState()) : TEXT("none"); };
		auto FromHome = [](int32 I) { return C[I].IsValid() ? FVector::Dist2D(C[I]->GetActorLocation(), FVector(H[I], 0.0)) / 100.0 : -1.0; };
		static bool bSpotted = false;
		RunSteps(World, {
			// I/J: a dig behind it, out of its sight, inside its hearing.
			{ 0.5, [World, Spawn]() { Spawn(0); Stand(World, H[0] + FVector2D(-900, -700), 0.0); } },
			{ 2.0, [Dig]()
			{
				const FGLNoiseEvent Noise = Dig(H[0] + FVector2D(-900, -300), TEXT("terraform.shovel.dig"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: I dig 9.5 m behind creature 1, out of its sight (%s, %.0f m reach; it hears to 16 m): heard by %d"), *Noise.Action.ToString(), Noise.RadiusCm / 100.0, Noise.Heard);
			} },
			{ 2.1, [World]() { Stand(World, H[0] + FVector2D(-900, -1600), 0.0); } }, // Zenny steps back out of the way
			{ 2.4, [State]() { UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: J creature 1 reacts: %s"), State(0)); } },
			{ 7.0, [State, FromHome]() { UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: J creature 1 walked to the noise: %s, %.1f m from home (the dig was 9.5 m away) | %s"), State(0), FromHome(0),
				FromHome(0) > 4.0 && FString(State(0)) != TEXT("Chase") ? TEXT("PASS") : TEXT("FAIL")); } },
			// K: the same dig 20 m from another creature: not heard.
			{ 12.0, [World, Spawn]() { Spawn(1); Stand(World, H[1] + FVector2D(-2000, -400), 0.0); } },
			{ 13.0, [Dig]()
			{
				const FGLNoiseEvent Noise = Dig(H[1] + FVector2D(-2000, 0), TEXT("terraform.shovel.dig"));
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: K dig 20 m from creature 2: heard by %d"), Noise.Heard);
			} },
			{ 15.0, [State, FromHome]() { UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: K creature 2 did not react: %s, %.1f m from home | %s"), State(1), FromHome(1),
				FString(State(1)) == TEXT("Idle") && FromHome(1) < 1.0 ? TEXT("PASS") : TEXT("FAIL")); } },
			// L/M: creature 3 detects Zenny; Zenny raises cover, then slips far away.
			{ 16.0, [World, Spawn]() { Spawn(2); } },
			{ 17.0, [World]() { Stand(World, H[2] + FVector2D(700, 0), 180.0); } },
			{ 17.4, [World, Dig, State]()
			{
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: L creature 3 detected Zenny: %s"), State(2));
				for (int32 Stroke = 0; Stroke < 8; ++Stroke)
				{
					Dig(H[2] + FVector2D(400, 0), TEXT("terraform.shovel.raise"));
				}
			} },
			{ 17.7, [State]()
			{
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: M cover raised between them: creature 3 is %s, last known %s, %.1f s of searching left"), State(2),
					C[2].IsValid() ? *C[2]->GetLastKnown().ToCompactString() : TEXT("-"), C[2].IsValid() ? C[2]->GetSearchSecondsLeft() : 0.0);
			} },
			{ 17.8, [World]() { Stand(World, H[2] + FVector2D(0, 3500), 0.0); } }, // Zenny slips far away
			{ 20.0, [State, FromHome]() { UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: M creature 3 goes to the last known position: %s, %.1f m from home"), State(2), FromHome(2)); } },
			{ 30.0, [State, FromHome]() { UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: M memory spent: creature 3 is %s, %.1f m from home"), State(2), FromHome(2)); } },
			// N control: walking past creature 5's front is seen.
			{ 32.0, [World, Spawn]() { Spawn(4); Stand(World, H[4] + FVector2D(600, -1500), 90.0); } },
			{ 33.0, [World]() { Stand(World, H[4] + FVector2D(600, -500), 90.0); } },
			{ 34.0, [World]() { Stand(World, H[4] + FVector2D(600, 0), 90.0); } },
			{ 34.5, [State]() { UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: N control: passing 6 m in front of creature 5 without a distraction: %s"), State(4)); bSpotted = FString(State(4)) == TEXT("Chase") || FString(State(4)) == TEXT("Attack"); } },
			{ 35.0, [World]() { Stand(World, H[4] + FVector2D(600, 3000), 90.0); } },
			// N: a deliberate noise behind creature 4, then the same walk.
			{ 36.0, [World, Spawn]() { Spawn(3); Stand(World, H[3] + FVector2D(-800, -1200), 0.0); } },
			{ 37.0, [Dig, FromHome]() { Dig(H[3] + FVector2D(-1000, -300), TEXT("terraform.shovel.dig")); UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: N a deliberate dig 10.4 m behind creature 4")); } },
			{ 37.1, [World]() { Stand(World, H[3] + FVector2D(600, -1500), 90.0); } },
			{ 43.0, [State, FromHome]() { UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: N creature 4 went to the noise: %s, %.1f m from home"), State(3), FromHome(3)); } },
			{ 43.2, [World]() { Stand(World, H[3] + FVector2D(600, -500), 90.0); } },
			{ 44.0, [World]() { Stand(World, H[3] + FVector2D(600, 0), 90.0); } },
			{ 45.0, [World]() { Stand(World, H[3] + FVector2D(600, 500), 90.0); } },
			{ 46.0, [World, State]()
			{
				Stand(World, H[3] + FVector2D(600, 1500), 90.0);
				const bool bUnseen = FString(State(3)) != TEXT("Chase") && FString(State(3)) != TEXT("Attack");
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Noise: N Zenny passed in front of creature 4's home unseen: %s (it is %s); the control was spotted: %s | %s"),
					bUnseen ? TEXT("yes") : TEXT("no"), State(3), bSpotted ? TEXT("yes") : TEXT("no"), bUnseen && bSpotted ? TEXT("PASS") : TEXT("FAIL"));
			} },
		});
	}

	FAutoConsoleCommandWithWorldAndArgs CollapseCommand(TEXT("gl.Demo.Collapse"),
		TEXT("DEV ONLY (P6): Zenny takes the carport's posts; the decks collapse on him. 'kill' starts him at 50 health."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Collapse));
	FAutoConsoleCommandWithWorld CollapseCreatureCommand(TEXT("gl.Demo.CollapseCreature"),
		TEXT("DEV ONLY (P6): a (proof) creature under the carport; the same collapse defeats it."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CollapseCreature));
	FAutoConsoleCommandWithWorld FellTreeCommand(TEXT("gl.Demo.FellTree"),
		TEXT("DEV ONLY (P6): chop the pine near the start, watch it topple, gather the log."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FellTree));
	FAutoConsoleCommandWithWorld ReportCommand(TEXT("gl.Demo.StructureReport"),
		TEXT("DEV ONLY (P6): logs every authored structure's facts."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Report));
	FAutoConsoleCommandWithWorld EdgeCommand(TEXT("gl.Demo.StructureEdge"),
		TEXT("DEV ONLY (P6): collapse and fell at the cell boundary, cross during and after, end in the lots."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Edge));
	FAutoConsoleCommandWithWorldAndArgs QuitInCommand(TEXT("gl.Demo.QuitIn"),
		TEXT("DEV ONLY: quits (autosaving) after N seconds."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&QuitIn));
	FAutoConsoleCommandWithWorld NoiseCommand(TEXT("gl.Demo.Noise"),
		TEXT("DEV ONLY (P6): a (proof) creature hears digging only in range, searches after losing sight, and a noise draws it away."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Noise));
}

#endif
