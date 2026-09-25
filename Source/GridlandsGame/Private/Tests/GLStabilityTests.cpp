#include "Events/GLEventSubsystem.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "Inventory/GLInventoryComponent.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Tests/GLTestUtils.h"
#include "World/GLPlacementSubsystem.h"
#include "World/GLStabilitySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStaticRecedes, "Gridlands.Game.Stability.StaticRecedesWhenPehlichiRepairs", GLTestUtils::Flags)
bool FGLStaticRecedes::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLStabilityTestWorld"));
	UWorld* World = Test.World;
	World->GetSubsystem<UGLPlacementSubsystem>()->SpawnCell(TEXT("cell.home.origin"));
	UGLGlitchSubsystem* Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
	UGLStabilitySubsystem* Stability = World->GetSubsystem<UGLStabilitySubsystem>();
	TArray<double> Composures;
	World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event.World.Stabilized")),
		FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent& E) { Composures.Add(E.Numbers.FindRef(TEXT("composure"))); }));

	AGLGlitch* LampGlitch = Glitches->FindByPlacement(TEXT("placement.origin.glitch_flicker_lamp"));
	const FVector At = LampGlitch->GetActorLocation();
	const double Before = Stability->InterferenceAt(At);
	TestTrue(FString::Printf(TEXT("an unrepaired glitch corrupts its surroundings (%.2f)"), Before), Before >= 0.2);
	TestEqual(TEXT("NICE starts composed"), Stability->NiceComposure(), 1.0);
	// Canonical 1 km cells (ADR-0027) are playable edge to edge; beyond the known Grid, the next
	// band's interference applies.
	TestTrue(TEXT("beyond the known Grid, the next band's interference applies"),
		Stability->BaselineAt(FVector(0, -100000, 0)) > Stability->BaselineAt(FVector(0, 0, 0)));

	// The real loop: Zenny clears the blocker, Pehlichi scans and repairs.
	AActor* Zenny = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Zenny);
	Zenny->SetRootComponent(Root);
	Root->RegisterComponent();
	NewObject<UGLInventoryComponent>(Zenny)->RegisterComponent();
	Zenny->SetActorLocation(At + FVector(0, -300, 0));
	Glitches->SetCommander(Zenny);
	AGLPehlichi* Pehlichi = World->SpawnActor<AGLPehlichi>(At + FVector(0, -200, 0), FRotator::ZeroRotator);
	Pehlichi->GetScan()->Scan();
	AGLSalvageNode* Blocker = World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(TEXT("placement.origin.junk_pile_01"));
	while (!Blocker->GetSalvageable()->IsSalvaged())
	{
		Blocker->GetSalvageable()->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
	}
	Glitches->EvaluateRequirements();
	Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Repair"), Zenny);
	for (int32 Step = 0; Step < 200 && LampGlitch->GetGlitch()->GetState() != EGLGlitchState::Repaired; ++Step)
	{
		Glitches->EvaluateRequirements();
		Pehlichi->GetPositioning()->Advance(0.1f);
		Pehlichi->GetRepair()->Advance(0.1f);
	}
	const double After = Stability->InterferenceAt(At);
	TestEqual(TEXT("repaired"), LampGlitch->GetGlitch()->GetState(), EGLGlitchState::Repaired);
	TestTrue(FString::Printf(TEXT("Pehlichi fixed the world: interference %.2f -> %.2f"), Before, After), After < Before - 0.15);
	TestTrue(TEXT("the tier dropped"), Stability->TierAt(At) < GLStabilityModel::TierFor(Before));
	TestTrue(TEXT("NICE's composure fell"), Stability->NiceComposure() < 1.0);
	TestTrue(TEXT("Event.World.Stabilized carried the new composure"), Composures.Num() == 1 && Composures[0] < 1.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
