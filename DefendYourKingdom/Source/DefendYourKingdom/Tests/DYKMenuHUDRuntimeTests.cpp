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
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/SWidget.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
template<typename T> T* MenuHUDWidget(UWorld* World)
{
    for (TObjectIterator<T> It; It; ++It)
        if (It->GetWorld() == World && It->IsInViewport()) return *It;
    return nullptr;
}
bool MenuHUDPress(FAutomationTestBase* Test, UButton* Button)
{
    if (!Test->TestNotNull(TEXT("Real player control exists"), Button) ||
        !Test->TestTrue(TEXT("Player control is enabled"), Button->GetIsEnabled())) return false;
    const auto Slate = Button->TakeWidget();
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(300, 50), FSlateLayoutTransform());
    const FKeyEvent Accept(EKeys::SpaceBar, FModifierKeysState(), 0, false, 0, 0);
    Test->TestTrue(TEXT("Slate accepts key-down"), Slate->OnKeyDown(Geometry, Accept).IsEventHandled());
    return Test->TestTrue(TEXT("Slate key-up routes into native command"), Slate->OnKeyUp(Geometry, Accept).IsEventHandled());
}
class FMenuHUDLifecycle : public IAutomationLatentCommand
{
public:
    FMenuHUDLifecycle(FAutomationTestBase* InTest, UDYKGameInstance* InInstance)
        : Test(InTest), Instance(InInstance), Deadline(FPlatformTime::Seconds() + 150), WaitUntil(FPlatformTime::Seconds() + 1.5) {}
    bool Update() override
    {
        if (!Instance.IsValid() || FPlatformTime::Seconds() > Deadline)
        { Test->AddError(TEXT("Menu/HUD lifecycle timed out.")); return true; }
        if (FPlatformTime::Seconds() < WaitUntil || GFrameCounter < WaitFrame) return false;
        if (!PendingCapture.IsEmpty())
        {
#if WITH_EDITOR
            if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return false;
#endif
            const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("M110") / (PendingCapture + TEXT(".png")));
            FScreenshotRequest::RequestScreenshot(File, true, false);
            Captures.Add(File); PendingCapture.Reset(); WaitFrame = GFrameCounter + 8; return false;
        }
        auto* GI = Instance.Get(); auto* World = GI->GetWorld();
        if (!World || !World->HasBegunPlay() || GI->IsLevelTravelPending()) return false;
        auto* Save = GI->GetSubsystem<UDYKSaveSubsystem>();
        auto* City = MenuHUDWidget<UDYKCityResourceWidget>(World);
        auto* HUD = MenuHUDWidget<UDYKPlacementWidget>(World);
        auto* Mode = World->GetAuthGameMode<ADYKPolygonGameMode>();
        const FDYKResourceId MenuHUDWood(TEXT("RES-WOOD")), MenuHUDGold(TEXT("RES-GOLD"));
        if (Stage == 0)
        {
            auto* Start = MenuHUDWidget<UDYKStartWidget>(World);
            if (!Test->TestNotNull(TEXT("Fresh local session displays start screen"), Start)) return true;
            Test->TestFalse(TEXT("Start gate not bypassed"), GI->bHasEnteredCity);
            Stage = 1; Capture(TEXT("01-start")); return false;
        }
        if (Stage == 1)
        {
            auto* Start = MenuHUDWidget<UDYKStartWidget>(World);
            if (!Start || !MenuHUDPress(Test, Start->EnterButton)) return true;
            Test->TestTrue(TEXT("Enter command marks session entered"), GI->bHasEnteredCity);
            Test->TestFalse(TEXT("Start screen removed by actual button"), Start->IsInViewport());
            if (!Test->TestNotNull(TEXT("City HUD is available after start gate"), City)) return true;
            City->Refresh();
            for (const TCHAR* Name : {TEXT("Пища"), TEXT("Древесина"), TEXT("Камень"), TEXT("Золото")})
                Test->TestTrue(TEXT("Approved resource is in permanent city HUD"), City->ResourceText->GetText().ToString().Contains(Name));
            Test->TestTrue(TEXT("Prepare partially full storage without claiming reward"), Save->ApplyProfileCommand(FGuid::NewGuid(), [MenuHUDWood, MenuHUDGold](auto& Profile)
            {
                for (auto& Resource : Profile.Resources)
                {
                    if (Resource.ResourceId == MenuHUDWood.ToContentId()) Resource.Amount = 95;
                    if (Resource.ResourceId == MenuHUDGold.ToContentId()) Resource.Amount = 0;
                }
                return true;
            }));
            BeforeAbort = *Save->GetProfileSnapshot();
            Stage = 2; Capture(TEXT("02-city")); return false;
        }
        if (Stage == 2)
        {
            if (!City || !MenuHUDPress(Test, City->MissionButton)) return true;
            Test->TestTrue(TEXT("City button requests real polygon travel"), GI->IsLevelTravelPending());
            Test->TestFalse(TEXT("Duplicate mission click immediately disabled"), City->MissionButton->GetIsEnabled());
            Stage = 3; return false;
        }
        if (Stage == 3 || Stage == 10 || Stage == 14)
        {
            if (!Test->TestNotNull(TEXT("Loaded polygon has actual battle mode"), Mode) || !Test->TestNotNull(TEXT("Loaded polygon has HUD"), HUD)) return true;
            HUD->Refresh();
            Test->TestFalse(TEXT("Cannot start before placing defense"), HUD->StartButton->GetIsEnabled());
            HUD->StartClicked(); Test->TestTrue(TEXT("Invalid start leaves preparation intact"), Mode->GetBattleSession().IsPreparation());
            HUD->ConfirmLeaveClicked(); Test->TestFalse(TEXT("Unconfirmed leave cannot travel"), GI->IsLevelTravelPending());
            ADYKUnitSlot* Slot = nullptr;
            for (TActorIterator<ADYKUnitSlot> It(World); It; ++It) if (It->bPlayerEditable && !It->GetOccupant()) { Slot = *It; break; }
            if (!Test->TestNotNull(TEXT("Authored compatible slot"), Slot)) return true;
            auto* PC = Cast<ADYKSelectionPlayerController>(World->GetFirstPlayerController());
            FDYKWorldSelection Selection; Selection.TargetKind = EDYKSelectionTargetKind::CombatSlot; Selection.TargetActor = Slot;
            PC->OnWorldSelectionChanged().Broadcast(Selection);
            if (!MenuHUDPress(Test, HUD->PlaceButton)) return true;
            Test->TestNotNull(TEXT("Native placement bought actual defender"), Slot->GetOccupant());
            if (Stage == 3)
            {
                Test->TestEqual(TEXT("Real price deducted"), Mode->GetBattleSession().GetSupplies(), 15);
                Test->TestTrue(TEXT("Buying normal unit has no confirmation"), HUD->ConfirmationPanel->GetVisibility() == ESlateVisibility::Collapsed);
                auto* Defender = Slot->GetOccupant();
                auto* Visual = Defender->FindComponentByClass<UStaticMeshComponent>();
                auto* Replacement = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/DefendYourKingdom/Art/Dungeon/SM_character_orc.SM_character_orc"));
                if (!Test->TestNotNull(TEXT("Imported alternate visual exists for replacement probe"), Replacement) || !Test->TestNotNull(TEXT("Defender exposes visual component"), Visual)) return true;
                const auto* DefinitionBefore = Defender->GetUnitDefinition();
                const auto PositionBefore = Defender->GetActorLocation();
                const auto DefenseIdBefore = Defender->GetDefenseId();
                UStaticMesh* OriginalMesh = Visual->GetStaticMesh();
                const int32 HPBeforeSwap = Defender->GetHP();
                Visual->SetStaticMesh(Replacement);
                Test->TestTrue(TEXT("Replacement actually changes rendering asset"), Visual->GetStaticMesh() != OriginalMesh);
                Test->TestTrue(TEXT("Visual replacement preserves immutable gameplay definition"), Defender->GetUnitDefinition() == DefinitionBefore);
                Test->TestEqual(TEXT("Visual replacement preserves current HP"), Defender->GetHP(), HPBeforeSwap);
                Test->TestEqual(TEXT("Visual replacement preserves logical position"), Defender->GetActorLocation(), PositionBefore);
                Test->TestTrue(TEXT("Visual replacement preserves defense identity"), Defender->GetDefenseId() == DefenseIdBefore);
                Test->TestFalse(TEXT("Preparation damage remains rejected after model replacement"), Defender->ApplyDamage(1));
                Test->TestEqual(TEXT("Preparation damage rejection preserves HP after model replacement"), Defender->GetHP(), HPBeforeSwap);
                Visual->SetStaticMesh(OriginalMesh);
                MenuHUDPress(Test, HUD->SellButton);
                Test->TestEqual(TEXT("Sale refunds whole purchase"), Mode->GetBattleSession().GetSupplies(), 20);
                MenuHUDPress(Test, HUD->PlaceButton);
                SelectedSlot = Slot; OldWorld = World; OldHUD = HUD;
                if (auto* Boom = PC->GetPawn()->FindComponentByClass<USpringArmComponent>()) Boom->TargetArmLength = 10000;
                CheckHUD(HUD, Mode); Stage = 4; Capture(TEXT("03-preparation-zoom-out")); return false;
            }
            MenuHUDPress(Test, HUD->StartButton);
            if (Stage == 14)
            {
                MenuHUDPress(Test, HUD->LeaveButton);
                Test->TestTrue(TEXT("Defeat probe opens leave dialog before destruction"), HUD->ConfirmationPanel->GetVisibility() == ESlateVisibility::Visible);
                Test->TestTrue(TEXT("Actual protected-object destruction applies"), Mode->GetBattleSession().DamageProtectedObject(Mode->GetBattleSession().GetWaveToken(), TEXT("PolygonCastle"), 1000, World->GetTimeSeconds()));
                HUD->Refresh();
                Test->TestTrue(TEXT("Defeat dismisses obsolete leave confirmation"), HUD->ConfirmationPanel->GetVisibility() == ESlateVisibility::Collapsed);
                Test->TestTrue(TEXT("Defeat is shown on result"), HUD->StatusText->GetText().ToString().Contains(TEXT("Поражение")));
                Test->TestEqual(TEXT("Defeat has no awarded resources"), Mode->GetAwardedResources().Num(), 0);
                Test->TestTrue(TEXT("Defeat leaves permanent awards unchanged"), FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Save->GetProfileSnapshot(), &AfterWin, 0));
                Stage = 15; Capture(TEXT("08-defeat")); return false;
            }
            Stage = 11; return false;
        }
        if (Stage == 4)
        {
            if (auto* Boom = World->GetFirstPlayerController()->GetPawn()->FindComponentByClass<USpringArmComponent>()) Boom->TargetArmLength = 1800;
            CheckHUD(HUD, Mode); Stage = 5; Capture(TEXT("04-preparation-zoom-in")); return false;
        }
        if (Stage == 5)
        {
            World->GetFirstPlayerController()->GetPawn()->SetActorLocation(SelectedSlot->GetActorLocation());
            if (auto* Boom = World->GetFirstPlayerController()->GetPawn()->FindComponentByClass<USpringArmComponent>()) Boom->TargetArmLength = 2400;
            MenuHUDPress(Test, HUD->SpeedTwoButton); Test->TestEqual(TEXT("x2 changes real world time dilation"), UGameplayStatics::GetGlobalTimeDilation(World), 2.f);
            MenuHUDPress(Test, HUD->SpeedThreeButton); Test->TestEqual(TEXT("x3 changes real world time dilation"), UGameplayStatics::GetGlobalTimeDilation(World), 3.f);
            MenuHUDPress(Test, HUD->SpeedOneButton); Test->TestEqual(TEXT("x1 restores real world time dilation"), UGameplayStatics::GetGlobalTimeDilation(World), 1.f);
            MenuHUDPress(Test, HUD->StartButton);
            Test->TestTrue(TEXT("Start button begins active wave"), Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::ActiveWave);
            Test->TestFalse(TEXT("Active wave disables purchase"), HUD->PlaceButton->GetIsEnabled());
            Test->TestFalse(TEXT("Active wave disables sale"), HUD->SellButton->GetIsEnabled());
            HUD->SellClicked(); Test->TestNotNull(TEXT("Stale sale click cannot remove active defense"), SelectedSlot->GetOccupant());
            auto* ActiveDefender = SelectedSlot->GetOccupant();
            auto* ActiveVisual = ActiveDefender->FindComponentByClass<UStaticMeshComponent>();
            UStaticMesh* ActiveOriginalMesh = ActiveVisual->GetStaticMesh();
            const int32 ActiveInitialHP = ActiveDefender->GetHP();
            Test->TestTrue(TEXT("Baseline damage applies during active wave"), ActiveDefender->ApplyDamage(1));
            Test->TestEqual(TEXT("Baseline active damage removes one HP"), ActiveDefender->GetHP(), ActiveInitialHP - 1);
            ActiveVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/DefendYourKingdom/Art/Dungeon/SM_character_orc.SM_character_orc")));
            Test->TestTrue(TEXT("Same active damage applies after visual replacement"), ActiveDefender->ApplyDamage(1));
            Test->TestEqual(TEXT("Active damage arithmetic unchanged by visual replacement"), ActiveDefender->GetHP(), ActiveInitialHP - 2);
            ActiveVisual->SetStaticMesh(ActiveOriginalMesh);
            CheckHUD(HUD, Mode); Stage = 6; Capture(TEXT("05-active-wave")); return false;
        }
        if (Stage == 6)
        {
            MenuHUDPress(Test, HUD->PauseButton); Test->TestTrue(TEXT("Pause button pauses real world"), UGameplayStatics::IsGamePaused(World));
            PausedAt = World->GetTimeSeconds(); WaitUntil = FPlatformTime::Seconds() + .5; Stage = 7; return false;
        }
        if (Stage == 7)
        {
            Test->TestEqual(TEXT("Paused world does not advance while real time elapses"), World->GetTimeSeconds(), PausedAt);
            HUD->SpeedThreeClicked(); Test->TestEqual(TEXT("Stale speed click rejected during pause"), UGameplayStatics::GetGlobalTimeDilation(World), 1.f);
            HUD->StartClicked(); Test->TestTrue(TEXT("Paused start cannot change phase"), Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::ActiveWave);
            MenuHUDPress(Test, HUD->LeaveButton);
            Test->TestTrue(TEXT("Leave asks for confirmation"), HUD->ConfirmationPanel->GetVisibility() == ESlateVisibility::Visible);
            Test->TestFalse(TEXT("Opening confirmation cannot travel"), GI->IsLevelTravelPending());
            Stage = 8; Capture(TEXT("06-leave-confirmation")); return false;
        }
        if (Stage == 8)
        {
            MenuHUDPress(Test, HUD->CancelLeaveButton);
            Test->TestTrue(TEXT("Cancel hides confirmation"), HUD->ConfirmationPanel->GetVisibility() == ESlateVisibility::Collapsed);
            Test->TestTrue(TEXT("Cancel preserves pause"), UGameplayStatics::IsGamePaused(World));
            Test->TestFalse(TEXT("Cancel preserves attempt"), Mode->GetBattleSession().IsTerminal());
            MenuHUDPress(Test, HUD->LeaveButton); MenuHUDPress(Test, HUD->ConfirmLeaveButton);
            Test->TestTrue(TEXT("Confirmed leave requests city"), GI->IsLevelTravelPending()); Stage = 9; return false;
        }
        if (Stage == 9)
        {
            if (!City) return false;
            Test->TestFalse(TEXT("Returning city has no recurring start gate"), MenuHUDWidget<UDYKStartWidget>(World) != nullptr);
            Test->TestFalse(TEXT("Leaving paused mission does not pause city"), UGameplayStatics::IsGamePaused(World));
            Test->TestEqual(TEXT("Abandon gives no reward receipt"), Save->GetProfileSnapshot()->MissionRewards.Num(), BeforeAbort.MissionRewards.Num());
            CollectGarbage(RF_NoFlags);
            Test->TestFalse(TEXT("Old battle world collected"), OldWorld.IsValid());
            Test->TestFalse(TEXT("Old HUD collected despite timers/delegates"), OldHUD.IsValid());
            MenuHUDPress(Test, City->MissionButton); Stage = 10; return false;
        }
        if (Stage == 11)
        {
            if (Mode->GetBattleSession().IsPreparation()) { HUD->Refresh(); MenuHUDPress(Test, HUD->StartButton); }
            if (!bFinalDialogOpened && Mode->GetBattleSession().GetWaveNumber() == Mode->GetBattleSession().GetTotalWaves())
            {
                MenuHUDPress(Test, HUD->LeaveButton); bFinalDialogOpened = true;
                Test->TestTrue(TEXT("Final active wave has open confirmation before completion"), HUD->ConfirmationPanel->GetVisibility() == ESlateVisibility::Visible);
            }
            for (TActorIterator<ADYKEnemyActor> It(World); It; ++It) if (It->IsAlive()) It->ApplyDamage(1000);
            if (Mode->GetBattleSession().GetPhase() != EDYKBattlePhase::Victory) return false;
            HUD->Refresh(); Test->TestTrue(TEXT("Victory result shows outcome"), HUD->StatusText->GetText().ToString().Contains(TEXT("Победа")));
            Test->TestTrue(TEXT("Victory dismisses obsolete leave confirmation"), HUD->ConfirmationPanel->GetVisibility() == ESlateVisibility::Collapsed);
            Test->TestTrue(TEXT("Result separately labels delivered and deferred resources"), HUD->RewardText->GetText().ToString().Contains(TEXT("Получено:")) && HUD->RewardText->GetText().ToString().Contains(TEXT("Ожидает места")));
            int64 AwardedMenuHUDWood = 0, DeferredMenuHUDWood = 0;
            for (const auto& R : Mode->GetAwardedResources()) if (R.ResourceId == MenuHUDWood.ToContentId()) AwardedMenuHUDWood += R.Amount;
            for (const auto& R : Mode->GetDeferredResources()) if (R.ResourceId == MenuHUDWood.ToContentId()) DeferredMenuHUDWood += R.Amount;
            Test->TestEqual(TEXT("Result reports only five wood actually credited"), AwardedMenuHUDWood, int64(5));
            Test->TestEqual(TEXT("Result reports twenty wood retained"), DeferredMenuHUDWood, int64(20));
            Test->TestEqual(TEXT("Victory persisted real bounded wood balance"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), MenuHUDWood), int64(100));
            AfterWin = *Save->GetProfileSnapshot();
            HUD->Refresh(); Mode->SettleMissionResult();
            Test->TestTrue(TEXT("Repeated result refresh/settlement cannot pay twice"), FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Save->GetProfileSnapshot(), &AfterWin, 0));
            Stage = 12; Capture(TEXT("07-victory")); return false;
        }
        if (Stage == 12) { MenuHUDPress(Test, HUD->ReturnButton); Stage = 13; return false; }
        if (Stage == 13) { if (!City) return false; MenuHUDPress(Test, City->MissionButton); Stage = 14; return false; }
        if (Stage == 15) { MenuHUDPress(Test, HUD->ReturnButton); Stage = 16; return false; }
        if (Stage == 16)
        {
            if (!City) return false;
            Test->TestEqual(TEXT("Both result routes retain permanent wood"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), MenuHUDWood), int64(100));
            for (const auto& File : Captures) Test->TestTrue(TEXT("Requested rendered UI capture exists"), IFileManager::Get().FileExists(*File));
            return true;
        }
        return false;
    }
private:
    void Capture(const TCHAR* Name)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("DYKM110Capture"))) { PendingCapture = Name; WaitUntil = FPlatformTime::Seconds() + 1.; }
    }
    void CheckHUD(UDYKPlacementWidget* HUD, ADYKPolygonGameMode* Mode)
    {
        HUD->Refresh(); const FString Text = HUD->HUDText->GetText().ToString();
        for (const TCHAR* Label : {TEXT("Волна"), TEXT("Припасы"), TEXT("Монеты ловушек"), TEXT("Скорость")})
            Test->TestTrue(TEXT("Permanent battle HUD field stays visible at camera extremes"), Text.Contains(Label));
        for (const auto& Objective : Mode->GetBattleSession().GetProtectedObjects())
            if (Objective.bRequired) Test->TestTrue(TEXT("HUD includes actual required-object HP"), Text.Contains(FString::Printf(TEXT("%d / %d HP"), Mode->GetBattleSession().GetProtectedObjectHP(Objective.Id), Objective.MaxHP)));
        Test->TestTrue(TEXT("HUD has configured total wave count"), Text.Contains(FString::Printf(TEXT("/ %d"), Mode->GetBattleSession().GetTotalWaves())));
        Test->TestTrue(TEXT("HUD is viewport UI independent of world camera scale"), HUD->IsInViewport() && HUD->HUDText->IsVisible());
        Test->TestFalse(TEXT("HUD does not reveal future/remaining enemy counter"), Text.Contains(TEXT("Осталось")) || Text.Contains(TEXT("Врагов:")));
    }
    FAutomationTestBase* Test;
    TWeakObjectPtr<UDYKGameInstance> Instance;
    TWeakObjectPtr<ADYKUnitSlot> SelectedSlot;
    TWeakObjectPtr<UWorld> OldWorld;
    TWeakObjectPtr<UDYKPlacementWidget> OldHUD;
    FDYKPlayerProfileSaveData BeforeAbort, AfterWin;
    double Deadline, WaitUntil;
    double PausedAt = 0;
    uint64 WaitFrame = 0;
    int32 Stage = 0;
    bool bFinalDialogOpened = false;
    FString PendingCapture;
    TArray<FString> Captures;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKMenuHUDLifecycleTest, "DYK.M110.RuntimeMenusHUDAndResults",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKMenuHUDLifecycleTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
    if (!TestNotNull(TEXT("M1.10 UI test requires a fresh -game city process"), World)) return false;
    auto* GI = World->GetGameInstance<UDYKGameInstance>();
    if (!TestNotNull(TEXT("Actual local game instance"), GI)) return false;
    AddCommand(new FMenuHUDLifecycle(this, GI)); return true;
}
#endif
