// P12 (ADR-0040) DEV ONLY: the public-intent build-mode proof in the real game. Everything is built through build mode's
// intents (the same calls the keys make) with the player's real camera aiming: before every commit the camera is turned to
// the target, and the proof checks that the view's aim is the camera's view (CAMERA AIM == GAMEPLAY AIM) and that what
// was committed is what the view showed. Modes: build (a new world: base, storage, frames, finishes, window wall, upper
// floor, stair climbed by Zenny, removal and collapse, salvage, build camera, interaction counts, screenshots) and restart
// (the house is back exactly, favorites persist, it streams away and back, building continues).
// Results: Saved/P12/intent-<mode>.json, "gl.Building.IntentProof:" log lines, Saved/P12/screenshots/<mode>-<n>-<name>.png.

#include "Building/GLBuildModeComponent.h"
#include "Building/GLBuildText.h"
#include "Building/GLBuildingSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/GLCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/GLInteractorComponent.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "UnrealClient.h"
#include "World/GLGridSubsystem.h"
#include "World/GLWinchesterHouse.h"

#if !UE_BUILD_SHIPPING

namespace
{
	const FName IPOrigin(TEXT("cell.home.origin"));
	const FVector IPAnchor(18000.0, 18000.0, 0.0);
	const FVector IPFarAway(120000.0, 18000.0, 0.0);
	const FName IPFoundation(TEXT("buildpiece.modern.timber_foundation"));
	const FName IPWall(TEXT("buildpiece.modern.timber_wall"));
	const FName IPWindow(TEXT("buildpiece.modern.window_wall"));
	const FName IPUpper(TEXT("buildpiece.modern.upper_floor"));
	const FName IPStair(TEXT("buildpiece.modern.timber_stair"));
	const FName IPCore(TEXT("buildpiece.modern.base_core"));
	const FName IPCrate(TEXT("buildpiece.modern.storage_crate"));
	const FName IPPost(TEXT("buildpiece.modern.porch_post"));
	const FName IPClapboard(TEXT("finish.victorian.clapboard"));
	const FName IPStud(TEXT("item.component.stud"));

	struct FIPStep
	{
		FString Name;
		/** Returns true when the step is done (it is called again next frame otherwise). */
		TFunction<bool(double Since)> Run;
	};

	struct FIPRun
	{
		FString Mode;
		TWeakObjectPtr<UWorld> World;
		TArray<FIPStep> Steps;
		int32 Index = 0;
		double StepAt = 0.0;
		double Started = 0.0;
		double HoldUntil = 0.0; // a screenshot is taken at the end of a later frame: nothing moves until then
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Checks;
		bool bPass = true;
		FTSTicker::FDelegateHandle Ticker;
		int32 Shots = 0;
		// The house's pieces by role (for the steps and the restart's identity).
		TMap<FString, int32> Ids;
		TArray<int32> Walls;
		TArray<int32> Posts;
		int32 AimFrames = 0;
		// CAMERA AIM == GAMEPLAY AIM over every commit.
		int32 AimChecks = 0;
		int32 AimMismatches = 0;
		int32 CommitChecks = 0;
		int32 CommitMismatches = 0;
		TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
		int32 CountBase = 0;
		double MaxClimbZ = -1e9;
		int32 HoldTarget = 0;
		TArray<int32> HoldPrediction;
		TMap<FName, int32> Before;
	};
	TSharedPtr<FIPRun> IP;

	AGLCharacter* IPZenny() { return IP && IP->World.IsValid() ? Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(IP->World.Get(), 0)) : nullptr; }
	APlayerController* IPController() { return IP && IP->World.IsValid() ? IP->World->GetFirstPlayerController() : nullptr; }
	UGLBuildModeComponent* IPBuild() { AGLCharacter* Z = IPZenny(); return Z ? Z->FindComponentByClass<UGLBuildModeComponent>() : nullptr; }
	UGLBuildingSubsystem* IPBuilding() { return IP->World->GetSubsystem<UGLBuildingSubsystem>(); }
	UGLStructureSubsystem* IPStructures() { return IP->World->GetSubsystem<UGLStructureSubsystem>(); }
	FVector IPAt(double X, double Y, double Z = 0.0) { return IPAnchor + FVector(X, Y, Z); }

	void IPNote(const FString& Name, bool bPass, const FString& Detail)
	{
		TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
		C->SetStringField(TEXT("check"), Name);
		C->SetBoolField(TEXT("pass"), bPass);
		C->SetStringField(TEXT("detail"), Detail);
		IP->Checks.Add(MakeShared<FJsonValueObject>(C));
		IP->bPass &= bPass;
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: %s %s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail);
	}

	FString IPDir() { return FPaths::ProjectSavedDir() / TEXT("P12"); }

	/** A frame with the HUD (the build bar, panels and markers are the evidence). */
	void IPShot(const TCHAR* Name)
	{
		const FString File = IPDir() / TEXT("screenshots") / FString::Printf(TEXT("%s-%d-%s.png"), *IP->Mode, IP->Shots++, Name);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		FScreenshotRequest::RequestScreenshot(File, /*bShowUI=*/true, /*bAddFilenameSuffix=*/false);
		IP->HoldUntil = IP->World->GetTimeSeconds() + 0.4;
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: screenshot %s"), *File);
	}

	/** Stands Zenny still (movement held) at a ground point (Z from the terrain). */
	void IPStand(double X, double Y)
	{
		AGLCharacter* Zenny = IPZenny();
		const FVector P = IPAt(X, Y);
		const double Ground = IP->World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(FVector2D(P));
		Zenny->GetCharacterMovement()->DisableMovement();
		Zenny->SetActorLocation(FVector(P.X, P.Y, Ground + Zenny->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0), false, nullptr, ETeleportType::TeleportPhysics);
	}

	/** Turns the player's camera toward Target, a frame at a time, until it points there (true then). */
	bool IPAim(const FVector& Target)
	{
		APlayerController* PC = IPController();
		if (!PC || !PC->PlayerCameraManager)
		{
			return false;
		}
		const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
		const FVector Want = (Target - Cam).GetSafeNormal();
		const FVector Have = PC->PlayerCameraManager->GetCameraRotation().Vector();
		if (FVector::DotProduct(Want, Have) > FMath::Cos(FMath::DegreesToRadians(0.15)) && ++IP->AimFrames > 2)
		{
			IP->AimFrames = 0;
			return true;
		}
		PC->SetControlRotation(Want.Rotation());
		return false;
	}

	/** The gate: the build view aims along the camera's view (the ray the commit will use). */
	void IPCheckAim(const TCHAR* Where)
	{
		APlayerController* PC = IPController();
		UGLBuildModeComponent* Build = IPBuild();
		Build->RefreshView();
		const FGLBuildView& View = Build->GetView();
		const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
		const FVector Forward = PC->PlayerCameraManager->GetCameraRotation().Vector();
		const bool bSame = FVector::Dist(View.AimStart, Cam) < 2.0 && FVector::DotProduct((View.AimEnd - View.AimStart).GetSafeNormal(), Forward) > 0.99999;
		++IP->AimChecks;
		if (!bSame)
		{
			++IP->AimMismatches;
			UE_LOG(LogGridlands, Warning, TEXT("gl.Building.IntentProof: aim mismatch at %s: view %s -> %s, camera %s fwd %s"), Where,
				*View.AimStart.ToString(), *View.AimEnd.ToString(), *Cam.ToString(), *Forward.ToString());
		}
	}

	/** Primary in PLACE: the committed piece must be the view's candidate. Returns the new piece (0: nothing placed). */
	int32 IPPlace(const TCHAR* Where)
	{
		UGLBuildModeComponent* Build = IPBuild();
		IPCheckAim(Where);
		const FGLBuildView View = Build->GetView();
		const int32 Before = IPBuilding()->GetPieces().Num();
		Build->Primary();
		const TArray<FGLPlacedPiece> After = IPBuilding()->GetPieces();
		if (After.Num() != Before + 1 || !View.bHasCandidate)
		{
			UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: nothing placed at %s: aim %s, %s"), Where, *(View.AimPoint - IPAnchor).ToCompactString(), *Build->StatusLine());
			return 0;
		}
		const FGLPlacedPiece& Placed = After.Last();
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: placed %s %d at %s yaw %.1f (aim %s)"), Where, Placed.Id, *(Placed.Location - IPAnchor).ToCompactString(),
			GLStructureRules::YawDegrees(Placed.YawStep), *(View.AimPoint - IPAnchor).ToCompactString());
		++IP->CommitChecks;
		if (!Placed.Location.Equals(View.Candidate.Location, 0.5) || Placed.YawStep != View.Candidate.YawStep || Placed.Def != View.Candidate.Def)
		{
			++IP->CommitMismatches;
			UE_LOG(LogGridlands, Warning, TEXT("gl.Building.IntentProof: commit mismatch at %s"), Where);
		}
		return Placed.Id;
	}

	/** Browser selection by intents only. */
	void IPChoose(FName Piece)
	{
		UGLBuildModeComponent* Build = IPBuild();
		if (Build->GetView().State == EGLBuildState::Browse)
		{
			Build->ToggleBrowser();
		}
		const TArray<FGLCatalogCategory>& Catalog = Build->GetCatalog();
		const int32 Category = Catalog.IndexOfByPredicate([Piece](const FGLCatalogCategory& C) { return C.Pieces.Contains(Piece); });
		Build->ToggleBrowser();
		Build->RefreshView();
		for (int32 Guard = 0; Guard < 32 && Build->GetView().BrowseCategory != Category; ++Guard) { Build->BrowseCategory(1); Build->RefreshView(); }
		for (int32 Guard = 0; Guard < 64 && Build->PiecesOf(Category).IsValidIndex(Build->GetView().BrowseIndex)
			&& Build->PiecesOf(Category)[Build->GetView().BrowseIndex] != Piece; ++Guard) { Build->BrowseMove(1); Build->RefreshView(); }
		Build->BrowseSelect();
	}

	void IPCount(const TCHAR* Scenario, const FString& How)
	{
		const int32 N = IPBuild()->GetIntentTotal() - IP->CountBase;
		TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
		C->SetNumberField(TEXT("intents"), N);
		C->SetStringField(TEXT("how"), How);
		IP->Counts->SetObjectField(Scenario, C);
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: interactions %s: %d (%s)"), Scenario, N, *How);
	}

	FString IPFingerprint()
	{
		TArray<FString> Lines;
		for (const FGLPlacedPiece& P : IPBuilding()->GetPieces())
		{
			Lines.Add(FString::Printf(TEXT("%d %s %s %d %s"), P.Id, *P.Def.ToString(), *P.Location.ToCompactString(), P.YawStep,
				*FString::JoinBy(P.Layers, TEXT(","), [](FName L) { return L.ToString(); })));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	TMap<FName, int32> IPCarried()
	{
		TMap<FName, int32> Out;
		for (const FGLInventoryStack& S : IPZenny()->FindComponentByClass<UGLInventoryComponent>()->GetInventory().GetStacks())
		{
			Out.FindOrAdd(S.Item) += S.Count;
		}
		return Out;
	}

	TMap<FName, int32> IPDelta(const TMap<FName, int32>& A, const TMap<FName, int32>& B)
	{
		TMap<FName, int32> Out;
		for (const auto& E : B) { if (const int32 D = E.Value - A.FindRef(E.Key)) { Out.Add(E.Key, D); } }
		for (const auto& E : A) { if (!B.Contains(E.Key)) { Out.Add(E.Key, -E.Value); } }
		return Out;
	}

	void IPWrite()
	{
		IP->Out->SetStringField(TEXT("mode"), IP->Mode);
		IP->Out->SetArrayField(TEXT("checks"), IP->Checks);
		IP->Out->SetObjectField(TEXT("interactionCounts"), IP->Counts);
		IP->Out->SetNumberField(TEXT("aimChecks"), IP->AimChecks);
		IP->Out->SetNumberField(TEXT("aimMismatches"), IP->AimMismatches);
		IP->Out->SetNumberField(TEXT("commitChecks"), IP->CommitChecks);
		IP->Out->SetNumberField(TEXT("commitMismatches"), IP->CommitMismatches);
		TSharedRef<FJsonObject> Ids = MakeShared<FJsonObject>();
		for (const auto& Id : IP->Ids) { Ids->SetNumberField(Id.Key, Id.Value); }
		IP->Out->SetObjectField(TEXT("ids"), Ids);
		IP->Out->SetBoolField(TEXT("pass"), IP->bPass);
		FString Text;
		FJsonSerializer::Serialize(IP->Out, TJsonWriterFactory<>::Create(&Text));
		IFileManager::Get().MakeDirectory(*IPDir(), true);
		FFileHelper::SaveStringToFile(Text, *(IPDir() / FString::Printf(TEXT("intent-%s.json"), *IP->Mode)));
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: %s RESULT %s (%d checks)"), *IP->Mode, IP->bPass ? TEXT("PASS") : TEXT("FAIL"), IP->Checks.Num());
	}

	bool IPReadPrevious(FString& OutFingerprint)
	{
		FString Text;
		TSharedPtr<FJsonObject> O;
		if (!FFileHelper::LoadFileToString(Text, *(IPDir() / TEXT("intent-build.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O)
		{
			return false;
		}
		OutFingerprint = O->GetStringField(TEXT("fingerprintAtEnd"));
		for (const auto& Id : O->GetObjectField(TEXT("ids"))->Values) { IP->Ids.Add(FString(Id.Key), static_cast<int32>(Id.Value->AsNumber())); }
		return true;
	}

	bool IPCellReady()
	{
		const UGLGridSubsystem* Grid = IP->World->GetSubsystem<UGLGridSubsystem>();
		return Grid && Grid->IsComplete(IPOrigin) && IPStructures()->IsCellPresented(IPOrigin);
	}

	void IPQuit()
	{
		IPWrite();
		FTSTicker::GetCoreTicker().RemoveTicker(IP->Ticker);
		GEngine->DeferredCommands.Add(TEXT("quit")); // quitting autosaves
	}

	bool IPTick(float)
	{
		UWorld* World = IP.IsValid() ? IP->World.Get() : nullptr;
		if (!World || !IPZenny())
		{
			return true;
		}
		if (World->GetTimeSeconds() - IP->Started > 420.0)
		{
			IPNote(TEXT("finished in time"), false, FString::Printf(TEXT("stuck at step %d (%s)"), IP->Index, IP->Steps.IsValidIndex(IP->Index) ? *IP->Steps[IP->Index].Name : TEXT("-")));
			IPQuit();
			return false;
		}
		if (World->GetTimeSeconds() < IP->HoldUntil)
		{
			IP->StepAt = World->GetTimeSeconds();
			return true;
		}
		if (!IP->Steps.IsValidIndex(IP->Index))
		{
			IP->Out->SetStringField(TEXT("fingerprintAtEnd"), IPFingerprint());
			IPQuit();
			return false;
		}
		if (IP->Steps[IP->Index].Run(World->GetTimeSeconds() - IP->StepAt))
		{
			UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: step %d done: %s"), IP->Index, *IP->Steps[IP->Index].Name);
			++IP->Index;
			IP->StepAt = World->GetTimeSeconds();
		}
		return true;
	}

	/** Aims at Target (frames), then runs Act once. */
	FIPStep IPAimThen(const FString& Name, TFunction<FVector()> Target, TFunction<void()> Act)
	{
		return { Name, [Target, Act](double) { if (!IPAim(Target())) { return false; } Act(); return true; } };
	}

	FIPStep IPDo(const FString& Name, TFunction<void()> Act) { return { Name, [Act](double) { Act(); return true; } }; }
	FIPStep IPWait(const FString& Name, double Seconds) { return { Name, [Seconds](double Since) { return Since >= Seconds; } }; }

	void IPBuildSteps()
	{
		TArray<FIPStep>& S = IP->Steps;
		S.Add({ TEXT("home cell ready"), [](double) { if (!IPCellReady()) { return false; } IPStand(-500, -450); return true; } });
		S.Add(IPDo(TEXT("pad and starter kit"), []
		{
			FGLWinchesterHouse Pad;
			Pad.Anchor = IPAnchor;
			Pad.PreparePad(IP->World.Get());
			GEngine->Exec(IP->World.Get(), TEXT("gl.Dev.BuildingStarterKit"));
			IPBuild()->SetProfilePath(IPDir() / TEXT("build-profile.json")); // the proof's own profile (the operator's stays untouched)
			IPBuild()->SetCameraMode(EGLHoldMode::Hold);
			IPBuild()->SetConfirmMode(EGLHoldMode::Hold);
			IPBuild()->ToggleBuild();
		}));
		S.Add(IPWait(TEXT("settle"), 1.0));
		// The base: a base core and a crate on a floor (chosen through the browser), materials stored by the real verb.
		S.Add(IPDo(TEXT("choose base core"), [] { IPStand(-900, -150); IPChoose(IPCore); }));
		S.Add(IPAimThen(TEXT("place base core"), [] { return IPAt(-1000, -550, 0); }, []
		{
			IP->Ids.Add(TEXT("core"), IPPlace(TEXT("core")));
			const TArray<FGLClaim> Claims = IPBuilding()->Claims();
			IPBuild()->RefreshView();
			const FGLBuildView& View = IPBuild()->GetView();
			IPNote(TEXT("base core placed through the browser; the claim shown is the canonical claim"), IP->Ids[TEXT("core")] != 0 && Claims.Num() == 1 && View.bShowClaim
				&& View.ClaimAreas.Num() == 1 && View.ClaimAreas[0].Centre.Equals(Claims[0].Areas[0].Centre, 0.01) && FMath::IsNearlyEqual(View.ClaimAreas[0].RadiusCm, Claims[0].Areas[0].RadiusCm),
				FString::Printf(TEXT("%d claim(s), shown %d area(s) of %.0f m (provisional)"), Claims.Num(), View.ClaimAreas.Num(), View.ClaimAreas.Num() ? View.ClaimAreas[0].RadiusCm / 100.0 : 0.0));
		}));
		S.Add(IPDo(TEXT("choose foundation and pin it"), [] { IPChoose(IPFoundation); IPBuild()->PinFavorite(0); }));
		S.Add(IPAimThen(TEXT("crate floor"), [] { return IPAt(-700, -450, 0); }, [] { IP->Ids.Add(TEXT("crateFloor"), IPPlace(TEXT("crate floor"))); }));
		S.Add(IPDo(TEXT("choose crate"), [] { IPChoose(IPCrate); }));
		S.Add(IPAimThen(TEXT("crate"), [] { return IPAt(-700, -450, 30); }, [] { IP->Ids.Add(TEXT("crate"), IPPlace(TEXT("crate"))); }));
		S.Add(IPDo(TEXT("store materials"), []
		{
			IPBuild()->ToggleBuild(); // hands free: the ordinary interaction (E) on the crate
			IPStand(-700, -700);
		}));
		// The interactor looks from Zenny's eyes (not the build camera): face the crate from there.
		S.Add(IPDo(TEXT("face the crate"), [] { IPController()->SetControlRotation((IPAt(-700, -450, 60) - IPZenny()->GetPawnViewLocation()).Rotation()); }));
		S.Add({ TEXT("store into the crate"), [](double Since)
		{
			if (Since < 0.3)
			{
				return false; // the interactor's focus is updated by its tick
			}
			{
				FVector Eye;
				FRotator Rot;
				IPZenny()->GetActorEyesViewPoint(Eye, Rot);
				FHitResult Hit;
				FCollisionQueryParams Params(TEXT("IPStore"), false, IPZenny());
				const bool bHit = IP->World->LineTraceSingleByChannel(Hit, Eye, Eye + Rot.Vector() * 300.0, ECC_Visibility, Params);
				UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: store trace from %s dir %s: %s"), *(Eye - IPAnchor).ToCompactString(), *Rot.Vector().ToCompactString(),
					bHit ? *FString::Printf(TEXT("%s (%s) at %s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *(FVector(Hit.ImpactPoint) - IPAnchor).ToCompactString()) : TEXT("nothing"));
			}
			const int32 Carried = IPCarried().FindRef(IPStud);
			const UObject* Focus = IPZenny()->GetInteractor()->GetFocus();
			UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: store focus %s, Zenny %s"), Focus ? *Focus->GetName() : TEXT("none"), *(IPZenny()->GetActorLocation() - IPAnchor).ToCompactString());
			IPZenny()->GetInteractor()->TryInteract(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Store")));
			const FGLInventory* Crate = IPBuilding()->StorageOf(IP->Ids[TEXT("crate")]);
			IPNote(TEXT("materials stored in base storage by the real verb"), Crate && Crate->CountOf(IPStud) > 0 && IPCarried().FindRef(IPStud) < Carried,
				FString::Printf(TEXT("crate studs %d (Zenny %d -> %d)"), Crate ? Crate->CountOf(IPStud) : -1, Carried, IPCarried().FindRef(IPStud)));
			IPBuild()->ToggleBuild();
			return true;
		} });
		// The row of floors (the favourite selects the foundation).
		S.Add(IPDo(TEXT("favourite foundation"), [] { IPBuild()->SelectFavorite(0); }));
		for (const double X : { -600.0, -400.0, -200.0, 0.0, 200.0, 400.0, 600.0, 800.0 })
		{
			// Zenny walks along the row (walking is not an interaction): each floor is in front of him, nothing between.
			S.Add(IPDo(FString::Printf(TEXT("stand for floor %.0f"), X), [X] { IPStand(X, -400); }));
			S.Add(IPAimThen(FString::Printf(TEXT("floor %.0f"), X), [X] { return IPAt(X, 0, 0); }, [X] { IP->Ids.Add(FString::Printf(TEXT("floor%.0f"), X), IPPlace(TEXT("floor"))); }));
		}
		S.Add(IPDo(TEXT("floors placed"), []
		{
			int32 Floors = 0;
			for (const auto& Id : IP->Ids) { Floors += Id.Key.StartsWith(TEXT("floor")) && Id.Value ? 1 : 0; }
			IPNote(TEXT("a row of 8 floors through the favourite"), Floors == 8 && IPBuild()->GetSelectedPiece() == IPFoundation, FString::Printf(TEXT("%d floors"), Floors));
		}));
		// Ten identical snapped walls (MEASURED): north edges from the north, south edges from the south.
		S.Add(IPDo(TEXT("choose wall (measured from here)"), [] { IP->CountBase = IPBuild()->GetIntentTotal(); IPChoose(IPWall); }));
		for (const double Y : { 85.0, -85.0 })
		{
			for (const double X : { 0.0, 200.0, 400.0, 600.0, 800.0 })
			{
				S.Add(IPDo(FString::Printf(TEXT("stand for wall %.0f,%.0f"), X, Y), [X, Y] { IPStand(X, Y > 0 ? 450 : -450); }));
				S.Add(IPAimThen(FString::Printf(TEXT("wall %.0f,%.0f"), X, Y), [X, Y] { return IPAt(X, Y, 30); }, [X, Y]
				{
					IPBuild()->RefreshView();
					const FGLBuildView View = IPBuild()->GetView();
					const int32 Id = IPPlace(TEXT("wall"));
					IP->Walls.Add(Id);
					IP->Ids.Add(FString::Printf(TEXT("wall%.0f%s"), X, Y > 0 ? TEXT("n") : TEXT("s")), Id);
					const FGLStructurePartRuntime* Part = Id ? IPStructures()->FindPlayerPiece(Id) : nullptr;
					const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(IPWall);
					const FGLWorldSocket* Own = Part && Def ? GLStructureRules::Sockets(*Def, Part->Piece).FindByPredicate([&View](const FGLWorldSocket& Socket) { return Socket.Name == View.Snap.OwnSocket; }) : nullptr;
					if (!Own || !View.Snap.bSnapped || !Own->Location.Equals(View.Snap.TargetLocation, 0.5))
					{
						IPNote(TEXT("snap marker == committed socket"), false, FString::Printf(TEXT("wall at %.0f,%.0f"), X, Y));
					}
				}));
			}
		}
		S.Add(IPDo(TEXT("walls placed"), []
		{
			IPCount(TEXT("tenSnappedWalls"), TEXT("choose the wall in the browser, then aim and click per wall"));
			IPNote(TEXT("ten identical snapped walls, each where its ghost and snap marker were"), !IP->Walls.Contains(0) && IP->Walls.Num() == 10,
				FString::Printf(TEXT("%d walls"), IP->Walls.Num()));
			IPShot(TEXT("walls-framed"));
		}));
		// Finishing ten walls with the same finish (MEASURED).
		S.Add(IPDo(TEXT("finish mode (measured from here)"), [] { IP->CountBase = IPBuild()->GetIntentTotal(); IPBuild()->ToggleFinishMode(); }));
		for (int32 I = 0; I < 10; ++I)
		{
			// From the south the north walls are behind the south ones: the south walls first, then the north ones from the north.
			const bool bSouthWall = I < 5;
			const double X = 200.0 * (I % 5);
			S.Add(IPDo(FString::Printf(TEXT("stand for finish %d"), I), [X, bSouthWall] { IPStand(X, bSouthWall ? -450 : 450); }));
			S.Add(IPAimThen(FString::Printf(TEXT("finish wall %d"), I), [X, bSouthWall] { return IPAt(X, bSouthWall ? -110 : 110, 150); }, [I]
			{
				UGLBuildModeComponent* Build = IPBuild();
				IPCheckAim(TEXT("finish"));
				if (I == 0)
				{
					for (int32 Guard = 0; Guard < 8 && Build->GetView().Finish != IPClapboard; ++Guard) { Build->CycleFinish(1); }
					const FGLCostView Cost = Build->GetView().FinishCost;
					const FGLInventory* Crate = IPBuilding()->StorageOf(IP->Ids[TEXT("crate")]);
					const int32 CrateBefore = Crate ? Crate->CountOf(TEXT("item.material.timber_plank")) : 0, MineBefore = IPCarried().FindRef(TEXT("item.material.timber_plank"));
					const int32 Target = Build->GetView().FinishTarget;
					Build->Primary();
					const int32 FromCrate = CrateBefore - (Crate ? Crate->CountOf(TEXT("item.material.timber_plank")) : 0), FromMe = MineBefore - IPCarried().FindRef(TEXT("item.material.timber_plank"));
					const FGLStructurePartRuntime* Part = IPStructures()->FindPlayerPiece(Target);
					IPNote(TEXT("the chosen finish is installed; its cost and sources are what was shown"), Part && Part->Piece.Layers.Contains(IPClapboard) && Cost.Lines.Num() == 1
						&& Cost.Lines[0].FromStorage == FromCrate && Cost.Lines[0].FromPersonal == FromMe,
						FString::Printf(TEXT("shown base %d / you %d, taken base %d / you %d"), Cost.Lines.Num() ? Cost.Lines[0].FromStorage : -1, Cost.Lines.Num() ? Cost.Lines[0].FromPersonal : -1, FromCrate, FromMe));
					IPShot(TEXT("finish-mode"));
				}
				else
				{
					Build->Primary();
				}
				UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: finish %d: target %d (%s), aim %s"), I, Build->GetView().FinishTarget, *Build->GetView().Finish.ToString(),
					*(Build->GetView().AimPoint - IPAnchor).ToCompactString());
			}));
		}
		S.Add(IPDo(TEXT("finished"), []
		{
			IPCount(TEXT("tenFinishes"), TEXT("finish mode, choose clapboard once (remembered per form), then aim and click per wall"));
			int32 Finished = 0;
			for (const int32 Id : IP->Walls)
			{
				const FGLStructurePartRuntime* Part = IPStructures()->FindPlayerPiece(Id);
				Finished += Part && Part->Piece.Layers.Contains(IPClapboard) ? 1 : 0;
			}
			IPNote(TEXT("ten walls finished in the chosen finish"), Finished == 10, FString::Printf(TEXT("%d of 10 in clapboard"), Finished));
			IPBuild()->ToggleFinishMode();
		}));
		// The window wall on the row's east end.
		S.Add(IPDo(TEXT("choose window wall"), [] { IPChoose(IPWindow); IPBuild()->RotateQuarter(1); IPStand(1200, -300); }));
		S.Add(IPAimThen(TEXT("window wall"), [] { return IPAt(885, 0, 30); }, []
		{
			const int32 Id = IPPlace(TEXT("window wall"));
			IP->Ids.Add(TEXT("window"), Id);
			FHitResult Hit;
			const bool bThrough = !IP->World->LineTraceSingleByChannel(Hit, IPAt(700, 0, 180), IPAt(1100, 0, 180), ECC_Visibility) || IPBuilding()->PieceIdAt(Hit) != Id;
			const bool bSill = IP->World->LineTraceSingleByChannel(Hit, IPAt(1100, 0, 80), IPAt(700, 0, 80), ECC_Visibility) && IPBuilding()->PieceIdAt(Hit) == Id;
			IPNote(TEXT("a window wall: placed like a wall, with a real opening"), Id && bThrough && bSill, FString::Printf(TEXT("through the opening %s, below the sill %s"),
				bThrough ? TEXT("clear") : TEXT("blocked"), bSill ? TEXT("hits the wall") : TEXT("misses")));
		}));
		// The upper floor on floor 0's walls, and a stair up to it from the west.
		S.Add(IPDo(TEXT("choose upper floor"), [] { IPChoose(IPUpper); IPBuild()->RotateQuarter(-1); IPStand(0, -700); }));
		S.Add(IPAimThen(TEXT("upper floor"), [] { return IPAt(0, -110, 265); }, [] // the south wall's top, from outside: the floor goes over the room
		{
			const int32 Id = IPPlace(TEXT("upper floor"));
			IP->Ids.Add(TEXT("upper"), Id);
			const FGLStructurePartRuntime* Part = Id ? IPStructures()->FindPlayerPiece(Id) : nullptr;
			IPNote(TEXT("an upper floor rests on the walls below"), Part && Part->Piece.Location.Equals(IPAt(0, 0, 280), 2.0) && IPBuilding()->Support().FindRef(Id) > 0.0,
				Part ? Part->Piece.Location.ToCompactString() : FString(TEXT("not placed")));
		}));
		S.Add(IPDo(TEXT("choose stair"), [] { IPChoose(IPStair); IPBuild()->RotateQuarter(-1); IPStand(-350, -500); }));
		S.Add(IPAimThen(TEXT("stair"), [] { return IPAt(-100, 0, 290); }, []
		{
			IPBuild()->RefreshView();
			const FGLBuildView View = IPBuild()->GetView();
			const int32 Id = IPPlace(TEXT("stair"));
			IP->Ids.Add(TEXT("stair"), Id);
			const FGLStructurePartRuntime* Part = Id ? IPStructures()->FindPlayerPiece(Id) : nullptr;
			IPNote(TEXT("a stair snapped to the upper floor's edge, standing on the floors"), Part && View.Snap.bSnapped && View.Snap.TargetPieceId == IP->Ids[TEXT("upper")]
				&& Part->Piece.Location.Equals(IPAt(-300, 0, 30), 2.0) && IPBuilding()->Support().FindRef(Id) > 0.0,
				Part ? FString::Printf(TEXT("at %s, yaw %.1f"), *Part->Piece.Location.ToCompactString(), GLStructureRules::YawDegrees(Part->Piece.YawStep)) : FString(TEXT("not placed")));
			IPShot(TEXT("stair-and-upper-floor"));
		}));
		// Zenny climbs it (physics, not a teleport) and navigation finds the way up.
		S.Add(IPDo(TEXT("to the stair's foot"), []
		{
			IPBuild()->ToggleBuild();
			AGLCharacter* Zenny = IPZenny();
			Zenny->SetActorLocation(IPAt(-620, 0, 30 + Zenny->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5), false, nullptr, ETeleportType::TeleportPhysics);
			Zenny->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			IPController()->SetControlRotation(FRotator(0, 0, 0));
			IP->MaxClimbZ = -1e9;
		}));
		S.Add({ TEXT("climb"), [](double Since)
		{
			AGLCharacter* Zenny = IPZenny();
			Zenny->AddMovementInput(FVector(1, 0, 0), 1.f);
			IP->MaxClimbZ = FMath::Max(IP->MaxClimbZ, Zenny->GetActorLocation().Z);
			return Zenny->GetActorLocation().X > IPAnchor.X - 20.0 || Since > 8.0;
		} });
		S.Add({ TEXT("arrived"), [](double Since)
		{
			if (Since < 0.5)
			{
				return false;
			}
			AGLCharacter* Zenny = IPZenny();
			const double Feet = Zenny->GetActorLocation().Z - Zenny->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - IPAnchor.Z;
			IPNote(TEXT("Zenny walks up the stair onto the upper floor (physics, no teleport)"), Feet > 280.0 && Zenny->GetActorLocation().X > IPAnchor.X - 100.0,
				FString::Printf(TEXT("feet at %.0f cm above the pad, x %.0f"), Feet, Zenny->GetActorLocation().X - IPAnchor.X));
			IPShot(TEXT("zenny-upstairs"));
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(IP->World.Get());
			UNavigationPath* Path = Nav ? Nav->FindPathToLocationSynchronously(IP->World.Get(), IPAt(-900, -300, 10), IPAt(20, 0, 310)) : nullptr;
			IPNote(TEXT("navigation finds a path up the stair"), Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Last().Z > IPAnchor.Z + 260.0,
				Path && Path->IsValid() ? FString::Printf(TEXT("%d points, ends at %.0f cm"), Path->PathPoints.Num(), Path->PathPoints.Last().Z - IPAnchor.Z) : FString(TEXT("no path")));
			return true;
		} });
		S.Add(IPWait(TEXT("upstairs frame"), 0.5)); // the screenshot is taken at the end of a later frame: keep still until then
		S.Add(IPDo(TEXT("back down"), [] { IPStand(0, -600); IPBuild()->ToggleBuild(); IPBuild()->ToggleRemoveMode(); }));
		// Removal on a second upper floor (over the room at x 400, no stair: a stair also holds what it reaches): its north
		// wall is redundant (one click); its south wall then brings it down (hold).
		S.Add(IPDo(TEXT("second upper floor"), [] { IPBuild()->ToggleRemoveMode(); IPChoose(IPUpper); IPStand(400, -700); }));
		S.Add(IPAimThen(TEXT("place the second upper floor"), [] { return IPAt(400, -110, 265); }, []
		{
			const int32 Id = IPPlace(TEXT("second upper floor"));
			IP->Ids.Add(TEXT("upper2"), Id);
			const FGLStructurePartRuntime* Part = Id ? IPStructures()->FindPlayerPiece(Id) : nullptr;
			IPNote(TEXT("a second upper floor over the room at x 400"), Part && Part->Piece.Location.Equals(IPAt(400, 0, 280), 2.0), Part ? (Part->Piece.Location - IPAnchor).ToCompactString() : FString(TEXT("not placed")));
			IPBuild()->ToggleRemoveMode();
		}));
		S.Add(IPDo(TEXT("to the north side"), [] { IPStand(400, 600); }));
		S.Add(IPAimThen(TEXT("remove the redundant support"), [] { return IPAt(400, 110, 150); }, []
		{
			UGLBuildModeComponent* Build = IPBuild();
			Build->RefreshView();
			const FGLBuildView View = Build->GetView();
			Build->Primary();
			IPNote(TEXT("a redundant support: nothing else falls, removed with one click"), View.RemoveTarget == IP->Ids[TEXT("wall400n")] && View.Predicted.Num() == 0 && !View.bNeedsConfirm
				&& !IPStructures()->FindPlayerPiece(View.RemoveTarget), FString::Printf(TEXT("target %d, predicted %d"), View.RemoveTarget, View.Predicted.Num()));
		}));
		S.Add(IPDo(TEXT("to the south side"), [] { IPStand(400, -600); }));
		S.Add(IPAimThen(TEXT("aim at the last support"), [] { return IPAt(400, -110, 150); }, []
		{
			UGLBuildModeComponent* Build = IPBuild();
			Build->RefreshView();
			IP->HoldTarget = Build->GetView().RemoveTarget;
			IP->HoldPrediction = Build->GetView().Predicted;
			IPNote(TEXT("the removal preview: the upper floor will come down (canonical prediction)"), IP->HoldTarget == IP->Ids[TEXT("wall400s")]
				&& IP->HoldPrediction == IPBuilding()->PreviewRemoval(IP->HoldTarget) && IP->HoldPrediction.Contains(IP->Ids[TEXT("upper2")]) && Build->GetView().bNeedsConfirm,
				FString::Printf(TEXT("predicted [%s]"), *FString::JoinBy(IP->HoldPrediction, TEXT(","), [](int32 I) { return FString::FromInt(I); })));
			IPShot(TEXT("removal-prediction"));
		}));
		S.Add(IPDo(TEXT("press and hold"), [] { IPBuild()->Primary(); })); // starts the hold
		S.Add({ TEXT("hold to confirm"), [](double Since)
		{
			if (Since < 0.15)
			{
				const bool bStill = IPStructures()->FindPlayerPiece(IP->HoldTarget) != nullptr;
				if (!bStill)
				{
					IPNote(TEXT("a collapse needs the hold"), false, TEXT("removed before the hold completed"));
				}
				return false;
			}
			if (Since < 0.8)
			{
				return false;
			}
			IPBuild()->PrimaryReleased();
			bool bAll = true;
			for (const int32 Id : IP->HoldPrediction)
			{
				const FGLStructurePartRuntime* Part = IPStructures()->FindPlayerPiece(Id);
				bAll &= Part && Part->State == EGLStructurePartState::Debris;
			}
			IPNote(TEXT("held: removed, and exactly the predicted pieces collapsed"), !IPStructures()->FindPlayerPiece(IP->HoldTarget) && bAll,
				FString::Printf(TEXT("%d predicted, all debris %s"), IP->HoldPrediction.Num(), bAll ? TEXT("yes") : TEXT("no")));
			return true;
		} });
		// Salvage: careful and smash previews are the recovery.
		for (const bool bSmash : { false, true })
		{
			S.Add(IPDo(TEXT("stand for salvage"), [bSmash] { IPStand(bSmash ? 600 : 800, -450); }));
			S.Add(IPAimThen(bSmash ? TEXT("smash a wall") : TEXT("dismantle a wall"), [bSmash] { return IPAt(bSmash ? 600 : 800, -110, 150); }, [bSmash]
			{
				UGLBuildModeComponent* Build = IPBuild();
				if (bSmash != (Build->GetView().Path == EGLSalvagePath::Destructive))
				{
					Build->TogglePath();
				}
				Build->RefreshView();
				const FGLBuildView View = Build->GetView();
				const TMap<FName, int32> Before = IPCarried();
				Build->Primary();
				const TMap<FName, int32> Got = IPDelta(Before, IPCarried());
				IPNote(bSmash ? TEXT("smash: the preview is the recovery") : TEXT("careful: the preview is the recovery"), View.RemoveTarget != 0
					&& GLBuildText::Items(Got) == GLBuildText::Items(bSmash ? View.YieldDestructive : View.YieldCareful),
					FString::Printf(TEXT("shown %s, got %s"), *GLBuildText::Items(bSmash ? View.YieldDestructive : View.YieldCareful), *GLBuildText::Items(Got)));
			}));
		}
		// The build camera: pulled back, and still aiming exactly where it looks.
		S.Add(IPDo(TEXT("build camera"), []
		{
			IPBuild()->ToggleRemoveMode();
			IPBuild()->SelectPiece(IPPost);
			IPStand(-200, -650);
			IPBuild()->BuildCamera(true);
		}));
		S.Add(IPWait(TEXT("camera pulls back"), 1.0));
		S.Add(IPAimThen(TEXT("place with the build camera"), [] { return IPAt(-600, -350, 0); }, []
		{
			APlayerController* PC = IPController();
			const double Distance = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), IPZenny()->GetActorLocation());
			const int32 Before = IP->AimMismatches + IP->CommitMismatches;
			const int32 Id = IPPlace(TEXT("build camera"));
			IPNote(TEXT("build camera: pulled back, and what it aims at is what is placed"), Distance > 800.0 && Id && IP->AimMismatches + IP->CommitMismatches == Before,
				FString::Printf(TEXT("camera %.0f cm from Zenny"), Distance));
			IPShot(TEXT("build-camera"));
			IPBuild()->BuildCamera(false);
		}));
		// Ten independent pieces dismantled carefully (MEASURED): posts in a row south of the house.
		S.Add(IPDo(TEXT("posts to dismantle"), [] { IPStand(0, -700); }));
		for (int32 I = 0; I < 10; ++I)
		{
			S.Add(IPAimThen(FString::Printf(TEXT("post %d"), I), [I] { return IPAt(-450 + 100.0 * I, -350, 0); }, [] { IP->Posts.Add(IPPlace(TEXT("post"))); }));
		}
		S.Add(IPDo(TEXT("remove mode (measured from here)"), []
		{
			IP->CountBase = IPBuild()->GetIntentTotal();
			IPBuild()->ToggleRemoveMode();
			if (IPBuild()->GetView().Path != EGLSalvagePath::Careful)
			{
				IPBuild()->TogglePath();
			}
		}));
		for (int32 I = 0; I < 10; ++I)
		{
			S.Add(IPAimThen(FString::Printf(TEXT("dismantle post %d"), I), [I] { return IPAt(-450 + 100.0 * I, -350, 100); }, [] { IPBuild()->Primary(); }));
		}
		S.Add(IPDo(TEXT("posts dismantled"), []
		{
			IPCount(TEXT("tenCarefulDismantles"), TEXT("remove mode once, then aim and click per piece (no confirmation: nothing else falls)"));
			int32 Left = 0;
			for (const int32 Id : IP->Posts) { Left += IPStructures()->FindPlayerPiece(Id) ? 1 : 0; }
			IPNote(TEXT("ten independent pieces dismantled with one click each"), !IP->Posts.Contains(0) && IP->Posts.Num() == 10 && Left == 0, FString::Printf(TEXT("%d left"), Left));
			IPBuild()->ToggleRemoveMode();
		}));
		// The browser on screen, then the gates' totals, the profile, and a save.
		S.Add(IPDo(TEXT("browser"), [] { IPStand(400, -700); IPBuild()->ToggleBrowser(); }));
		S.Add(IPWait(TEXT("browser frame"), 0.3));
		S.Add(IPDo(TEXT("browser shot"), [] { IPShot(TEXT("browser")); }));
		S.Add(IPWait(TEXT("shot"), 0.3));
		S.Add(IPDo(TEXT("gates and save"), []
		{
			IPBuild()->ToggleBrowser();
			IPNote(TEXT("CAMERA AIM == GAMEPLAY AIM at every commit"), IP->AimChecks > 30 && IP->AimMismatches == 0, FString::Printf(TEXT("%d checks, %d mismatches"), IP->AimChecks, IP->AimMismatches));
			IPNote(TEXT("every commit is the piece the view showed"), IP->CommitChecks > 30 && IP->CommitMismatches == 0, FString::Printf(TEXT("%d commits, %d mismatches"), IP->CommitChecks, IP->CommitMismatches));
			IP->Out->SetStringField(TEXT("profile"), IPBuild()->GetFavorites()[0].ToString());
			IP->World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(IP->World->GetSubsystem<UGLSaveSubsystem>()->AutosaveSlot);
		}));
	}

	void IPRestartSteps()
	{
		TArray<FIPStep>& S = IP->Steps;
		S.Add({ TEXT("home cell ready"), [](double) { if (!IPCellReady()) { return false; } IPStand(400, -700); return true; } });
		S.Add(IPWait(TEXT("presented"), 1.0));
		S.Add(IPDo(TEXT("back exactly"), []
		{
			FString Before;
			const bool bRead = IPReadPrevious(Before);
			IPBuild()->SetProfilePath(IPDir() / TEXT("build-profile.json"));
			IPNote(TEXT("restart: the house is back exactly (stair, window wall, finishes, debris)"), bRead && IPFingerprint() == Before,
				FString::Printf(TEXT("%d pieces"), IPBuilding()->GetPieces().Num()));
			IPNote(TEXT("restart: the favourite persists (playtest profile)"), IPBuild()->GetFavorites()[0] == IPFoundation, IPBuild()->GetFavorites()[0].ToString());
			IP->Out->SetStringField(TEXT("fingerprintBefore"), IPFingerprint());
			IPZenny()->SetActorLocation(IPFarAway + FVector(0, 0, 300), false, nullptr, ETeleportType::TeleportPhysics);
		}));
		S.Add({ TEXT("streamed away"), [](double Since) { return !IP->World->GetSubsystem<UGLGridSubsystem>()->IsLoaded(IPOrigin) && Since > 1.0; } });
		S.Add(IPDo(TEXT("back"), [] { IPZenny()->SetActorLocation(IPAt(400, -700, 300), false, nullptr, ETeleportType::TeleportPhysics); }));
		S.Add({ TEXT("streamed back"), [](double Since)
		{
			if (!IPCellReady() || Since < 1.0)
			{
				return false;
			}
			IPStand(400, -700);
			IPNote(TEXT("streamed away and back: still exactly the house"), IPFingerprint() == IP->Out->GetStringField(TEXT("fingerprintBefore")), TEXT(""));
			IPBuild()->ToggleBuild();
			IPChoose(IPWall);
			return true;
		} });
		S.Add(IPAimThen(TEXT("keep building"), [] { return IPAt(800, -85, 30); }, []
		{
			const int32 Id = IPPlace(TEXT("restart wall"));
			IPNote(TEXT("and building continues (a wall where one was smashed)"), Id != 0 && IP->CommitMismatches == 0 && IP->AimMismatches == 0, FString::Printf(TEXT("wall %d"), Id));
			IPShot(TEXT("restart-continued"));
		}));
		S.Add(IPWait(TEXT("shot"), 0.5));
	}

	FAutoConsoleCommandWithWorldAndArgs IntentProofCommand(
		TEXT("gl.Building.IntentProof"),
		TEXT("DEV ONLY (P12): the public-intent build-mode proof in the real game: build | restart."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			IP = MakeShared<FIPRun>();
			IP->Mode = Args.Num() ? Args[0] : TEXT("build");
			IP->World = World;
			IP->Started = IP->StepAt = World->GetTimeSeconds();
			IP->Mode == TEXT("restart") ? IPRestartSteps() : IPBuildSteps();
			IP->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&IPTick));
			UE_LOG(LogGridlands, Log, TEXT("gl.Building.IntentProof: %s started (%d steps)"), *IP->Mode, IP->Steps.Num());
		}));
}

#endif // !UE_BUILD_SHIPPING
