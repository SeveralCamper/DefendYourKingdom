#include "../Meta/DYKMissionRewards.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Meta/DYKCityWidgets.h"
#include "../Battle/DYKEnemyActor.h"
#include "../Battle/DYKPlacementWidget.h"
#include "../Battle/DYKPolygonGameMode.h"
#include "../Battle/DYKUnitSlot.h"
#include "../Core/DYKGameInstance.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "DYKMissionRewardTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWidget.h"

namespace
{
	UWorld* RewardRuntimeWorld()
	{
		for (const auto& C : GEngine->GetWorldContexts()) if (C.WorldType == EWorldType::Game) return C.World();
		return nullptr;
	}
	template<typename T> T* RewardWorldWidget(UWorld* World)
	{
		for (TObjectIterator<T> It; It; ++It) if (It->GetWorld() == World && It->IsInViewport()) return *It;
		return nullptr;
	}
	bool RewardPress(FAutomationTestBase* T, UButton* Button)
	{
		if (!T->TestNotNull(TEXT("Player action has real native UButton"), Button)) return false;
		if (!T->TestTrue(TEXT("Player action is enabled"), Button->GetIsEnabled())) return false;
		const auto Slate = Button->TakeWidget();
		const auto Geometry = FGeometry::MakeRoot(FVector2D(250.f, 40.f), FSlateLayoutTransform());
		const FKeyEvent Accept(EKeys::SpaceBar, FModifierKeysState(), 0, false, 0, 0);
		T->TestTrue(TEXT("Slate key-down handled"), Slate->OnKeyDown(Geometry, Accept).IsEventHandled());
		return T->TestTrue(TEXT("Slate key-up invokes player command"), Slate->OnKeyUp(Geometry, Accept).IsEventHandled());
	}
	bool RewardStartRealDefense(FAutomationTestBase* T, ADYKPolygonGameMode* Mode)
	{
		auto& B = Mode->GetBattleSession();
		for (TActorIterator<ADYKUnitSlot> It(Mode->GetWorld()); It; ++It)
			if (It->bPlayerEditable && !It->GetOccupant())
			{
				if (!T->TestNotNull(TEXT("Real player defense purchased"), Mode->PlaceDefender(*It, B.GetPreparationToken()))) return false;
				return T->TestTrue(TEXT("Actual battle wave starts"), B.StartWave(B.GetPreparationToken(), Mode->GetWorld()->GetTimeSeconds()));
			}
		T->AddError(TEXT("Authored polygon has no available unit slots.")); return false;
	}
	bool RewardRuntimeSame(const UDYKSaveSubsystem* Save, const FDYKPlayerProfileSaveData& Expected)
	{
		return Save && Save->GetProfileSnapshot() && FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Save->GetProfileSnapshot(), &Expected, 0);
	}
	struct FRewardLifecycleProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<UDYKGameInstance> Instance;
		TWeakObjectPtr<UWorld> OldWorld;
		TWeakObjectPtr<ADYKPolygonGameMode> OldMode;
		TWeakObjectPtr<UDYKPlacementWidget> OldWidget;
		FDYKPlayerProfileSaveData BeforeWin, AfterClaim;
		FGuid FirstAttempt;
		int64 BeforeRevision = 0;
		int32 Stage = 0;
		double Deadline = FPlatformTime::Seconds() + 60;
		bool bFirstInterwaveChecked = false;
		bool bWriteBlocked = false;
		bool Return(ADYKPolygonGameMode* Mode)
		{
			auto* Widget = RewardWorldWidget<UDYKPlacementWidget>(Mode->GetWorld());
			if (!Test->TestNotNull(TEXT("Result screen exists"), Widget)) return false;
			Widget->Refresh();
			OldWorld = Mode->GetWorld(); OldMode = Mode; OldWidget = Widget;
			return RewardPress(Test, Widget->ReturnButton);
		}
		void EliminateReleasedEnemies(ADYKPolygonGameMode* Mode)
		{
			for (TActorIterator<ADYKEnemyActor> It(Mode->GetWorld()); It; ++It)
			{
				if (!It->IsAlive()) continue;
				const FGuid Token = Mode->GetBattleSession().GetWaveToken(), Id = It->GetEnemyId();
				Test->TestTrue(TEXT("Actual enemy death applies"), It->ApplyDamage(1000));
				Test->TestFalse(TEXT("Duplicate real death callback cannot count kill again"), Mode->GetBattleSession().RemoveEnemy(Token, Id, true));
			}
		}
	};
	class FRewardLifecycleCommand : public IAutomationLatentCommand
	{
	public:
		explicit FRewardLifecycleCommand(TSharedRef<FRewardLifecycleProbe> State) : P(State) {}
		virtual bool Update() override
		{
			auto* T = P->Test;
			if (!P->Instance.IsValid() || FPlatformTime::Seconds() > P->Deadline)
			{
				T->AddError(TEXT("Mission reward lifecycle timed out or lost GameInstance.")); return true;
			}
			auto* World = P->Instance->GetWorld();
			if (!World || !World->HasBegunPlay() || P->Instance->IsLevelTravelPending()) return false;
			auto* Save = P->Instance->GetSubsystem<UDYKSaveSubsystem>();
			auto* Mode = World->GetAuthGameMode<ADYKPolygonGameMode>();
			const auto& Def = DYKMissionRewards::PolygonDEV_PLACEHOLDER();
			const FDYKResourceId RewardRuntimeWood(TEXT("RES-WOOD"));
			FString Error;
			if (P->Stage == 0 || P->Stage == 5)
			{
				if (!Mode) return false;
				auto& Battle = Mode->GetBattleSession();
				P->EliminateReleasedEnemies(Mode);
				if (Battle.GetPhase() == EDYKBattlePhase::InterwavePreparation)
				{
					if (P->Stage == 0)
					{
						T->TestTrue(TEXT("Nonfinal wave has no permanent reward"), RewardRuntimeSame(Save, P->BeforeWin));
						T->TestEqual(TEXT("Nonfinal wave does not save profile"), Save->GetRevision(), P->BeforeRevision);
						P->bFirstInterwaveChecked = true;
						const FString Blocker = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("M109-runtime-obstruction"));
						T->TestTrue(TEXT("Create actual result-save IO obstruction"), FFileHelper::SaveStringToFile(TEXT("blocked"), *Blocker));
						FDYKMissionRewardTestAccess::Prefix(*Save, FPaths::Combine(Blocker, TEXT("profile")));
						P->bWriteBlocked = true;
					}
					T->TestTrue(TEXT("Second actual wave starts"), Battle.StartWave(Battle.GetPreparationToken(), World->GetTimeSeconds()));
				}
				if (Battle.GetPhase() != EDYKBattlePhase::Victory) return false;
				if (P->bWriteBlocked)
				{
					T->TestFalse(TEXT("Failed save is never reported as durable success"), Mode->IsMissionResultSaved());
					T->TestFalse(TEXT("Unsaved result cannot return to city"), Mode->ReturnToCity(Error));
					T->TestFalse(TEXT("Unsaved result cannot start pending travel"), P->Instance->IsLevelTravelPending());
					T->TestTrue(TEXT("Failed result write preserves all pre-victory state"), RewardRuntimeSame(Save, P->BeforeWin));
					T->TestEqual(TEXT("Failed result write leaves revision untouched"), Save->GetRevision(), P->BeforeRevision);
					T->TestTrue(TEXT("Failed write retains original immutable victory"), Battle.GetResult() && Battle.GetResult()->AttemptId == P->FirstAttempt);
					FDYKMissionRewardTestAccess::Prefix(*Save, FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames/DYK_LocalProfile")));
					T->TestTrue(TEXT("Same immutable result retries after storage restored"), Mode->SettleMissionResult());
					P->bWriteBlocked = false;
				}
				T->TestTrue(TEXT("Victory settlement durable before result return"), Mode->IsMissionResultSaved());
				T->TestTrue(TEXT("Result includes nonnegative active time"), Battle.GetResult()->ActiveSeconds >= 0);
				T->TestEqual(TEXT("Result counts six kills despite duplicate death callbacks"), Battle.GetResult()->EnemiesKilled, 6);
				const auto Settled = *Save->GetProfileSnapshot();
				const auto Revision = Save->GetRevision();
				T->TestTrue(TEXT("Repeated result opening/settlement accepted"), Mode->SettleMissionResult());
				T->TestEqual(TEXT("Repeated result opening performs no new write"), Save->GetRevision(), Revision);
				T->TestTrue(TEXT("Repeated result callback cannot duplicate award"), RewardRuntimeSame(Save, Settled));
				if (P->Stage == 0)
				{
					T->TestTrue(TEXT("First interwave observation actually ran"), P->bFirstInterwaveChecked);
					T->TestEqual(TEXT("Near-full city wood bounded at one hundred"), DYKCityEconomy::Balance(Settled, RewardRuntimeWood), int64(100));
					T->TestEqual(TEXT("One completion recorded by actual GameMode"), Settled.MissionCompletions.Num(), 1);
					T->TestEqual(TEXT("One permanent source receipt"), Settled.MissionRewards.Num(), 1);
					if (Settled.MissionRewards.Num() > 0 && Settled.MissionRewards[0].Remaining.Num() > 0)
						T->TestEqual(TEXT("Twenty wood retained for city collection"), Settled.MissionRewards[0].Remaining[0].Amount, int64(20));
					else T->AddError(TEXT("Victory did not retain expected warehouse overflow."));
					P->Stage = 1;
				}
				else
				{
					T->TestTrue(TEXT("Repeated victory in fresh world cannot farm reward"), RewardRuntimeSame(Save, P->AfterClaim));
					P->Stage = 6;
				}
				if (!P->Return(Mode)) return true;
				return false;
			}
			if (P->Stage == 1)
			{
				if (!World->GetOutermost()->GetName().Contains(TEXT("L_Citadel"))) return false;
				CollectGarbage(RF_NoFlags);
				T->TestFalse(TEXT("Finished battle world collected after city return"), P->OldWorld.IsValid());
				T->TestFalse(TEXT("Finished battle GameMode collected"), P->OldMode.IsValid());
				T->TestFalse(TEXT("Result widget collected"), P->OldWidget.IsValid());
				auto* HUD = RewardWorldWidget<UDYKCityResourceWidget>(World);
				if (!T->TestNotNull(TEXT("City has resource HUD"), HUD)) return true;
				HUD->Refresh();
				if (!T->TestNotNull(TEXT("City has campaign reward button"), HUD->CollectRewardsButton.Get())) return true;
				T->TestFalse(TEXT("Full warehouse disables campaign collection"), HUD->CollectRewardsButton->GetIsEnabled());
				T->TestTrue(TEXT("Real spend creates room for pending mission award"), DYKCityEconomy::Spend(*Save, FGuid::NewGuid(), RewardRuntimeWood, 20));
				HUD->Refresh();
				if (!P->Test->TestTrue(TEXT("City exposes actual campaign reward button"), HUD->CollectRewardsButton->GetIsEnabled()) || !RewardPress(T, HUD->CollectRewardsButton)) return true;
				T->TestEqual(TEXT("City button transfers retained award exactly"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardRuntimeWood), int64(100));
				T->TestTrue(TEXT("Claimed source retained with empty remainder"), Save->GetProfileSnapshot()->MissionRewards[0].Remaining.IsEmpty());
				HUD->Refresh(); T->TestFalse(TEXT("No second claim possible"), HUD->CollectRewardsButton->GetIsEnabled());
				P->AfterClaim = *Save->GetProfileSnapshot();
				if (!T->TestTrue(TEXT("Return to polygon accepted"), P->Instance->RequestLevelTravel(EDYKLocalLevel::MissionPolygon, Error))) { T->AddError(Error); return true; }
				P->Stage = 2; return false;
			}
			if (P->Stage == 2 || P->Stage == 4)
			{
				if (!Mode) return false;
				auto& Battle = Mode->GetBattleSession();
				T->TestTrue(TEXT("New attempt starts in preparation"), Battle.GetPhase() == EDYKBattlePhase::InitialPreparation);
				T->TestTrue(TEXT("New attempt uses fresh ID"), Battle.GetAttemptId() != P->FirstAttempt);
				T->TestEqual(TEXT("New attempt resets objective HP"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				T->TestEqual(TEXT("New attempt resets supplies"), Battle.GetSupplies(), 20);
				T->TestEqual(TEXT("New attempt resets enemies"), Battle.GetActiveEnemyCount(), 0);
				T->TestTrue(TEXT("Permanent profile survives fresh battle world"), RewardRuntimeSame(Save, P->AfterClaim));
				if (!RewardStartRealDefense(T, Mode)) return true;
				if (P->Stage == 2)
				{
					T->TestTrue(TEXT("Actual protected-object death ends mission"), Battle.DamageProtectedObject(Battle.GetWaveToken(), TEXT("PolygonCastle"), 100, World->GetTimeSeconds()));
					T->TestTrue(TEXT("Actual result is defeat"), Battle.GetPhase() == EDYKBattlePhase::Defeat);
					T->TestTrue(TEXT("Defeat gives no permanent resource or completion"), RewardRuntimeSame(Save, P->AfterClaim));
					if (!P->Return(Mode)) return true;
					P->Stage = 3;
				}
				else P->Stage = 5;
				return false;
			}
			if (P->Stage == 3 || P->Stage == 6)
			{
				if (!World->GetOutermost()->GetName().Contains(TEXT("L_Citadel"))) return false;
				T->TestTrue(TEXT("City retains exactly existing permanent progress"), RewardRuntimeSame(Save, P->AfterClaim));
				if (P->Stage == 6) return true;
				if (!T->TestTrue(TEXT("Replay after defeat loads new battle"), P->Instance->RequestLevelTravel(EDYKLocalLevel::MissionPolygon, Error))) { T->AddError(Error); return true; }
				P->Stage = 4;
			}
			return false;
		}
	private:
		TSharedRef<FRewardLifecycleProbe> P;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRewardRuntimeLifecycleTest, "DYK.M109.RuntimeVictoryDefeatCityAndReplay",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKRewardRuntimeLifecycleTest::RunTest(const FString& Parameters)
{
	auto* World = RewardRuntimeWorld();
	if (!TestNotNull(TEXT("Requires own fresh L_MissionPolygon -game process"), World)) return false;
	auto* Mode = World->GetAuthGameMode<ADYKPolygonGameMode>();
	auto* Instance = Cast<UDYKGameInstance>(World->GetGameInstance());
	if (!TestNotNull(TEXT("Real polygon mode"), Mode) || !TestNotNull(TEXT("Real persistent GameInstance"), Instance)) return false;
	auto* Save = Instance->GetSubsystem<UDYKSaveSubsystem>();
	if (!TestNotNull(TEXT("Real profile service"), Save)) return false;
	TestTrue(TEXT("Dedicated runtime starts without any receipt"), Save->GetProfileSnapshot()->MissionRewards.IsEmpty());
	TestTrue(TEXT("Seed warehouse at ninety five"), Save->ApplyProfileCommand(FGuid::NewGuid(), [](auto& P)
	{
		if (!DYKCityEconomy::Initialize(P, 1794700000)) return false;
		for (auto& R : P.Resources) if (R.ResourceId == FDYKResourceId(TEXT("RES-WOOD")).ToContentId()) R.Amount = 95;
		return true;
	}));
	FString Error;
	TestFalse(TEXT("Return command rejected before terminal result"), Mode->ReturnToCity(Error));
	TestFalse(TEXT("Preparation cannot settle mission award"), Mode->SettleMissionResult());
	const auto P = MakeShared<FRewardLifecycleProbe>(); P->Test = this; P->Instance = Instance;
	P->BeforeWin = *Save->GetProfileSnapshot(); P->BeforeRevision = Save->GetRevision(); P->FirstAttempt = Mode->GetBattleSession().GetAttemptId();
	if (!RewardStartRealDefense(this, Mode)) return false;
	AddCommand(new FRewardLifecycleCommand(P)); return true;
}

#endif
