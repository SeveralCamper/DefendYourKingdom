#include "../Battle/DYKPolygonGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "../Core/DYKGameInstance.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../Saving/DYKProfileSaveGame.h"

namespace
{
	struct FBattleRuntimeProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ADYKPolygonGameMode> Mode;
		TWeakObjectPtr<UWorld> World;
		double Deadline = FPlatformTime::Seconds() + 20;
		int32 Stage = 0, Boundaries = 0, Finishes = 0;
		bool bCascade = false, bBeforeTickInvoked = false, bLateDamageInvoked = false, bFinalBoundarySeen = false;
		uint64 RequestFrame = 0, TerminalFrame = 0;
		FGuid Enemy, Wave, Attempt;
		FDelegateHandle WorldTickHandle, PostActorTickHandle, FinishedHandle, BoundaryHandle;

		void Cleanup()
		{
			FWorldDelegates::OnWorldTickStart.Remove(WorldTickHandle);
			FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
			if (Mode.IsValid())
			{
				Mode->GetBattleSession().OnFinished.Remove(FinishedHandle);
				Mode->GetBattleSession().OnWaveCompleted.Remove(BoundaryHandle);
			}
		}
	};

	class FBattleRuntimeCommand : public IAutomationLatentCommand
	{
	public:
		explicit FBattleRuntimeCommand(TSharedRef<FBattleRuntimeProbe> InProbe) : Probe(InProbe) {}
		virtual bool Update() override
		{
			if (!Probe->Mode.IsValid() || !Probe->World.IsValid() || FPlatformTime::Seconds() > Probe->Deadline)
			{
				Probe->Test->AddError(TEXT("Runtime battle timed out or lost its actual game world."));
				Probe->Cleanup();
				return true;
			}
			auto& Session = Probe->Mode->GetBattleSession();
			auto* Test = Probe->Test;
			if (Probe->Stage == 0)
			{
				if (Session.GetPhase() != EDYKBattlePhase::InterwavePreparation) return false;
				Test->TestEqual(TEXT("Actual GM timer paid first-wave supplies once"), Session.GetSupplies(), 30);
				Test->TestEqual(TEXT("Actual GM timer emitted first boundary"), Probe->Boundaries, 1);
				Test->TestEqual(TEXT("Nonfinal GM timer did not finalize mission"), Probe->Finishes, 0);
				Test->TestTrue(TEXT("Runtime second wave starts"), Session.StartWave(Session.GetPreparationToken(), Probe->World->GetTimeSeconds()));
				Probe->Wave = Session.GetWaveToken();
				Probe->Enemy = FGuid::NewGuid();
				Test->TestTrue(TEXT("Runtime registers final enemy"), Session.RegisterEnemy(Probe->Wave, TEXT("MainEntry"), Probe->Enemy));
				if (Probe->bCascade)
				{
					const TSharedRef<FBattleRuntimeProbe> State = Probe;
					Probe->WorldTickHandle = FWorldDelegates::OnWorldTickStart.AddLambda(
						[State](UWorld* World, ELevelTick TickType, float DeltaSeconds)
						{
							if (World != State->World.Get() || State->bBeforeTickInvoked || !State->Mode.IsValid()) return;
							State->bBeforeTickInvoked = true;
							State->RequestFrame = GFrameCounter;
							auto& Battle = State->Mode->GetBattleSession();
							State->Test->TestTrue(TEXT("Pre-timer callback closes final emission"), Battle.CompleteSpawnGroup(State->Wave, TEXT("MainEntry")));
							State->Test->TestTrue(TEXT("Pre-timer callback removes last final enemy"), Battle.RemoveEnemy(State->Wave, State->Enemy, true));
							State->Test->TestTrue(TEXT("Clear before TimerManager tick does not finish immediately"), Battle.GetResult() == nullptr);
						});
					// Unlike equal-expiry timer heap order, this hook is guaranteed after
					// TimerManager.Tick and all actor tick groups in UE5.5 LevelTick.cpp.
					Probe->PostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda(
						[State](UWorld* World, ELevelTick TickType, float DeltaSeconds)
						{
							if (World != State->World.Get() || !State->bBeforeTickInvoked || State->bLateDamageInvoked || !State->Mode.IsValid()) return;
							State->bLateDamageInvoked = true;
							State->Test->TestTrue(TEXT("Late post-actor damage really runs in same world frame"), GFrameCounter == State->RequestFrame);
							State->Test->TestTrue(TEXT("Damage after TimerManager tick wins over queued victory"),
								State->Mode->GetBattleSession().DamageProtectedObject(State->Wave, TEXT("PolygonCastle"), 100, World->GetTimeSeconds()));
						});
				}
				else
				{
					Test->TestTrue(TEXT("Runtime closes final group"), Session.CompleteSpawnGroup(Probe->Wave, TEXT("MainEntry")));
					Test->TestTrue(TEXT("Runtime removes last enemy"), Session.RemoveEnemy(Probe->Wave, Probe->Enemy, true));
					Test->TestTrue(TEXT("Runtime last death is deferred"), Session.GetResult() == nullptr);
				}
				Probe->Stage = 1;
			}
			else if (Probe->Stage == 1)
			{
				if (!Session.GetResult()) return false;
				Test->TestTrue(TEXT("Expected actual runtime terminal state"), Session.GetPhase() ==
					(Probe->bCascade ? EDYKBattlePhase::Defeat : EDYKBattlePhase::Victory));
				Test->TestEqual(TEXT("Exactly one actual runtime final event"), Probe->Finishes, 1);
				Test->TestEqual(TEXT("No final supplies in runtime"), Session.GetSupplies(), 30);
				Test->TestEqual(TEXT("Runtime completed-wave count"), Session.GetResult()->CompletedWaves, Probe->bCascade ? 1 : 2);
				Test->TestEqual(TEXT("Both actual runtime enemies killed"), Session.GetResult()->EnemiesKilled, 2);
				Test->TestTrue(TEXT("Runtime result attempt identity"), Session.GetResult()->AttemptId == Probe->Attempt);
				if (Probe->bCascade)
				{
					Test->TestTrue(TEXT("Cascade used world pre-tick callback"), Probe->bBeforeTickInvoked);
					Test->TestTrue(TEXT("Cascade executed late combat timer"), Probe->bLateDamageInvoked);
					Test->TestEqual(TEXT("Defeated final wave emits no completion boundary"), Probe->Boundaries, 1);
				}
				else
				{
					Test->TestEqual(TEXT("Victory restores/clears both wave boundaries"), Probe->Boundaries, 2);
					Test->TestTrue(TEXT("Final boundary was observed before final notification"), Probe->bFinalBoundarySeen);
				}
				Probe->TerminalFrame = GFrameCounter;
				Probe->Stage = 2;
			}
			else if (GFrameCounter > Probe->TerminalFrame + 2)
			{
				Test->TestEqual(TEXT("Delayed production resolution cannot emit another final event"), Probe->Finishes, 1);
				Test->TestFalse(TEXT("Terminal actual-world session rejects restart"), Session.StartWave(Session.GetPreparationToken(), Probe->World->GetTimeSeconds()));
				Test->TestFalse(TEXT("Terminal actual-world session rejects damage"), Session.DamageProtectedObject(Probe->Wave, TEXT("PolygonCastle"), 100, Probe->World->GetTimeSeconds()));
				Probe->Cleanup();
				return true;
			}
			return false;
		}
	private:
		TSharedRef<FBattleRuntimeProbe> Probe;
	};

	bool RunRuntimeProbe(FAutomationTestBase* Test, bool bCascade)
	{
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
		}
		if (!World)
		{
			Test->AddError(TEXT("Requires its own fresh -game polygon invocation; no actual runtime world exists."));
			return false;
		}
		auto* Mode = Cast<ADYKPolygonGameMode>(World->GetAuthGameMode());
		if (!Test->TestNotNull(TEXT("Real polygon game mode"), Mode)) return false;
		auto& Session = Mode->GetBattleSession();
		if (!Test->TestTrue(TEXT("Real polygon starts in preparation"), Session.GetPhase() == EDYKBattlePhase::InitialPreparation)) return false;
		Test->TestFalse(TEXT("Runtime begins without fabricated defense"), Session.CanStartWave());
		Test->TestEqual(TEXT("Runtime begins without fabricated enemies"), Session.GetActiveEnemyCount(), 0);
		Test->TestEqual(TEXT("Runtime starts with DEV_PLACEHOLDER full objective HP"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
		const auto Probe = MakeShared<FBattleRuntimeProbe>();
		Probe->Test = Test;
		Probe->Mode = Mode;
		Probe->World = World;
		Probe->bCascade = bCascade;
		Probe->Attempt = Session.GetAttemptId();
		Probe->BoundaryHandle = Session.OnWaveCompleted.AddLambda([Probe](int32 Completed)
		{
			++Probe->Boundaries;
			if (Completed == 2) Probe->bFinalBoundarySeen = true;
		});
		Probe->FinishedHandle = Session.OnFinished.AddLambda([Probe](const FDYKBattleResult& Result)
		{
			++Probe->Finishes;
			if (!Probe->bCascade) Probe->Test->TestTrue(TEXT("Boundary consumers notified before runtime result consumers"), Probe->bFinalBoundarySeen);
		});
		Session.AddDefense(FGuid::NewGuid());
		Test->TestTrue(TEXT("Runtime starts first wave"), Session.StartWave(Session.GetPreparationToken(), World->GetTimeSeconds()));
		const FGuid Wave = Session.GetWaveToken(), Enemy = FGuid::NewGuid();
		Session.RegisterEnemy(Wave, TEXT("MainEntry"), Enemy);
		Session.CompleteSpawnGroup(Wave, TEXT("MainEntry"));
		Session.RemoveEnemy(Wave, Enemy, true);
		Test->TestTrue(TEXT("First wave requires actual GM timer, no synchronous completion"), Session.GetPhase() == EDYKBattlePhase::ActiveWave);
		Test->AddCommand(new FBattleRuntimeCommand(Probe));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleRuntimeVictoryTest, "DYK.M106.RuntimeAutomaticVictory",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKBattleRuntimeVictoryTest::RunTest(const FString& Parameters)
{
	return RunRuntimeProbe(this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleRuntimeCascadeTest, "DYK.M106.RuntimeSameFrameDefeat",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKBattleRuntimeCascadeTest::RunTest(const FString& Parameters)
{
	return RunRuntimeProbe(this, true);
}

namespace
{
	struct FBattleTravelProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<UDYKGameInstance> Instance;
		TWeakObjectPtr<UWorld> OldWorld;
		TWeakObjectPtr<ADYKPolygonGameMode> OldMode;
		FDYKPlayerProfileSaveData InitialProfile, CityProfile;
		int64 InitialRevision = 0, CityRevision = 0;
		FGuid InitialAttempt, NewAttempt;
		int32 Stage = 0, Finishes = 0;
		EDYKBattlePhase OldOutcome = EDYKBattlePhase::Uninitialized;
		FDelegateHandle FinishedHandle;
		double Deadline = FPlatformTime::Seconds() + 30;
		uint64 ReloadedFrame = 0;

		bool SameProfile(const FDYKPlayerProfileSaveData& Expected) const
		{
			const auto* Save = Instance.IsValid() ? Instance->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
			const auto* Actual = Save ? Save->GetProfileSnapshot() : nullptr;
			return Actual && FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Actual, &Expected, 0);
		}
		void Cleanup()
		{
			if (OldMode.IsValid()) OldMode->GetBattleSession().OnFinished.Remove(FinishedHandle);
		}
	};

	class FBattleTravelCommand : public IAutomationLatentCommand
	{
	public:
		explicit FBattleTravelCommand(TSharedRef<FBattleTravelProbe> InProbe) : Probe(InProbe) {}
		virtual bool Update() override
		{
			if (!Probe->Instance.IsValid() || FPlatformTime::Seconds() > Probe->Deadline)
			{
				Probe->Test->AddError(TEXT("Battle travel timed out or lost persistent GameInstance."));
				Probe->Cleanup();
				return true;
			}
			UWorld* World = Probe->Instance->GetWorld();
			if (!World || !World->HasBegunPlay() || Probe->Instance->IsLevelTravelPending()) return false;
			auto* Test = Probe->Test;
			const auto* Save = Probe->Instance->GetSubsystem<UDYKSaveSubsystem>();
			if (Probe->Stage == 0)
			{
				if (!World->GetOutermost()->GetName().Contains(TEXT("L_Citadel"))) return false;
				Test->TestTrue(TEXT("Level travel replaced battle world"), World != Probe->OldWorld.Get());
				Test->TestEqual(TEXT("Unloaded battle finalized once"), Probe->Finishes, 1);
				Test->TestTrue(TEXT("Pending resolution teardown aborts battle without victory"), Probe->OldOutcome == EDYKBattlePhase::Aborted);
				// City initializes its starter economy on first visit. Snapshot after that
				// legitimate migration, then require the returning battle to preserve it.
				if (!Test->TestNotNull(TEXT("City still has persistent profile"), Save->GetProfileSnapshot())) { Probe->Cleanup(); return true; }
				Probe->CityProfile = *Save->GetProfileSnapshot();
				Probe->CityRevision = Save->GetRevision();
				CollectGarbage(RF_NoFlags);
				Test->TestFalse(TEXT("Old battle GameMode is actually collected"), Probe->OldMode.IsValid());
				Test->TestFalse(TEXT("Old battle world is actually collected"), Probe->OldWorld.IsValid());
				FString Error;
				if (!Test->TestTrue(TEXT("Actual city-to-polygon load accepted"), Probe->Instance->RequestLevelTravel(EDYKLocalLevel::MissionPolygon, Error)))
				{
					Test->AddError(Error); Probe->Cleanup(); return true;
				}
				Probe->Stage = 1;
			}
			else if (Probe->Stage == 1)
			{
				if (!World->GetOutermost()->GetName().Contains(TEXT("L_MissionPolygon"))) return false;
				auto* Mode = Cast<ADYKPolygonGameMode>(World->GetAuthGameMode());
				if (!Test->TestNotNull(TEXT("Freshly loaded polygon owns native mode"), Mode)) { Probe->Cleanup(); return true; }
				const auto& Battle = Mode->GetBattleSession();
				Probe->NewAttempt = Battle.GetAttemptId();
				Test->TestTrue(TEXT("Reload creates new attempt identity"), Probe->NewAttempt != Probe->InitialAttempt);
				Test->TestTrue(TEXT("Reload starts in initial preparation"), Battle.GetPhase() == EDYKBattlePhase::InitialPreparation);
				Test->TestEqual(TEXT("Reload resets all objective HP"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				Test->TestEqual(TEXT("Reload resets mission supplies"), Battle.GetSupplies(), 20);
				Test->TestEqual(TEXT("Reload starts before first wave"), Battle.GetWaveNumber(), 0);
				Test->TestEqual(TEXT("Reload inherits no enemies"), Battle.GetActiveEnemyCount(), 0);
				Test->TestEqual(TEXT("Reload inherits no active emission groups"), Battle.GetPendingGroupCount(), 0);
				Test->TestFalse(TEXT("Reload inherits no defenses"), Battle.CanStartWave());
				Test->TestTrue(TEXT("Reload inherits no terminal result"), Battle.GetResult() == nullptr);
				Test->TestTrue(TEXT("Battle reload preserves complete city profile"), Probe->SameProfile(Probe->CityProfile));
				Test->TestEqual(TEXT("Battle reload does not write profile"), Save->GetRevision(), Probe->CityRevision);
				Probe->ReloadedFrame = GFrameCounter;
				Probe->Stage = 2;
			}
			else if (GFrameCounter > Probe->ReloadedFrame + 2)
			{
				const auto* Mode = Cast<ADYKPolygonGameMode>(World->GetAuthGameMode());
				Test->TestTrue(TEXT("Old queued callback cannot mutate new attempt"), Mode && Mode->GetBattleSession().GetAttemptId() == Probe->NewAttempt
					&& Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::InitialPreparation);
				Test->TestEqual(TEXT("Old queued callback cannot finalize twice"), Probe->Finishes, 1);
				Test->TestTrue(TEXT("Profile remains unchanged after delayed callbacks"), Probe->SameProfile(Probe->CityProfile));
				Probe->Cleanup();
				return true;
			}
			return false;
		}
	private:
		TSharedRef<FBattleTravelProbe> Probe;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleRuntimeTravelTest, "DYK.M106.RuntimePendingResolutionTravel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKBattleRuntimeTravelTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	}
	if (!TestNotNull(TEXT("Travel test requires actual game world"), World)) return false;
	auto* Instance = Cast<UDYKGameInstance>(World->GetGameInstance());
	auto* Mode = Cast<ADYKPolygonGameMode>(World->GetAuthGameMode());
	if (!TestNotNull(TEXT("Travel test uses real GameInstance"), Instance) || !TestNotNull(TEXT("Travel test uses real battle mode"), Mode)) return false;
	auto* Save = Instance->GetSubsystem<UDYKSaveSubsystem>();
	if (!TestNotNull(TEXT("Travel test profile service"), Save) || !TestNotNull(TEXT("Travel test profile snapshot"), Save->GetProfileSnapshot())) return false;
	auto& Battle = Mode->GetBattleSession();
	if (!TestTrue(TEXT("Travel test begins fresh"), Battle.GetPhase() == EDYKBattlePhase::InitialPreparation)) return false;
	const auto Probe = MakeShared<FBattleTravelProbe>();
	Probe->Test = this; Probe->Instance = Instance; Probe->OldWorld = World; Probe->OldMode = Mode;
	Probe->InitialProfile = *Save->GetProfileSnapshot(); Probe->InitialRevision = Save->GetRevision();
	Probe->InitialAttempt = Battle.GetAttemptId();
	Probe->FinishedHandle = Battle.OnFinished.AddLambda([Probe](const FDYKBattleResult& Result)
	{
		++Probe->Finishes;
		Probe->OldOutcome = Result.Outcome;
		Probe->Test->TestTrue(TEXT("Battle commands and real EndPlay preserve pre-battle profile"), Probe->SameProfile(Probe->InitialProfile));
		Probe->Test->TestEqual(TEXT("Battle commands and real EndPlay perform no durable write"),
			Probe->Instance->GetSubsystem<UDYKSaveSubsystem>()->GetRevision(), Probe->InitialRevision);
	});
	Battle.AddDefense(FGuid::NewGuid());
	Battle.StartWave(Battle.GetPreparationToken(), World->GetTimeSeconds());
	const FGuid Wave = Battle.GetWaveToken(), Enemy = FGuid::NewGuid();
	Battle.RegisterEnemy(Wave, TEXT("MainEntry"), Enemy);
	Battle.DamageProtectedObject(Wave, TEXT("PolygonCastle"), 37, World->GetTimeSeconds());
	Battle.CompleteSpawnGroup(Wave, TEXT("MainEntry"));
	Battle.RemoveEnemy(Wave, Enemy, true);
	TestTrue(TEXT("Travel requested while real owner resolution still queued"), Battle.GetPhase() == EDYKBattlePhase::ActiveWave);
	TestTrue(TEXT("Pending battle has no result"), Battle.GetResult() == nullptr);
	FString Error;
	if (!TestTrue(TEXT("Actual polygon-to-city load accepted"), Instance->RequestLevelTravel(EDYKLocalLevel::City, Error)))
	{
		AddError(Error); Probe->Cleanup(); return false;
	}
	AddCommand(new FBattleTravelCommand(Probe));
	return true;
}

#endif
