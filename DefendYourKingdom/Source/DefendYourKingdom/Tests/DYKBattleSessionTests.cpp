#include "../Battle/DYKBattleSession.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Battle/DYKPolygonGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include <limits>

namespace
{
	constexpr auto BattleTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FName Keep(TEXT("Keep")), Gate(TEXT("Gate")), Optional(TEXT("Optional"));
	const FName EntryA(TEXT("EntryA")), EntryB(TEXT("EntryB")), EventGroup(TEXT("ReservedReinforcement"));

	FDYKBattleDefinition BattleDefinition(int32 WaveCount = 2)
	{
		FDYKBattleDefinition Definition;
		Definition.InitialSupplies = 23;
		Definition.ProtectedObjects = {{Keep, 100, true}, {Gate, 40, true}, {Optional, 5, false}};
		for (int32 Index = 0; Index < WaveCount; ++Index)
		{
			Definition.Waves.Add({{EntryA, EntryB, EventGroup}, Index == 0 ? 7 : 999});
		}
		return Definition;
	}

	bool InitializeBattle(FDYKBattleSession& Session, int32 WaveCount = 2)
	{
		FString Error;
		return Session.Initialize(BattleDefinition(WaveCount), Error);
	}

	void CloseGroups(FDYKBattleSession& Session, const FGuid& Token)
	{
		Session.CompleteSpawnGroup(Token, EntryA);
		Session.CompleteSpawnGroup(Token, EntryB);
		Session.CompleteSpawnGroup(Token, EventGroup);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleDefinitionTest, "DYK.M106.DefinitionValidation", BattleTestFlags)
bool FDYKBattleDefinitionTest::RunTest(const FString& Parameters)
{
	auto Reject = [this](const FDYKBattleDefinition& Definition, const TCHAR* Label)
	{
		FDYKBattleSession Session;
		FString Error;
		TestFalse(Label, Session.Initialize(Definition, Error));
		TestFalse(TEXT("Rejected definition supplies a reason"), Error.IsEmpty());
		TestTrue(TEXT("Rejected definition leaves session uninitialized"), Session.GetPhase() == EDYKBattlePhase::Uninitialized);
		TestFalse(TEXT("Rejected definition creates no attempt ID"), Session.GetAttemptId().IsValid());
		TestTrue(TEXT("Valid definition still accepted after rejection"), Session.Initialize(BattleDefinition(), Error));
		TestTrue(TEXT("Successful initialization clears old error"), Error.IsEmpty());
	};
	FDYKBattleDefinition Definition = BattleDefinition();
	Definition.Waves.Reset(); Reject(Definition, TEXT("No waves rejected"));
	Definition = BattleDefinition(); Definition.ProtectedObjects.Reset(); Reject(Definition, TEXT("No objectives rejected"));
	Definition = BattleDefinition(); for (auto& Object : Definition.ProtectedObjects) Object.bRequired = false;
	Reject(Definition, TEXT("No mandatory objective rejected"));
	Definition = BattleDefinition(); Definition.ProtectedObjects[0].MaxHP = 0; Reject(Definition, TEXT("Nonpositive HP rejected"));
	Definition = BattleDefinition(); Definition.ProtectedObjects[1].Id = Keep; Reject(Definition, TEXT("Duplicate objective ID rejected"));
	Definition = BattleDefinition(); Definition.ProtectedObjects[0].Id = NAME_None; Reject(Definition, TEXT("Empty objective ID rejected"));
	Definition = BattleDefinition(); Definition.Waves[0].SpawnGroups.Reset(); Reject(Definition, TEXT("No spawn groups rejected"));
	Definition = BattleDefinition(); Definition.Waves[0].SpawnGroups.Add(EntryA); Reject(Definition, TEXT("Duplicate spawn group rejected"));
	Definition = BattleDefinition(); Definition.Waves[0].SpawnGroups.Add(NAME_None); Reject(Definition, TEXT("Empty spawn group rejected"));
	Definition = BattleDefinition(); Definition.InitialSupplies = -1; Reject(Definition, TEXT("Negative initial supplies rejected"));
	Definition = BattleDefinition(); Definition.Waves[0].CompletionSupplies = -1; Reject(Definition, TEXT("Negative wave supplies rejected"));
	Definition = BattleDefinition(); Definition.InitialSupplies = MAX_int32; Reject(Definition, TEXT("Potential supplies overflow rejected"));

	FDYKBattleSession Session;
	Definition = BattleDefinition();
	FString Error;
	TestTrue(TEXT("Valid copied definition accepted"), Session.Initialize(Definition, Error));
	Definition.ProtectedObjects[0].MaxHP = 1;
	Definition.Waves.Reset();
	TestEqual(TEXT("Caller cannot overwrite stored objective definition"), Session.GetProtectedObjectHP(Keep), 100);
	const FGuid Attempt = Session.GetAttemptId();
	TestFalse(TEXT("An existing attempt cannot be reinitialized"), Session.Initialize(BattleDefinition(), Error));
	TestTrue(TEXT("Rejected reinitialization preserves attempt identity"), Session.GetAttemptId() == Attempt);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleStartTest, "DYK.M106.P03T02.StartAndPhaseGuards", BattleTestFlags)
bool FDYKBattleStartTest::RunTest(const FString& Parameters)
{
	FDYKBattleSession Session;
	const FGuid Defense = FGuid::NewGuid();
	TestFalse(TEXT("Cannot place before initialization"), Session.AddDefense(Defense));
	TestFalse(TEXT("Cannot start before initialization"), Session.StartWave(FGuid::NewGuid(), 0));
	if (!TestTrue(TEXT("Initialize"), InitializeBattle(Session))) return false;
	TestTrue(TEXT("Initial preparation has no timer/automatic start"), Session.GetPhase() == EDYKBattlePhase::InitialPreparation);
	const FGuid Preparation = Session.GetPreparationToken();
	TestFalse(TEXT("Empty defense blocks start"), Session.CanStartWave());
	TestFalse(TEXT("Command also blocks empty defense"), Session.StartWave(Preparation, 1));
	TestFalse(TEXT("Invalid defense handle rejected"), Session.AddDefense(FGuid()));
	TestTrue(TEXT("Any placed defense counts, including an exhausted trap handle"), Session.AddDefense(Defense));
	TestFalse(TEXT("Cannot double-register defense"), Session.AddDefense(Defense));
	TestTrue(TEXT("Removing last defense accepted in preparation"), Session.RemoveDefense(Defense));
	TestFalse(TEXT("Empty field becomes blocked again"), Session.CanStartWave());
	TestFalse(TEXT("Cannot remove same defense twice"), Session.RemoveDefense(Defense));
	TestTrue(TEXT("Replacing defense accepted"), Session.AddDefense(Defense));
	TestFalse(TEXT("Unknown preparation command rejected"), Session.StartWave(FGuid::NewGuid(), 5));
	TestFalse(TEXT("Negative game time rejected"), Session.StartWave(Preparation, -1));
	TestFalse(TEXT("Nonfinite game time rejected"), Session.StartWave(Preparation, std::numeric_limits<double>::quiet_NaN()));
	int32 Starts = 0;
	Session.OnWaveStarted.AddLambda([&](const FGuid& Wave)
	{
		++Starts;
		TestTrue(TEXT("Wave notification carries active token"), Wave == Session.GetWaveToken());
		TestFalse(TEXT("Notification cannot reenter wave start"), Session.StartWave(Preparation, 5));
		TestFalse(TEXT("Notification cannot abort the emitting transition"), Session.Abort(5));
	});
	TestTrue(TEXT("Prepared wave starts"), Session.StartWave(Preparation, 5));
	TestFalse(TEXT("Double click cannot start another wave"), Session.StartWave(Preparation, 5));
	TestEqual(TEXT("Exactly one spawn/contract-start notification"), Starts, 1);
	TestEqual(TEXT("One logical wave started"), Session.GetWaveNumber(), 1);
	TestFalse(TEXT("Placement blocked in active wave"), Session.AddDefense(FGuid::NewGuid()));
	TestFalse(TEXT("Sale blocked in active wave"), Session.RemoveDefense(Defense));
	TestFalse(TEXT("Unfinished wave cannot resolve"), Session.ResolveWave(Session.GetWaveToken(), 6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleCompletionTest, "DYK.M106.SpawnBarrierAndInterwave", BattleTestFlags)
bool FDYKBattleCompletionTest::RunTest(const FString& Parameters)
{
	FDYKBattleSession Session;
	if (!TestTrue(TEXT("Initialize"), InitializeBattle(Session))) return false;
	Session.AddDefense(FGuid::NewGuid());
	const FGuid OldPreparation = Session.GetPreparationToken();
	Session.StartWave(OldPreparation, 10);
	const FGuid Wave = Session.GetWaveToken(), Enemy = FGuid::NewGuid(), LastEnemy = FGuid::NewGuid();
	int32 Resolutions = 0, Boundaries = 0;
	Session.OnResolutionRequested.AddLambda([&](const FGuid& Token) { ++Resolutions; TestTrue(TEXT("Resolution token"), Token == Wave); });
	Session.OnWaveCompleted.AddLambda([&](int32 Count)
	{
		++Boundaries;
		TestEqual(TEXT("Completed one wave"), Count, 1);
		TestTrue(TEXT("Read-only boundary observer sees next wave available"), Session.CanStartWave());
		TestFalse(TEXT("Boundary observer cannot reenter start despite readable availability"), Session.StartWave(Session.GetPreparationToken(), 20));
	});
	TestFalse(TEXT("Unknown entry cannot spawn"), Session.RegisterEnemy(Wave, TEXT("Unknown"), Enemy));
	TestFalse(TEXT("Invalid enemy ID cannot spawn"), Session.RegisterEnemy(Wave, EntryA, FGuid()));
	TestTrue(TEXT("First entry spawns enemy"), Session.RegisterEnemy(Wave, EntryA, Enemy));
	TestFalse(TEXT("Duplicate spawn does not count twice"), Session.RegisterEnemy(Wave, EntryB, Enemy));
	TestTrue(TEXT("First enemy killed"), Session.RemoveEnemy(Wave, Enemy, true));
	TestFalse(TEXT("Duplicate death rejected"), Session.RemoveEnemy(Wave, Enemy, true));
	TestFalse(TEXT("Dead ID cannot be registered again"), Session.RegisterEnemy(Wave, EntryB, Enemy));
	Session.CompleteSpawnGroup(Wave, EntryA);
	Session.CompleteSpawnGroup(Wave, EntryB);
	TestFalse(TEXT("No living enemies still waits for event reinforcement group"), Session.ResolveWave(Wave, 20));
	TestEqual(TEXT("Incomplete emission requests no boundary"), Resolutions, 0);
	TestTrue(TEXT("Reserved event group can spawn later"), Session.RegisterEnemy(Wave, EventGroup, LastEnemy));
	TestTrue(TEXT("Last emission completed"), Session.CompleteSpawnGroup(Wave, EventGroup));
	TestFalse(TEXT("Repeated emission completed rejected"), Session.CompleteSpawnGroup(Wave, EventGroup));
	TestFalse(TEXT("Cannot spawn after emission closed"), Session.RegisterEnemy(Wave, EventGroup, FGuid::NewGuid()));
	TestFalse(TEXT("Last planned enemy alive prevents completion"), Session.ResolveWave(Wave, 20));
	TestTrue(TEXT("Objective damage accepted"), Session.DamageProtectedObject(Wave, Keep, 17, 15));
	TestTrue(TEXT("Last enemy dies"), Session.RemoveEnemy(Wave, LastEnemy, true));
	TestEqual(TEXT("One deferred resolution requested"), Resolutions, 1);
	TestTrue(TEXT("Boundary is deferred until damage/death batch ends"), Session.GetPhase() == EDYKBattlePhase::ActiveWave);
	TestTrue(TEXT("Resolve surviving-object nonfinal wave"), Session.ResolveWave(Wave, 20));
	TestTrue(TEXT("Nonfinal transitions into preparation"), Session.GetPhase() == EDYKBattlePhase::InterwavePreparation);
	TestEqual(TEXT("Full configured supplies despite damage"), Session.GetSupplies(), 30);
	TestEqual(TEXT("Objective HP persists between waves"), Session.GetProtectedObjectHP(Keep), 83);
	TestEqual(TEXT("Exactly one boundary notification"), Boundaries, 1);
	TestTrue(TEXT("Start remains available after boundary observer"), Session.CanStartWave());
	TestFalse(TEXT("Repeated boundary rejected"), Session.ResolveWave(Wave, 21));
	TestEqual(TEXT("Supplies not paid twice"), Session.GetSupplies(), 30);
	TestFalse(TEXT("Queued previous click cannot start next wave"), Session.StartWave(OldPreparation, 30));
	TestTrue(TEXT("Fresh preparation token starts second wave"), Session.StartWave(Session.GetPreparationToken(), 100));
	TestFalse(TEXT("Old wave event cannot spawn in next wave"), Session.RegisterEnemy(Wave, EntryA, FGuid::NewGuid()));
	TestFalse(TEXT("Old wave event cannot damage next wave"), Session.DamageProtectedObject(Wave, Keep, 100, 101));
	TestEqual(TEXT("Stale events preserve HP"), Session.GetProtectedObjectHP(Keep), 83);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleVictoryTest, "DYK.M106.P03T16.FinalVictoryOnce", BattleTestFlags)
bool FDYKBattleVictoryTest::RunTest(const FString& Parameters)
{
	FDYKBattleSession Session;
	if (!TestTrue(TEXT("Initialize"), InitializeBattle(Session))) return false;
	Session.AddDefense(FGuid::NewGuid());
	Session.StartWave(Session.GetPreparationToken(), 10);
	FGuid Wave = Session.GetWaveToken();
	CloseGroups(Session, Wave);
	TestTrue(TEXT("First wave cleared"), Session.ResolveWave(Wave, 20));
	Session.StartWave(Session.GetPreparationToken(), 1000);
	Wave = Session.GetWaveToken();
	const FGuid Killed = FGuid::NewGuid(), Despawned = FGuid::NewGuid();
	Session.RegisterEnemy(Wave, EntryA, Killed);
	Session.RegisterEnemy(Wave, EventGroup, Despawned);
	CloseGroups(Session, Wave);
	Session.RemoveEnemy(Wave, Killed, true);
	TestFalse(TEXT("Last remaining enemy prevents victory"), Session.ResolveWave(Wave, 1005));
	Session.RemoveEnemy(Wave, Despawned, false);
	int32 Finishes = 0;
	Session.OnFinished.AddLambda([&](const FDYKBattleResult& Result)
	{
		++Finishes;
		TestTrue(TEXT("Victory listener sees already committed result"), Session.GetResult() == &Result);
		TestTrue(TEXT("Victory immediately terminal without interwave"), Session.GetPhase() == EDYKBattlePhase::Victory);
		TestFalse(TEXT("Result notification cannot restart"), Session.StartWave(Session.GetPreparationToken(), 1007));
		TestFalse(TEXT("Result notification cannot replace result"), Session.Abort(1007));
	});
	TestTrue(TEXT("Final wave resolves"), Session.ResolveWave(Wave, 1007));
	if (!TestNotNull(TEXT("Final result recorded"), Session.GetResult())) return false;
	const FDYKBattleResult Result = *Session.GetResult();
	TestEqual(TEXT("Final completion issues no supplies"), Session.GetSupplies(), 30);
	TestEqual(TEXT("Both waves counted"), Result.CompletedWaves, 2);
	TestEqual(TEXT("Despawn not counted as kill"), Result.EnemiesKilled, 1);
	TestEqual(TEXT("Preparation time excluded from active duration"), Result.ActiveSeconds, 17.0);
	TestTrue(TEXT("Result belongs to this attempt"), Result.AttemptId == Session.GetAttemptId());
	TestFalse(TEXT("Duplicate final completion rejected"), Session.ResolveWave(Wave, 1008));
	TestFalse(TEXT("Late objective damage cannot replace victory"), Session.DamageProtectedObject(Wave, Keep, 100, 1008));
	TestFalse(TEXT("Late enemy death rejected"), Session.RemoveEnemy(Wave, Killed, true));
	TestFalse(TEXT("Late abort rejected"), Session.Abort(1008));
	TestEqual(TEXT("Result fixed exactly once"), Finishes, 1);
	TestEqual(TEXT("Immutable recorded duration"), Session.GetResult()->ActiveSeconds, Result.ActiveSeconds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleDefeatTest, "DYK.M106.P03T17.RequiredObjectivesAndDefeat", BattleTestFlags)
bool FDYKBattleDefeatTest::RunTest(const FString& Parameters)
{
	FDYKBattleSession Session;
	if (!TestTrue(TEXT("Initialize"), InitializeBattle(Session, 1))) return false;
	Session.AddDefense(FGuid::NewGuid());
	TestFalse(TEXT("Preparation cannot take combat damage"), Session.DamageProtectedObject(FGuid::NewGuid(), Keep, 1, 0));
	Session.StartWave(Session.GetPreparationToken(), 5);
	const FGuid Wave = Session.GetWaveToken();
	TestFalse(TEXT("Unknown objective damage rejected"), Session.DamageProtectedObject(Wave, TEXT("Unknown"), 1, 6));
	TestFalse(TEXT("Negative damage rejected"), Session.DamageProtectedObject(Wave, Keep, -1, 6));
	TestFalse(TEXT("Zero damage rejected"), Session.DamageProtectedObject(Wave, Keep, 0, 6));
	TestFalse(TEXT("Damage from before wave start rejected"), Session.DamageProtectedObject(Wave, Keep, 1, 4));
	TestFalse(TEXT("Nonfinite damage time rejected"), Session.DamageProtectedObject(Wave, Keep, 1, std::numeric_limits<double>::infinity()));
	TestTrue(TEXT("Optional objective can be destroyed"), Session.DamageProtectedObject(Wave, Optional, MAX_int32, 6));
	TestEqual(TEXT("Overkill clamps HP to zero"), Session.GetProtectedObjectHP(Optional), 0);
	TestTrue(TEXT("Optional loss does not end mission"), Session.GetPhase() == EDYKBattlePhase::ActiveWave);
	TestTrue(TEXT("No result until required loss"), Session.GetResult() == nullptr);
	int32 Finishes = 0;
	Session.OnFinished.AddLambda([&](const FDYKBattleResult&) { ++Finishes; });
	TestTrue(TEXT("Second mandatory object reaches zero"), Session.DamageProtectedObject(Wave, Gate, 40, 8));
	TestTrue(TEXT("Any mandatory loss is immediate defeat"), Session.GetPhase() == EDYKBattlePhase::Defeat);
	TestEqual(TEXT("Other mandatory object survives unchanged"), Session.GetProtectedObjectHP(Keep), 100);
	TestEqual(TEXT("No wave payment for defeat"), Session.GetSupplies(), 23);
	if (!TestNotNull(TEXT("Defeat result"), Session.GetResult())) return false;
	TestEqual(TEXT("No completed waves"), Session.GetResult()->CompletedWaves, 0);
	TestEqual(TEXT("Defeat duration"), Session.GetResult()->ActiveSeconds, 3.0);
	TestFalse(TEXT("Repeated zero-HP event rejected"), Session.DamageProtectedObject(Wave, Gate, 40, 9));
	TestFalse(TEXT("Another objective cannot cause second defeat"), Session.DamageProtectedObject(Wave, Keep, 100, 9));
	TestFalse(TEXT("No later wave completion"), Session.ResolveWave(Wave, 9));
	TestFalse(TEXT("No later abort"), Session.Abort(9));
	TestEqual(TEXT("Defeat fixed once"), Finishes, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleCascadeTest, "DYK.M106.P03T23.SameFrameFinalDeaths", BattleTestFlags)
bool FDYKBattleCascadeTest::RunTest(const FString& Parameters)
{
	for (const bool bEnemyFirst : {false, true})
	{
		FDYKBattleSession Session;
		if (!TestTrue(TEXT("Initialize"), InitializeBattle(Session, 1))) return false;
		Session.AddDefense(FGuid::NewGuid());
		Session.StartWave(Session.GetPreparationToken(), 1);
		const FGuid Wave = Session.GetWaveToken(), Enemy = FGuid::NewGuid();
		Session.RegisterEnemy(Wave, EntryA, Enemy);
		CloseGroups(Session, Wave);
		int32 Finishes = 0;
		Session.OnFinished.AddLambda([&](const FDYKBattleResult&) { ++Finishes; });
		if (bEnemyFirst)
		{
			Session.RemoveEnemy(Wave, Enemy, true);
			TestTrue(TEXT("Last enemy death awaits frame boundary"), Session.GetResult() == nullptr);
		}
		TestTrue(TEXT("Same-frame required object loss"), Session.DamageProtectedObject(Wave, Keep, 100, 2));
		if (!bEnemyFirst) TestFalse(TEXT("Death after defeat cannot modify terminated attempt"), Session.RemoveEnemy(Wave, Enemy, true));
		TestFalse(TEXT("Queued victory resolution cannot replace loss"), Session.ResolveWave(Wave, 2));
		TestTrue(TEXT("Both orderings produce defeat"), Session.GetPhase() == EDYKBattlePhase::Defeat);
		TestEqual(TEXT("One outcome in death cascade"), Finishes, 1);
		TestEqual(TEXT("Cascade issues no completion supplies"), Session.GetSupplies(), 23);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBattleAbortRetryTest, "DYK.M106.AbortAndFreshAttempt", BattleTestFlags)
bool FDYKBattleAbortRetryTest::RunTest(const FString& Parameters)
{
	FDYKBattleSession BeforeStart;
	TestFalse(TEXT("Uninitialized abort rejected"), BeforeStart.Abort(0));
	InitializeBattle(BeforeStart);
	TestTrue(TEXT("Cancel initial preparation"), BeforeStart.Abort(100));
	TestTrue(TEXT("Cancellation outcome distinct from victory"), BeforeStart.GetResult()->Outcome == EDYKBattlePhase::Aborted);
	TestEqual(TEXT("No combat time before start"), BeforeStart.GetResult()->ActiveSeconds, 0.0);
	TestEqual(TEXT("No completed wave before start"), BeforeStart.GetResult()->CompletedWaves, 0);
	FDYKBattleSession Active;
	InitializeBattle(Active);
	const FGuid Defense = FGuid::NewGuid();
	Active.AddDefense(Defense);
	Active.StartWave(Active.GetPreparationToken(), 10);
	const FGuid OldWave = Active.GetWaveToken();
	Active.DamageProtectedObject(OldWave, Keep, 90, 11);
	TestFalse(TEXT("Invalid abort timestamp rejected"), Active.Abort(9));
	TestTrue(TEXT("Abort active mission"), Active.Abort(12));
	TestEqual(TEXT("Aborted mission accounts combat time"), Active.GetResult()->ActiveSeconds, 2.0);
	FDYKBattleSession Retry;
	InitializeBattle(Retry);
	TestTrue(TEXT("Retry is a new attempt ID"), Retry.GetAttemptId() != Active.GetAttemptId());
	TestEqual(TEXT("Retry resets objective health"), Retry.GetProtectedObjectHP(Keep), 100);
	TestEqual(TEXT("Retry resets supplies"), Retry.GetSupplies(), 23);
	TestEqual(TEXT("Retry starts before wave one"), Retry.GetWaveNumber(), 0);
	TestEqual(TEXT("Retry has no living enemies"), Retry.GetActiveEnemyCount(), 0);
	TestFalse(TEXT("Prior defense is not inherited"), Retry.CanStartWave());
	TestTrue(TEXT("Retry has no inherited result"), Retry.GetResult() == nullptr);
	TestFalse(TEXT("Old attempt token rejected"), Retry.DamageProtectedObject(OldWave, Keep, 100, 20));
	return true;
}

#endif
