// DEV ONLY: heightfield-collision spike, the operator's visual review of the render-diagonal change
// (ADR-0035). The same terrain, edits, cameras, lighting and settings are captured once per process; the
// process's -GLTerrainCollision mode decides the render split (0: canonical, 1: heightfield's diagonal).
// Frame-counted (run with -benchmark -fps=30) so two runs see the same simulated time at every shot.
// Captures only: it changes nothing visual.
// Not in shipping builds.

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Character/GLCharacter.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Presentation/GLStyleSubsystem.h"
#include "Terrain/GLHeightfield.h"
#include "Terrain/GLTerrainCollision.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLGridSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace GLTerrainReview
{
	const FName RLots(TEXT("cell.outer.diner_lots"));
	// A chunk corner of the lots (field origin (51200, -51200) cm, 64 m chunks): seams run through it on both axes.
	const FVector2D RCorner(51200.0 + 13 * 6400.0, -51200.0 + 6 * 6400.0);

	struct FView
	{
		FString Name;
		FVector2D Target;
		FVector2D Dir;   // from target toward the eye (unit)
		double Distance; // cm, horizontal
		double EyeUp;    // cm above the ground under the eye
		double TargetUp; // cm above the ground at the target
	};

	TWeakObjectPtr<ACameraActor> Camera;

	double Ground(UWorld* World, const FVector2D& At) { return World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(At); }

	void Frame(UWorld* World, const FView& V)
	{
		const FVector2D EyeXY = V.Target + V.Dir * V.Distance;
		const FVector From(EyeXY, Ground(World, EyeXY) + V.EyeUp);
		const FVector To(V.Target, Ground(World, V.Target) + V.TargetUp);
		if (!Camera.IsValid())
		{
			Camera = World->SpawnActor<ACameraActor>();
		}
		Camera->SetActorLocationAndRotation(From, (To - From).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(60.0f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTarget(Camera.Get());
		}
	}

	void Edit(UGLTerrainSubsystem* Terrain, EGLTerrainOp Op, const FVector2D& At, double Radius, double Amount)
	{
		FGLTerrainEdit E;
		E.Op = Op;
		E.Centre = At;
		E.RadiusCm = Radius;
		E.AmountCm = Amount;
		Terrain->ApplyEdit(E);
	}

	/** The steepest natural ground in the lots' playable middle (untouched relief), away from the edited scenes. */
	FVector2D SteepestNatural(const FGLHeightfield& F, double& OutDegrees)
	{
		double Best = -1.0;
		FIntPoint BestAt(512, 512);
		for (int32 Y = 120; Y < 900; Y += 2)
		{
			for (int32 X = 120; X < 900; X += 2)
			{
				const FVector2D W = F.VertexLocation(X, Y);
				if (FVector2D::Distance(W, RCorner) < 6000.0)
				{
					continue; // not inside the edited scenes
				}
				const double GX = (F.VertexHeight(X + 1, Y) - F.VertexHeight(X - 1, Y)) / (2.0 * F.GetSpacing());
				const double GY = (F.VertexHeight(X, Y + 1) - F.VertexHeight(X, Y - 1)) / (2.0 * F.GetSpacing());
				const double G = FMath::Sqrt(GX * GX + GY * GY);
				if (G > Best)
				{
					Best = G;
					BestAt = FIntPoint(X, Y);
				}
			}
		}
		OutDegrees = FMath::RadiansToDegrees(FMath::Atan(Best));
		return F.VertexLocation(BestAt.X, BestAt.Y);
	}

	void Review(const TArray<FString>& Args, UWorld* World)
	{
		TSharedRef<int32> FrameNo = MakeShared<int32>(0);
		TSharedRef<TArray<FView>> Shots = MakeShared<TArray<FView>>();
		TWeakObjectPtr<UWorld> Weak(World);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, FrameNo, Shots](float)
		{
			UWorld* W = Weak.Get();
			if (!W)
			{
				return false;
			}
			AGLCharacter* Z = Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(W, 0));
			if (!Z)
			{
				return true; // not possessed yet: frames are counted from the first frame with Zenny
			}
			const int32 F = (*FrameNo)++;
			constexpr int32 Settle = 240, PerView = 120, ShotAt = 90;
			if (F == 0)
			{
				// Zenny (the streaming invoker) stands among the scenes, hidden with Pehlichi: the review judges terrain.
				UGLGridSubsystem* Grid = W->GetSubsystem<UGLGridSubsystem>();
				UGLTerrainSubsystem* Terrain = W->GetSubsystem<UGLTerrainSubsystem>();
				Z->SetActorLocation(FVector(RCorner, 2000.0));
				Grid->Advance(FVector(RCorner, 0.0));
				Grid->FlushAll();
				Z->SetActorLocation(FVector(RCorner, Ground(W, RCorner) + 100.0), false, nullptr, ETeleportType::TeleportPhysics);
				Z->SetActorHiddenInGame(true);
				if (AGLPehlichi* Peh = Z->GetPehlichi())
				{
					Peh->GetPositioning()->Follow(nullptr);
					Peh->SetActorLocation(FVector(RCorner + FVector2D(-100.0, 0.0), Ground(W, RCorner) + 170.0));
					Peh->SetActorHiddenInGame(true);
				}
				if (APlayerController* PC = W->GetFirstPlayerController(); PC && PC->MyHUD)
				{
					PC->MyHUD->bShowHUD = false;
				}
				W->GetSubsystem<UGLStyleSubsystem>()->ApplyPreset(TEXT("day"));

				// The scenes (identical edits in every run; the seeded stream makes the irregular one repeatable).
				const FVector2D Rolling = RCorner + FVector2D(-3000.0, -3200.0);
				const FVector2D Pit = RCorner + FVector2D(2600.0, -3000.0);
				const FVector2D Mound = RCorner + FVector2D(-3000.0, 2600.0);
				const FVector2D Rough = RCorner + FVector2D(2600.0, 2600.0);
				const FVector2D Seam = RCorner + FVector2D(0.0, 1300.0); // on the X seam (x = RCorner.X)
				for (int32 I = 0; I < 3; ++I)
				{
					Edit(Terrain, EGLTerrainOp::Dig, Pit, 350.0, 160.0);
				}
				Edit(Terrain, EGLTerrainOp::Raise, Mound, 420.0, 300.0);
				Edit(Terrain, EGLTerrainOp::Raise, Mound + FVector2D(180.0, -120.0), 200.0, 80.0);
				FRandomStream Rng(0x6ea1);
				for (int32 I = 0; I < 40; ++I)
				{
					const FVector2D At = Rough + FVector2D(Rng.FRandRange(-900.0, 900.0), Rng.FRandRange(-900.0, 900.0));
					Edit(Terrain, Rng.RandBool() ? EGLTerrainOp::Dig : EGLTerrainOp::Raise, At, Rng.FRandRange(80.0, 350.0), Rng.FRandRange(40.0, 300.0));
				}
				Edit(Terrain, EGLTerrainOp::Raise, Seam, 300.0, 220.0);
				Edit(Terrain, EGLTerrainOp::Dig, Seam + FVector2D(60.0, 380.0), 200.0, 180.0);
				double Degrees = 0.0;
				const FVector2D Steep = SteepestNatural(*Terrain->FieldOf(RLots), Degrees);
				UE_LOG(LogGridlands, Log, TEXT("gl.Terrain.DiagonalReview: collision mode %s, render split %s; steepest natural slope %.1f deg at %s"),
					GLTerrainCollision::ModeName(GLTerrainCollision::GetMode()), GLTerrainCollision::RenderSplitsMainDiagonal() ? TEXT("(x,y)-(x+1,y+1)") : TEXT("(x+1,y)-(x,y+1) canonical"), Degrees, *Steep.ToString());

				const FVector2D D1 = FVector2D(-0.8, -0.6), D2 = FVector2D(0.6, -0.8), Across = FVector2D(-0.95, 0.31);
				auto Pair = [&Shots](const FString& Name, const FVector2D& T, const FVector2D& Dir, double TargetUp)
				{
					Shots->Add({ Name + TEXT("-medium"), T, Dir, 1600.0, 650.0, TargetUp });
					Shots->Add({ Name + TEXT("-close"), T, Dir, 520.0, 240.0, TargetUp });
				};
				Pair(TEXT("01-rolling"), Rolling, D1, 0.0);
				Pair(TEXT("02-steep-natural-slope"), Steep, D2, 0.0);
				Pair(TEXT("03-dig"), Pit, D1, -150.0);
				// Raised ground: the close view looks down onto the flank facing the camera (aimed at the peak, it saw sky).
				Shots->Add({ TEXT("04-raise-mound-medium"), Mound, D2, 1600.0, 650.0, 150.0 });
				Shots->Add({ TEXT("04-raise-mound-close"), Mound + D2 * 220.0, D2, 520.0, 420.0, 0.0 });
				Pair(TEXT("05-aggressive-terraform"), Rough, D1, 0.0);
				Shots->Add({ TEXT("06-chunk-seam-medium"), Seam, Across, 1600.0, 650.0, 60.0 });
				Shots->Add({ TEXT("06-chunk-seam-close"), Seam + Across * 200.0, Across, 560.0, 400.0, 0.0 });
				return true;
			}
			const int32 Index = (F - Settle) / PerView, Phase = (F - Settle) % PerView;
			if (F < Settle)
			{
				return true;
			}
			if (Index >= Shots->Num())
			{
				GEngine->DeferredCommands.Add(TEXT("quit"));
				return false;
			}
			if (Phase == 0)
			{
				Frame(W, (*Shots)[Index]);
			}
			else if (Phase == ShotAt)
			{
				UE_LOG(LogGridlands, Log, TEXT("gl.Terrain.DiagonalShot %d %s"), Index + 1, *(*Shots)[Index].Name);
				GEngine->DeferredCommands.Add(TEXT("HighResShot 1920x1080"));
			}
			return true;
		}));
	}

	FAutoConsoleCommandWithWorldAndArgs ReviewCommand(TEXT("gl.Terrain.DiagonalReview"),
		TEXT("DEV ONLY (heightfield spike): captures the render-diagonal review scenes with fixed cameras and quits. Run with -benchmark -fps=30."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Review));
}

#endif
