#include "../UI/DYKStartWidget.h"
#include "../Meta/DYKCityWidgets.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Battle/DYKPlacementWidget.h"
#include "../Battle/DYKPolygonGameMode.h"
#include "../Battle/DYKUnitSlot.h"
#include "../Battle/DYKDefenderActor.h"
#include "../Battle/DYKEnemyActor.h"
#include "../Core/DYKGameInstance.h"
#include "../Core/Interaction/DYKSelectionPlayerController.h"
#include "../Saving/DYKSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealClient.h"
#include "Widgets/SWidget.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
template<typename T> T* M111Widget(UWorld* World)
{
    for (TObjectIterator<T> It; It; ++It)
        if (It->GetWorld() == World && It->IsInViewport()) return *It;
    return nullptr;
}
bool M111Press(FAutomationTestBase* Test, UButton* Button)
{
    if (!Test->TestNotNull(TEXT("M111 actual button exists"), Button) ||
        !Test->TestTrue(TEXT("M111 actual button enabled"), Button->GetIsEnabled())) return false;
    const auto Slate = Button->TakeWidget();
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(300, 50), FSlateLayoutTransform());
    const FKeyEvent Accept(EKeys::SpaceBar, FModifierKeysState(), 0, false, 0, 0);
    if (!Test->TestTrue(TEXT("M111 Slate key-down handled"), Slate->OnKeyDown(Geometry, Accept).IsEventHandled())) return false;
    return Test->TestTrue(TEXT("M111 Slate key-up executes native button"), Slate->OnKeyUp(Geometry, Accept).IsEventHandled());
}
bool M111Same(const UDYKSaveSubsystem* Save, const UDYKProfileSaveGame* Expected)
{
    const auto* Active = Save->GetActiveProfile();
    return Active && Expected && Active->SchemaVersion == Expected->SchemaVersion &&
        Active->AppliedCommandIds == Expected->AppliedCommandIds &&
        FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Active->Profile, &Expected->Profile, 0);
}
class FM111Lifecycle : public IAutomationLatentCommand
{
public:
    FM111Lifecycle(FAutomationTestBase* InTest, UDYKGameInstance* InGI, FString InPhase, FString InEvidence)
        : Test(InTest), GI(InGI), Phase(MoveTemp(InPhase)), Evidence(MoveTemp(InEvidence)),
          Deadline(FPlatformTime::Seconds() + 230.) {}
    bool Update() override
    {
        if (!GI.IsValid() || FPlatformTime::Seconds() > Deadline)
        { Test->AddError(FString::Printf(TEXT("M111 %s lifecycle timeout at stage %d."), *Phase, Stage)); return true; }
        if (FPlatformTime::Seconds() < CaptureAfter || GFrameCounter < CaptureFrame) return false;
        if (!PendingCapture.IsEmpty())
        {
#if WITH_EDITOR
            if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return false;
#endif
            const FString File = Evidence / (Phase + TEXT("-") + PendingCapture + TEXT(".png"));
            FScreenshotRequest::RequestScreenshot(File, true, false);
            Captures.Add(File); PendingCapture.Reset(); CaptureFrame = GFrameCounter + 8; return false;
        }
        auto* World = GI->GetWorld();
        if (!World || !World->HasBegunPlay() || GI->IsLevelTravelPending()) return false;
        auto* Save = GI->GetSubsystem<UDYKSaveSubsystem>();
        if (!Test->TestNotNull(TEXT("M111 loaded profile service"), Save) || !Save->GetProfileSnapshot()) return true;
        auto* City = M111Widget<UDYKCityResourceWidget>(World);
        auto* HUD = M111Widget<UDYKPlacementWidget>(World);
        auto* Mode = World->GetAuthGameMode<ADYKPolygonGameMode>();
        if (bReturnAfterCapture)
        {
            bReturnAfterCapture = false;
            if (!HUD || !M111Press(Test, HUD->ReturnButton)) return true;
            Stage = StageAfterCapture; return false;
        }
        if (bFinishAfterCapture) return Finish(Save, Phase);
        if (Stage == 0)
        {
            if (!Test->TestTrue(TEXT("OS process uses actual city default map"), World->GetOutermost()->GetName().EndsWith(TEXT("L_Citadel")))) return true;
            Test->TestNull(TEXT("Restart never restores battle GameMode"), Mode);
            Test->TestFalse(TEXT("No active mission persisted"), Save->GetProfileSnapshot()->CurrentMission.bIsActive);
            Test->TestEqual(TEXT("No deployment persisted"), Save->GetProfileSnapshot()->CurrentMission.Deployment.Num(), 0);
            Test->TestEqual(TEXT("No wave progress persisted"), Save->GetProfileSnapshot()->CurrentMission.CurrentWaveIndex, 0);
            if (Phase != TEXT("play"))
            {
                const FString Prior = Phase == TEXT("recover") ? TEXT("interrupt") : (Phase == TEXT("interrupt") ? TEXT("reload") : TEXT("play"));
                if (!ReadExpected(Save, Prior)) return true;
                Test->TestTrue(TEXT("Independent OS process loaded existing profile"), Save->GetLoadStatus() == EDYKProfileLoadStatus::Loaded);
            }
            else
            {
                Test->TestTrue(TEXT("Play requires fresh isolated profile"), Save->GetProfileSnapshot()->MissionRewards.IsEmpty());
                Before = *Save->GetProfileSnapshot();
            }
            auto* Start = M111Widget<UDYKStartWidget>(World);
            if (!Test->TestNotNull(TEXT("New OS process has start screen"), Start) || !M111Press(Test, Start->EnterButton)) return true;
            Test->TestFalse(TEXT("Enter removes start screen"), Start->IsInViewport());
            if (!Test->TestNotNull(TEXT("City controls exist"), City) || !M111Press(Test, City->MissionButton)) return true;
            Stage = 1; return false;
        }
        if (Stage == 1 || Stage == 4)
        {
            if (!Test->TestNotNull(TEXT("City button loaded real polygon"), Mode) || !Test->TestNotNull(TEXT("Polygon has actual HUD"), HUD)) return true;
            auto& Battle = Mode->GetBattleSession();
            Test->TestTrue(TEXT("Attempt begins in first preparation"), Battle.GetPhase() == EDYKBattlePhase::InitialPreparation);
            Test->TestEqual(TEXT("Fresh attempt wave is zero"), Battle.GetWaveNumber(), 0);
            Test->TestEqual(TEXT("Fresh attempt supplies reset"), Battle.GetSupplies(), 20);
            Test->TestEqual(TEXT("Fresh attempt objective HP reset"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
            Test->TestEqual(TEXT("Fresh attempt has no enemies"), Battle.GetActiveEnemyCount(), 0);
            if (!PreviousAttempt.IsEmpty()) Test->TestTrue(TEXT("Process restart creates new attempt ID"), Battle.GetAttemptId().ToString() != PreviousAttempt);
            HUD->Refresh();
            Test->TestFalse(TEXT("Empty defense cannot start"), HUD->StartButton->GetIsEnabled());
            Attempt = Battle.GetAttemptId();
            if (Phase == TEXT("recover")) return Finish(Save, TEXT("recover"));
            if (Stage == 4)
            {
                ADYKUnitSlot* BadSlot = nullptr;
                for (TActorIterator<ADYKUnitSlot> It(World); It; ++It)
                    if (It->bPlayerEditable && (!BadSlot || It->GetActorLocation().Y > BadSlot->GetActorLocation().Y)) BadSlot = *It;
                if (!Test->TestNotNull(TEXT("Single-defense negative deployment slot"), BadSlot)) return true;
                Test->TestEqual(TEXT("Negative scenario uses actual authored BendRight slot"), BadSlot->SlotId, FName(TEXT("BendRight")));
                if (!Buy(World, HUD, BadSlot)) return true;
            }
            else
            {
                int32 Count = 0;
                for (TActorIterator<ADYKUnitSlot> It(World); It; ++It)
                    if (It->bPlayerEditable) { if (!Buy(World, HUD, *It)) return true; ++Count; }
                Test->TestEqual(TEXT("Unmodified map has four player slots"), Count, 4);
                Test->TestEqual(TEXT("All authored defenders cost twenty supplies"), Battle.GetSupplies(), 0);
            }
            OldWorld = World; OldHUD = HUD; OldMode = Mode;
            HUD->Refresh();
            if (!M111Press(Test, HUD->SpeedThreeButton) || !M111Press(Test, HUD->StartButton)) return true;
            Test->TestTrue(TEXT("Actual start button begins active wave"), Battle.GetPhase() == EDYKBattlePhase::ActiveWave);
            Test->TestFalse(TEXT("Second start disabled"), HUD->StartButton->GetIsEnabled());
            WaveObservedAt = World->GetTimeSeconds();
            Stage = Stage == 4 ? 5 : 2; return false;
        }
        if (Stage == 2 || Stage == 5)
        {
            if (!Test->TestNotNull(TEXT("Battle world remains loaded"), Mode) || !HUD) return true;
            auto& Battle = Mode->GetBattleSession();
            for (TActorIterator<ADYKEnemyActor> It(World); It; ++It)
            {
                OldCombat.AddUnique(*It);
                if (It->IsAlive()) bSawLiveEnemy = true;
            }
            if (Phase == TEXT("interrupt") && bSawLiveEnemy && World->GetTimeSeconds() > WaveObservedAt + .5)
            {
                Test->TestTrue(TEXT("Interruption marker written during active wave"), Battle.GetPhase() == EDYKBattlePhase::ActiveWave);
                Test->TestTrue(TEXT("Interrupted battle has actual active enemy"), Battle.GetActiveEnemyCount() > 0);
                Test->TestTrue(TEXT("Interrupted attempt has no terminal result"), Battle.GetResult() == nullptr);
                if (!Test->TestTrue(TEXT("Battle did not mutate permanent profile"), M111Same(Save, Expected.Get()))) return true;
                HUD->Refresh(); if (!M111Press(Test, HUD->PauseButton)) return true;
                Test->TestTrue(TEXT("Actual pause keeps active wave stable until external process kill"), UGameplayStatics::IsGamePaused(World) && Battle.GetPhase() == EDYKBattlePhase::ActiveWave);
                return Finish(Save, TEXT("interrupt"));
            }
            if (Battle.GetPhase() == EDYKBattlePhase::InterwavePreparation)
            {
                Test->TestTrue(TEXT("Nonfinal wave cannot award permanent mission reward"),
                    Stage == 5 || (Phase == TEXT("reload") ? M111Same(Save, Expected.Get()) : Save->GetProfileSnapshot()->MissionRewards.Num() == Before.MissionRewards.Num()));
                HUD->Refresh(); if (!M111Press(Test, HUD->StartButton)) return true;
                return false;
            }
            if (!Battle.IsTerminal()) return false;
            if (Stage == 5 && Battle.GetPhase() == EDYKBattlePhase::Victory)
            {
                int32 RemainingDefenderHP = 0;
                for (TActorIterator<ADYKDefenderActor> It(World); It; ++It) RemainingDefenderHP += It->GetHP();
                Test->AddError(FString::Printf(TEXT("Single authored peasant naturally wins: remainingHP=%d castleHP=%d kills=%d. Natural defeat is not available in this deployment; root decision required."), RemainingDefenderHP, Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), Battle.GetResult()->EnemiesKilled));
                Test->TestTrue(TEXT("Single-defense repeated victory cannot pay twice"), FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Save->GetProfileSnapshot(), &AfterVictory, 0));
                return true;
            }
            const bool Victory = Stage == 2;
            if (!Test->TestTrue(TEXT("Natural gameplay produced intended outcome"), Battle.GetPhase() == (Victory ? EDYKBattlePhase::Victory : EDYKBattlePhase::Defeat))) return true;
            Test->TestTrue(TEXT("Natural gameplay emitted visible living enemies"), bSawLiveEnemy);
            Test->TestTrue(TEXT("Natural gameplay advanced real game time"), World->GetTimeSeconds() > WaveObservedAt + 2.);
            Test->TestTrue(TEXT("Result durable before returning"), Mode->IsMissionResultSaved());
            if (Victory)
            {
                Test->TestEqual(TEXT("Natural victory recorded all six kills"), Battle.GetResult()->EnemiesKilled, 6);
                Test->TestEqual(TEXT("Only one completion recorded"), Save->GetProfileSnapshot()->MissionCompletions.Num(), 1);
                Test->TestEqual(TEXT("Only one source receipt recorded"), Save->GetProfileSnapshot()->MissionRewards.Num(), 1);
                if (Phase == TEXT("reload"))
                {
                    Test->TestTrue(TEXT("Natural repeated victory after OS restart cannot farm"), FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Save->GetProfileSnapshot(), &Expected->Profile, 0));
                    Test->TestEqual(TEXT("New victorious attempt adds one durable command"), Save->GetActiveProfile()->AppliedCommandIds.Num(), Expected->AppliedCommandIds.Num() + 1);
                    Test->TestTrue(TEXT("New victorious attempt ID recorded"), Save->GetActiveProfile()->AppliedCommandIds.Contains(Attempt));
                    Test->TestEqual(TEXT("Repeated victory advances revision exactly once"), Save->GetRevision(), ExpectedRevision + 1);
                }
                else
                {
                    const FDYKResourceId M111Wood(TEXT("RES-WOOD")), M111Gold(TEXT("RES-GOLD"));
                    Test->TestEqual(TEXT("Natural victory credits exact configured wood"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), M111Wood), DYKCityEconomy::Balance(Before, M111Wood) + 25);
                    Test->TestEqual(TEXT("Natural victory credits exact configured gold"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), M111Gold), DYKCityEconomy::Balance(Before, M111Gold) + 10);
                    AfterVictory = *Save->GetProfileSnapshot();
                }
            }
            else
            {
                Test->TestEqual(TEXT("Negative deployment allows real enemies to destroy castle"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), 0);
                Test->TestTrue(TEXT("Natural defeat preserves every permanent field"), FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Save->GetProfileSnapshot(), &AfterVictory, 0));
                Test->TestEqual(TEXT("Defeat awards no resources"), Mode->GetAwardedResources().Num(), 0);
            }
            HUD->Refresh();
            Test->TestTrue(TEXT("Result UI displays actual outcome"), HUD->StatusText->GetText().ToString().Contains(Victory ? TEXT("Победа") : TEXT("Поражение")));
            if (Capture(Victory ? TEXT("01-victory") : TEXT("02-defeat")))
            { bReturnAfterCapture = true; StageAfterCapture = Victory ? 3 : 6; return false; }
            if (!M111Press(Test, HUD->ReturnButton)) return true;
            Stage = Victory ? 3 : 6; return false;
        }
        if (Stage == 3 || Stage == 6)
        {
            if (!Test->TestNotNull(TEXT("Result button returned to city controls"), City)) return true;
            Test->TestTrue(TEXT("Result return loaded actual separate city map"), World->GetOutermost()->GetName().EndsWith(TEXT("L_Citadel")));
            CollectGarbage(RF_NoFlags);
            Test->TestFalse(TEXT("Finished combat world collected"), OldWorld.IsValid());
            Test->TestFalse(TEXT("Finished GameMode collected"), OldMode.IsValid());
            Test->TestFalse(TEXT("Finished HUD collected"), OldHUD.IsValid());
            for (const auto& Ref : OldCombat) Test->TestFalse(TEXT("Combat actor collected after return"), Ref.IsValid());
            if (Stage == 3 && Phase == TEXT("play"))
            { if (!M111Press(Test, City->MissionButton)) return true; Stage = 4; return false; }
            if (Capture(TEXT("03-city-after-return"))) { bFinishAfterCapture = true; return false; }
            return Finish(Save, Phase);
        }
        return false;
    }
private:
    bool Capture(const TCHAR* Name)
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("DYKM111Capture"))) return false;
        PendingCapture = Name; CaptureAfter = FPlatformTime::Seconds() + 1.; return true;
    }
    bool Buy(UWorld* World, UDYKPlacementWidget* HUD, ADYKUnitSlot* Slot)
    {
        auto* PC = Cast<ADYKSelectionPlayerController>(World->GetFirstPlayerController());
        if (!Test->TestNotNull(TEXT("Native selection controller"), PC)) return false;
        FDYKWorldSelection Selection; Selection.TargetKind = EDYKSelectionTargetKind::CombatSlot; Selection.TargetActor = Slot;
        PC->OnWorldSelectionChanged().Broadcast(Selection);
        if (!M111Press(Test, HUD->PlaceButton)) return false;
        if (!Test->TestNotNull(TEXT("Button purchased actual map defender"), Slot->GetOccupant())) return false;
        OldCombat.Add(Slot->GetOccupant()); return true;
    }
    bool ReadExpected(UDYKSaveSubsystem* Save, const FString& Prior)
    {
        TArray<uint8> Bytes;
        if (!Test->TestTrue(TEXT("Previous process wrote expected snapshot fixture"), FFileHelper::LoadFileToArray(Bytes, *(Evidence / (Prior + TEXT("-profile.sav")))))) return false;
        Expected.Reset(Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
        if (!Test->TestNotNull(TEXT("Expected snapshot deserializes"), Expected.Get())) return false;
        FString Revision, PriorAttempt;
        if (!Test->TestTrue(TEXT("Previous revision fixture exists"), FFileHelper::LoadFileToString(Revision, *(Evidence / (Prior + TEXT("-revision.txt")))))) return false;
        if (!Test->TestTrue(TEXT("Previous attempt fixture exists"), FFileHelper::LoadFileToString(PriorAttempt, *(Evidence / (Prior + TEXT("-attempt.txt")))))) return false;
        PreviousAttempt = PriorAttempt;
        ExpectedRevision = FCString::Atoi64(*Revision);
        Test->TestEqual(TEXT("OS restart preserves confirmed save revision"), Save->GetRevision(), ExpectedRevision);
        return Test->TestTrue(TEXT("OS restart restores full profile and command ledger"), M111Same(Save, Expected.Get()));
    }
    bool Finish(UDYKSaveSubsystem* Save, const FString& CompletedPhase)
    {
        for (const auto& File : Captures) Test->TestTrue(TEXT("Requested rendered capture exists"), IFileManager::Get().FileExists(*File));
        TArray<uint8> Bytes;
        if (!Test->TestTrue(TEXT("Serialize confirmed full profile fixture"), UGameplayStatics::SaveGameToMemory(const_cast<UDYKProfileSaveGame*>(Save->GetActiveProfile()), Bytes))) return true;
        Test->TestTrue(TEXT("Write independently readable profile fixture"), FFileHelper::SaveArrayToFile(Bytes, *(Evidence / (CompletedPhase + TEXT("-profile.sav")))));
        Test->TestTrue(TEXT("Write revision fixture"), FFileHelper::SaveStringToFile(LexToString(Save->GetRevision()), *(Evidence / (CompletedPhase + TEXT("-revision.txt")))));
        Test->TestTrue(TEXT("Write actual attempt fixture"), FFileHelper::SaveStringToFile(Attempt.ToString(), *(Evidence / (CompletedPhase + TEXT("-attempt.txt")))));
        if (!Test->HasAnyErrors())
        {
            const FString Marker = FString::Printf(TEXT("{\"phase\":\"%s\",\"attempt\":\"%s\",\"revision\":%lld,\"activeWave\":%s,\"paused\":%s}"), *CompletedPhase, *Attempt.ToString(), Save->GetRevision(), CompletedPhase == TEXT("interrupt") ? TEXT("true") : TEXT("false"), UGameplayStatics::IsGamePaused(GI->GetWorld()) ? TEXT("true") : TEXT("false"));
            Test->TestTrue(TEXT("Write completed phase marker last"), FFileHelper::SaveStringToFile(Marker, *(Evidence / (CompletedPhase + TEXT("-complete.json")))));
        }
        return true;
    }
    FAutomationTestBase* Test;
    TWeakObjectPtr<UDYKGameInstance> GI;
    FString Phase, Evidence, PreviousAttempt;
    double Deadline, WaveObservedAt = 0, CaptureAfter = 0;
    uint64 CaptureFrame = 0;
    int64 ExpectedRevision = 0;
    int32 Stage = 0;
    bool bSawLiveEnemy = false, bReturnAfterCapture = false, bFinishAfterCapture = false;
    int32 StageAfterCapture = 0;
    FString PendingCapture;
    TArray<FString> Captures;
    FGuid Attempt;
    FDYKPlayerProfileSaveData Before, AfterVictory;
    TStrongObjectPtr<UDYKProfileSaveGame> Expected;
    TWeakObjectPtr<UWorld> OldWorld;
    TWeakObjectPtr<ADYKPolygonGameMode> OldMode;
    TWeakObjectPtr<UDYKPlacementWidget> OldHUD;
    TArray<TWeakObjectPtr<ADYKCombatUnit>> OldCombat;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKSliceAcrossProcessesTest, "DYK.M111.RuntimeSliceAcrossProcesses",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKSliceAcrossProcessesTest::RunTest(const FString& Parameters)
{
    FString Phase, Evidence;
    if (!TestTrue(TEXT("M111 dedicated process phase required"), FParse::Value(FCommandLine::Get(), TEXT("DYKM111Phase="), Phase) &&
        (Phase == TEXT("play") || Phase == TEXT("reload") || Phase == TEXT("interrupt") || Phase == TEXT("recover")))) return false;
    if (!TestTrue(TEXT("M111 dedicated evidence directory required"), FParse::Value(FCommandLine::Get(), TEXT("DYKM111EvidenceDir="), Evidence) && !Evidence.IsEmpty())) return false;
    IFileManager::Get().MakeDirectory(*Evidence, true);
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
    if (!TestNotNull(TEXT("M111 requires actual -game process"), World)) return false;
    auto* Instance = World->GetGameInstance<UDYKGameInstance>();
    if (!TestNotNull(TEXT("Actual persistent game instance"), Instance)) return false;
    AddCommand(new FM111Lifecycle(this, Instance, Phase, Evidence)); return true;
}
#endif
