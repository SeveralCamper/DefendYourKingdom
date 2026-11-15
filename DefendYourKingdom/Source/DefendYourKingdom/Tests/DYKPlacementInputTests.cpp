#include "../Battle/DYKPlacementWidget.h"
#include "../Battle/DYKPolygonGameMode.h"
#include "../Battle/DYKUnitSlot.h"
#include "../Battle/DYKDefenderActor.h"
#include "../Battle/DYKEnemyActor.h"
#include "../Core/DYKGameInstance.h"
#include "../Core/Interaction/DYKSelectionPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/SWidget.h"

namespace
{
	void PressPlacementButton(FAutomationTestBase* Test, UButton* Button)
	{
		const auto Slate = Button->TakeWidget();
		const auto Geometry = FGeometry::MakeRoot(FVector2D(300, 45), FSlateLayoutTransform());
		const FKeyEvent Accept(EKeys::SpaceBar, FModifierKeysState(), 0, false, 0, 0);
		Test->TestTrue(TEXT("Slate button accepts key down"), Slate->OnKeyDown(Geometry, Accept).IsEventHandled());
		Test->TestTrue(TEXT("Slate key up routes into real UButton click"), Slate->OnKeyUp(Geometry, Accept).IsEventHandled());
	}

	void SelectPlacement(ADYKSelectionPlayerController* PC, ADYKUnitSlot* Placement)
	{
		FDYKWorldSelection Selection;
		Selection.TargetKind = EDYKSelectionTargetKind::CombatSlot;
		Selection.TargetActor = Placement;
		PC->OnWorldSelectionChanged().Broadcast(Selection);
	}

	class FPlacementTravelCommand : public IAutomationLatentCommand
	{
	public:
		FPlacementTravelCommand(FAutomationTestBase* InTest, ADYKPolygonGameMode* Mode, UDYKPlacementWidget* Widget)
			: Test(InTest), Instance(Cast<UDYKGameInstance>(Mode->GetGameInstance())), OldMode(Mode), OldWorld(Mode->GetWorld()),
			OldWidget(Widget), Deadline(FPlatformTime::Seconds() + 45), Started(Mode->GetWorld()->GetTimeSeconds()), Attempt(Mode->GetBattleSession().GetAttemptId()) {}
		virtual bool Update() override
		{
			if (!Instance.IsValid() || FPlatformTime::Seconds() > Deadline)
			{
				Test->AddError(TEXT("Placement input/travel did not complete in a real game world.")); return true;
			}
			auto* World = Instance->GetWorld();
			if (!World || !World->HasBegunPlay() || Instance->IsLevelTravelPending()) return false;
			if (Stage == 0)
			{
				if (!OldMode.IsValid()) { Test->AddError(TEXT("Initial combat world disappeared prematurely.")); return true; }
				auto* Mode = OldMode.Get();
				bool bRealCombat = false;
				for (TActorIterator<ADYKEnemyActor> It(World); It; ++It)
					if (It->GetCurrentTarget() && It->GetCurrentTarget()->IsAlive()) bRealCombat = true;
				if (!bRealCombat) return false;
				if (FParse::Param(FCommandLine::Get(), TEXT("DYKM108Capture")))
				{
					// Let Slate finish drawing the updated status/font atlas before visual QA.
					if (CaptureReadyAt == 0) CaptureReadyAt = World->GetTimeSeconds() + 1.2;
					if (World->GetTimeSeconds() < CaptureReadyAt) return false;
				}
				Test->TestTrue(TEXT("Authored slots actually intercept the authored map wave"), World->GetTimeSeconds() - Started < 10);
				Test->TestEqual(TEXT("Intercepted wave preserves castle before contact"), Mode->GetBattleSession().GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				for (TActorIterator<ADYKDefenderActor> It(World); It; ++It) OldDefenders.Add(*It);
				if (FParse::Param(FCommandLine::Get(), TEXT("DYKM108Capture")))
				{
					FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("M108/placement-combat.png")), true, false);
					CaptureFrame = GFrameCounter;
				}
				Stage = 1;
			}
			else if (Stage == 1 && GFrameCounter > CaptureFrame + 2)
			{
				if (FParse::Param(FCommandLine::Get(), TEXT("DYKM108Capture")))
					Test->TestTrue(TEXT("Actual rendered battle frame was written"), IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir() / TEXT("M108/placement-combat.png"))));
				FString Error;
				if (!Test->TestTrue(TEXT("Travel with live defenders and attack timers is accepted"), Instance->RequestLevelTravel(EDYKLocalLevel::City, Error))) return true;
				Stage = 2;
			}
			else if (Stage == 2 && World->GetOutermost()->GetName().Contains(TEXT("L_Citadel")))
			{
				CollectGarbage(RF_NoFlags);
				Test->TestFalse(TEXT("Old polygon mode is collected after live combat travel"), OldMode.IsValid());
				Test->TestFalse(TEXT("Old polygon world is collected"), OldWorld.IsValid());
				Test->TestFalse(TEXT("Placement widget does not retain the old world"), OldWidget.IsValid());
				for (const auto& Unit : OldDefenders) Test->TestFalse(TEXT("Defender actors with pending attacks are collected"), Unit.IsValid());
				FString Error;
				if (!Test->TestTrue(TEXT("Return to polygon is accepted"), Instance->RequestLevelTravel(EDYKLocalLevel::MissionPolygon, Error))) return true;
				Stage = 3;
			}
			else if (Stage == 3 && World->GetOutermost()->GetName().Contains(TEXT("L_MissionPolygon")))
			{
				auto* Mode = World->GetAuthGameMode<ADYKPolygonGameMode>();
				if (!Test->TestNotNull(TEXT("Reloaded polygon has battle mode"), Mode)) return true;
				Test->TestTrue(TEXT("Reloaded attempt has fresh identity"), Mode->GetBattleSession().GetAttemptId() != Attempt);
				Test->TestEqual(TEXT("No old purchases/supplies leak into new attempt"), Mode->GetBattleSession().GetSupplies(), 20);
				Test->TestFalse(TEXT("New attempt has no phantom defenses"), Mode->GetBattleSession().CanStartWave());
				for (TActorIterator<ADYKUnitSlot> It(World); It; ++It) Test->TestTrue(TEXT("Reloaded authored slots are empty"), It->GetOccupant() == nullptr);
				if (!FParse::Param(FCommandLine::Get(), TEXT("DYKM108Capture"))) return true;
				auto* Camera = World->SpawnActor<ACameraActor>(FVector(-2800,-1300,1700), FRotator(-90,0,0));
				World->GetFirstPlayerController()->SetViewTarget(Camera);
				CaptureReadyAt = World->GetTimeSeconds() + 1.2;
				Stage = 4;
			}
			else if (Stage == 4 && World->GetTimeSeconds() >= CaptureReadyAt)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("M108/placement-slots.png")), true, false);
				CaptureFrame = GFrameCounter;
				Stage = 5;
			}
			else if (Stage == 5 && GFrameCounter > CaptureFrame + 2)
			{
				Test->TestTrue(TEXT("Rendered empty slot highlights were written"), IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir() / TEXT("M108/placement-slots.png"))));
				return true;
			}
			return false;
		}
	private:
		FAutomationTestBase* Test;
		TWeakObjectPtr<UDYKGameInstance> Instance;
		TWeakObjectPtr<ADYKPolygonGameMode> OldMode;
		TWeakObjectPtr<UWorld> OldWorld;
		TWeakObjectPtr<UDYKPlacementWidget> OldWidget;
		TArray<TWeakObjectPtr<ADYKDefenderActor>> OldDefenders;
		double Deadline, Started, CaptureReadyAt = 0;
		FGuid Attempt;
		int32 Stage = 0;
		uint64 CaptureFrame = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKPlacementInputTest, "DYK.M108.RuntimePlacementSlateAndTravel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDYKPlacementInputTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!TestNotNull(TEXT("Placement UI test requires its own fresh -game polygon"), World)) return false;
	auto* Mode = World->GetAuthGameMode<ADYKPolygonGameMode>();
	auto* PC = Cast<ADYKSelectionPlayerController>(World->GetFirstPlayerController());
	if (!TestNotNull(TEXT("Actual battle mode"), Mode) || !TestNotNull(TEXT("Actual selection controller"), PC)) return false;
	UDYKPlacementWidget* Widget = nullptr;
	for (TObjectIterator<UDYKPlacementWidget> It; It; ++It) if (It->GetWorld() == World && It->IsInViewport()) Widget = *It;
	if (!TestNotNull(TEXT("Polygon displays real placement controls"), Widget)) return false;
	ADYKUnitSlot* First = nullptr; ADYKUnitSlot* Second = nullptr;
	int32 SlotCount = 0;
	for (TActorIterator<ADYKUnitSlot> It(World); It; ++It)
	{
		++SlotCount;
		if (It->SlotId == TEXT("EntryLeft")) First = *It;
		if (It->SlotId == TEXT("EntryRight")) Second = *It;
	}
	TestEqual(TEXT("Polygon has four authored common unit slots"), SlotCount, 4);
	if (!TestNotNull(TEXT("First authored entry slot"), First) || !TestNotNull(TEXT("Second authored entry slot"), Second)) return false;
	FHitResult Hit;
	TestTrue(TEXT("Visibility query selects the slot without physical unit collisions"), World->LineTraceSingleByChannel(Hit,
		First->GetActorLocation() + FVector(0,0,500), First->GetActorLocation() - FVector(0,0,10), ECC_Visibility));
	TestTrue(TEXT("Clickable volume belongs to common slot actor"), Hit.GetActor() == First);
	SelectPlacement(PC, First);
	TestTrue(TEXT("Selection enables actual place button"), Widget->PlaceButton->GetIsEnabled());
	PressPlacementButton(this, Widget->PlaceButton);
	TestNotNull(TEXT("Slate input purchases real defender"), First->GetOccupant());
	TestEqual(TEXT("Slate purchase debits five supplies"), Mode->GetBattleSession().GetSupplies(), 15);
	TestFalse(TEXT("Occupied slot highlight disappears"), First->FindComponentByClass<UStaticMeshComponent>()->IsVisible());
	PressPlacementButton(this, Widget->SellButton);
	TestTrue(TEXT("Slate sale clears actual occupant"), First->GetOccupant() == nullptr);
	TestEqual(TEXT("Slate sale refunds full price"), Mode->GetBattleSession().GetSupplies(), 20);
	TestTrue(TEXT("Empty slot highlight is restored"), First->FindComponentByClass<UStaticMeshComponent>()->IsVisible());
	PressPlacementButton(this, Widget->PlaceButton);
	SelectPlacement(PC, Second);
	PressPlacementButton(this, Widget->PlaceButton);
	if (FParse::Param(FCommandLine::Get(), TEXT("DYKM108Capture")))
	{
		auto* Camera = World->SpawnActor<ACameraActor>(FVector(-2800,-1300,1700), FRotator(-90,0,0));
		PC->SetViewTarget(Camera);
	}
	PressPlacementButton(this, Widget->StartButton);
	TestTrue(TEXT("Slate start begins real wave"), Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::ActiveWave);
	TestFalse(TEXT("Active wave disables place control"), Widget->PlaceButton->GetIsEnabled());
	TestFalse(TEXT("Active wave disables sale control"), Widget->SellButton->GetIsEnabled());
	AddCommand(new FPlacementTravelCommand(this, Mode, Widget));
	return true;
}

#endif
