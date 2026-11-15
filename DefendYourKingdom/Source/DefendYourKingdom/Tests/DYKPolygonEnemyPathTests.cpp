#include "../Battle/DYKEnemyActor.h"
#include "../Battle/DYKEnemyRoute.h"
#include "../Battle/DYKPolygonGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	class FAuthoredPolygonPathCommand : public IAutomationLatentCommand
	{
	public:
		FAuthoredPolygonPathCommand(FAutomationTestBase* InTest, ADYKPolygonGameMode* InMode)
			: Test(InTest), Mode(InMode), Started(InMode->GetWorld()->GetTimeSeconds()), Deadline(FPlatformTime::Seconds() + 100) {}
		virtual bool Update() override
		{
			if (!Mode.IsValid() || FPlatformTime::Seconds() > Deadline)
			{
				Test->AddError(TEXT("Unmodified polygon route did not finish in the real game world.")); return true;
			}
			auto* World = Mode->GetWorld();
			auto& Battle = Mode->GetBattleSession();
			const double Elapsed = World->GetTimeSeconds() - Started;
			TArray<ADYKEnemyActor*> Enemies;
			for (TActorIterator<ADYKEnemyActor> It(World); It; ++It) Enemies.Add(*It);
			if (Stage == 0 && Elapsed > 20)
			{
				Test->TestEqual(TEXT("Authored map emits exactly three actors"), Enemies.Num(), 3);
				Test->TestEqual(TEXT("Authored map closes emission but keeps live enemies"), Battle.GetPendingGroupCount(), 0);
				Test->TestEqual(TEXT("No objective damage during the full map traverse"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				for (auto* Enemy : Enemies)
				{
					Test->TestFalse(TEXT("Authored route has not prematurely teleported to contact"), Enemy->HasReachedObjective());
					Test->TestTrue(TEXT("Both authored bends were traversed to the final road"), FMath::IsNearlyEqual(Enemy->GetActorLocation().Y, 700, 1));
				}
				if (FParse::Param(FCommandLine::Get(), TEXT("DYKM107Capture")))
				{
					auto* Camera = World->SpawnActor<ACameraActor>(FVector(800, 700, 5000), FRotator(-90, 0, 0));
					World->GetFirstPlayerController()->SetViewTarget(Camera);
					// Capture on a subsequent frame, after the view target is applied.
					CaptureAt = Elapsed + 1;
				}
				Stage = 1;
			}
			if (CaptureAt > 0 && Elapsed > CaptureAt)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("M107/authored-route.png")), false, false);
				CaptureAt = 0;
			}
			if (Stage == 1 && Elapsed > 37)
			{
				Test->TestEqual(TEXT("All three map actors survive contact"), Enemies.Num(), 3);
				for (auto* Enemy : Enemies)
				{
					Test->TestTrue(TEXT("Full authored route reaches its logical objective"), Enemy->HasReachedObjective());
					Test->TestTrue(TEXT("Contact is before the decorative castle mesh"), Enemy->GetActorLocation().Equals(FVector(3400,700,80), 1));
				}
				Test->TestTrue(TEXT("Authored map produces repeated actual hits"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")) <= 94);
				Test->TestEqual(TEXT("Contact never removes registrations"), Battle.GetActiveEnemyCount(), 3);
				for (auto* Enemy : Enemies) Test->TestTrue(TEXT("Map enemy death accepted once"), Enemy->ApplyDamage(10));
				Stage = 2;
			}
			else if (Stage == 2 && Battle.GetPhase() == EDYKBattlePhase::InterwavePreparation)
			{
				Test->TestEqual(TEXT("Actual map wave clears after all three deaths"), Battle.GetActiveEnemyCount(), 0);
				if (FParse::Param(FCommandLine::Get(), TEXT("DYKM107Capture")))
					Test->TestTrue(TEXT("Rendered gameplay screenshot exists"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*(FPaths::ProjectSavedDir() / TEXT("M107/authored-route.png"))));
				return true;
			}
			return false;
		}
	private:
		FAutomationTestBase* Test;
		TWeakObjectPtr<ADYKPolygonGameMode> Mode;
		double Started, Deadline, CaptureAt = 0;
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKAuthoredPolygonPathTest, "DYK.M107.RuntimeAuthoredPolygonPath",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKAuthoredPolygonPathTest::RunTest(const FString& Parameters)
{
	ADYKPolygonGameMode* Mode = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World()) Mode = Cast<ADYKPolygonGameMode>(Context.World()->GetAuthGameMode());
	if (!TestNotNull(TEXT("Full path test requires its own real polygon game world"), Mode)) return false;
	auto& Battle = Mode->GetBattleSession();
	if (!TestTrue(TEXT("Full path begins with a fresh attempt"), Battle.GetPhase() == EDYKBattlePhase::InitialPreparation)) return false;
	Battle.AddDefense(FGuid::NewGuid()); // Logical start prerequisite; placement belongs to M1.08.
	if (!TestTrue(TEXT("Full authored map wave starts"), Battle.StartWave(Battle.GetPreparationToken(), Mode->GetWorld()->GetTimeSeconds()))) return false;
	AddCommand(new FAuthoredPolygonPathCommand(this, Mode));
	return true;
}

#endif
