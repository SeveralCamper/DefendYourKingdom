#include "../Battle/DYKEnemyActor.h"
#include "../Battle/DYKEnemyRoute.h"
#include "../Battle/DYKPolygonGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "../Core/DYKGameInstance.h"

namespace
{
	ADYKPolygonGameMode* RequireEnemyWorld(FAutomationTestBase* Test)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				auto* Mode = Cast<ADYKPolygonGameMode>(Context.World()->GetAuthGameMode());
				if (!Test->TestNotNull(TEXT("Requires the real polygon GameMode"), Mode)) return nullptr;
				if (!Test->TestTrue(TEXT("Requires a fresh initial-preparation world"),
					Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::InitialPreparation)) return nullptr;
				return Mode;
			}
		}
		Test->AddError(TEXT("Requires its own fresh L_MissionPolygon -game invocation; missing game world is not a pass."));
		return nullptr;
	}

	ADYKEnemyRoute* MakeEnemyTestRoute(UWorld* World, FName Id, const TArray<FVector>& Points)
	{
		auto* Route = World->SpawnActor<ADYKEnemyRoute>();
		Route->RouteId = Id;
		Route->ProtectedObjectId = TEXT("PolygonCastle");
		Route->Path->SetSplinePoints(Points, ESplineCoordinateSpace::World, false);
		for (int32 Index = 0; Index < Points.Num(); ++Index)
			Route->Path->SetSplinePointType(Index, ESplinePointType::Linear, false);
		Route->Path->UpdateSpline();
		return Route;
	}

	struct FEnemyRoutesProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ADYKPolygonGameMode> Mode;
		TWeakObjectPtr<ADYKEnemyActor> A, B;
		TWeakObjectPtr<ADYKEnemyRoute> RouteA, RouteB;
		FGuid Wave;
		double Deadline = FPlatformTime::Seconds() + 25;
		double Started = 0, Arrived = 0, FirstHit = 0, KilledAt = 0;
		int32 Stage = 0;
	};

	class FEnemyRoutesCommand : public IAutomationLatentCommand
	{
	public:
		explicit FEnemyRoutesCommand(TSharedRef<FEnemyRoutesProbe> State) : Probe(State) {}
		virtual bool Update() override
		{
			auto* T = Probe->Test;
			if (!Probe->Mode.IsValid() || FPlatformTime::Seconds() > Probe->Deadline)
			{
				T->AddError(TEXT("Route/attack runtime timed out or lost its world.")); return true;
			}
			auto& Session = Probe->Mode->GetBattleSession();
			const double Now = Probe->Mode->GetWorld()->GetTimeSeconds();
			if (Probe->Stage < 4 && (!Probe->A.IsValid() || !Probe->B.IsValid()))
			{
				T->AddError(TEXT("A live route enemy disappeared before being killed.")); return true;
			}
			if (Probe->Stage == 0 && Now - Probe->Started > 0.4)
			{
				const auto CheckRoute = [T](ADYKEnemyActor* Enemy, ADYKEnemyRoute* Route)
				{
					const float Distance = Enemy->GetRouteDistance();
					T->TestTrue(TEXT("Movement advances on the assigned path"), Distance > 100 && Distance < Route->Path->GetSplineLength());
					T->TestTrue(TEXT("3D actor position follows assigned reusable route"), Enemy->GetActorLocation().Equals(
						Route->Path->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World), 0.1));
					T->TestTrue(TEXT("Visual forward follows the 3D authored direction"), FVector::DotProduct(Enemy->GetActorForwardVector(),
						Route->Path->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)) > 0.99);
					T->TestTrue(TEXT("3D route changes elevation"), Enemy->GetActorLocation().Z > 0);
				};
				CheckRoute(Probe->A.Get(), Probe->RouteA.Get());
				CheckRoute(Probe->B.Get(), Probe->RouteB.Get());
				T->TestTrue(TEXT("Two distinct assigned entrances are not randomly interchanged"),
					!Probe->A->GetActorLocation().Equals(Probe->B->GetActorLocation(), 10));
				T->TestEqual(TEXT("Following path never damages objective"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				Probe->Stage = 1;
			}
			else if (Probe->Stage == 1 && Probe->A->HasReachedObjective() && Probe->B->HasReachedObjective())
			{
				Probe->Arrived = Now;
				T->TestEqual(TEXT("Arrival is not a one-shot damage event"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				T->TestEqual(TEXT("Both contacting enemies remain active"), Session.GetActiveEnemyCount(), 2);
				T->TestTrue(TEXT("Independent paths merge at the authored contact point"), Probe->A->GetActorLocation().Equals(Probe->B->GetActorLocation(), 0.1));
				T->TestTrue(TEXT("Stationary enemies stop movement ticks"), !Probe->A->IsActorTickEnabled() && !Probe->B->IsActorTickEnabled());
				Probe->Stage = 2;
			}
			else if (Probe->Stage == 2 && Session.GetProtectedObjectHP(TEXT("PolygonCastle")) < 100)
			{
				Probe->FirstHit = Now;
				T->TestEqual(TEXT("Each first scheduled hit deals one authored damage"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 98);
				T->TestTrue(TEXT("Contact waits for the configured impact delay"), Now - Probe->Arrived >= 0.15);
				T->TestTrue(TEXT("Contacting enemies keep the wave active"), Session.GetPhase() == EDYKBattlePhase::ActiveWave);
				Probe->Stage = 3;
			}
			else if (Probe->Stage == 3 && Session.GetProtectedObjectHP(TEXT("PolygonCastle")) < 98)
			{
				T->TestEqual(TEXT("Enemies repeat their hits rather than despawning"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 96);
				T->TestTrue(TEXT("Repeated hit uses the authored two game-second interval"), Now - Probe->FirstHit > 1.8 && Now - Probe->FirstHit < 2.3);
				T->TestEqual(TEXT("Repeated attacks preserve active enemies"), Session.GetActiveEnemyCount(), 2);
				auto* A = Probe->A.Get(); auto* B = Probe->B.Get();
				T->TestFalse(TEXT("Zero damage is rejected"), A->ApplyDamage(0));
				T->TestFalse(TEXT("Negative damage is rejected"), A->ApplyDamage(-1));
				T->TestTrue(TEXT("Nonlethal damage is applied"), A->ApplyDamage(4));
				T->TestEqual(TEXT("Per-instance HP starts at authored ten"), A->GetHP(), 6);
				T->TestEqual(TEXT("Other instance keeps independent HP"), B->GetHP(), 10);
				const FGuid EnemyId = A->GetEnemyId();
				T->TestTrue(TEXT("Lethal damage kills once"), A->ApplyDamage(6));
				T->TestFalse(TEXT("Repeated death is rejected before teardown"), A->ApplyDamage(6));
				T->TestFalse(TEXT("Repeated session death is rejected"), Session.RemoveEnemy(Probe->Wave, EnemyId, true));
				T->TestTrue(TEXT("Second enemy can die independently"), B->ApplyDamage(10));
				T->TestEqual(TEXT("Deaths clear only their active registrations"), Session.GetActiveEnemyCount(), 0);
				Probe->KilledAt = Now;
				Probe->Stage = 4;
			}
			else if (Probe->Stage == 4 && Now - Probe->KilledAt > 2.4)
			{
				T->TestTrue(TEXT("Final live enemy removal resolves the wave"), Session.GetPhase() == EDYKBattlePhase::InterwavePreparation);
				T->TestEqual(TEXT("Dead actors cannot deliver their queued next hit"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 96);
				T->TestFalse(TEXT("Stale-wave spawn is rejected"), Probe->Mode->SpawnEnemy(Probe->RouteA.Get(), Probe->Wave, TEXT("MainEntry")) != nullptr);
				Session.Abort(Now);
				T->TestNotNull(TEXT("Abort exposes immutable result"), Session.GetResult());
				if (Session.GetResult()) T->TestEqual(TEXT("Only three true kills count; three despawns do not"), Session.GetResult()->EnemiesKilled, 3);
				return true;
			}
			return false;
		}
	private:
		TSharedRef<FEnemyRoutesProbe> Probe;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKEnemyRoutesTest, "DYK.M107.RuntimeRoutesAndRepeatedAttacks",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKEnemyRoutesTest::RunTest(const FString& Parameters)
{
	auto* Mode = RequireEnemyWorld(this);
	if (!Mode) return false;
	auto* World = Mode->GetWorld();
	auto& Session = Mode->GetBattleSession();
	auto* RouteA = MakeEnemyTestRoute(World, TEXT("TestEntranceA"), { {-600,0,0}, {-300,0,120}, {0,0,120}, {300,0,120} });
	auto* RouteB = MakeEnemyTestRoute(World, TEXT("TestEntranceB"), { {-300,-300,0}, {-300,0,120}, {0,0,120}, {300,0,120} });
	TestTrue(TEXT("Pre-wave enemy spawn rejected"), Mode->SpawnEnemy(RouteA, FGuid::NewGuid(), TEXT("MainEntry")) == nullptr);
	Session.AddDefense(FGuid::NewGuid());
	if (!TestTrue(TEXT("Preparation starts a real wave"), Session.StartWave(Session.GetPreparationToken(), World->GetTimeSeconds()))) return false;
	const FGuid Wave = Session.GetWaveToken();
	TestTrue(TEXT("Unknown group spawn rejected"), Mode->SpawnEnemy(RouteA, Wave, TEXT("Unknown")) == nullptr);
	TestTrue(TEXT("Stale token spawn rejected"), Mode->SpawnEnemy(RouteA, FGuid::NewGuid(), TEXT("MainEntry")) == nullptr);
	TestTrue(TEXT("Null route spawn rejected"), Mode->SpawnEnemy(nullptr, Wave, TEXT("MainEntry")) == nullptr);
	RouteA->Path->SetClosedLoop(true);
	TestTrue(TEXT("Closed route cannot spawn an enemy"), Mode->SpawnEnemy(RouteA, Wave, TEXT("MainEntry")) == nullptr);
	RouteA->Path->SetClosedLoop(false);
	RouteA->ProtectedObjectId = TEXT("Unknown");
	TestTrue(TEXT("Unknown objective route cannot spawn an enemy"), Mode->SpawnEnemy(RouteA, Wave, TEXT("MainEntry")) == nullptr);
	RouteA->ProtectedObjectId = TEXT("PolygonCastle");
	auto* A = Mode->SpawnEnemy(RouteA, Wave, TEXT("MainEntry"));
	auto* B = Mode->SpawnEnemy(RouteB, Wave, TEXT("MainEntry"));
	if (!TestNotNull(TEXT("First route enemy spawned"), A) || !TestNotNull(TEXT("Second route enemy spawned"), B)) return false;
	auto* CancelRoute = MakeEnemyTestRoute(World, TEXT("CancelledImpact"), { {0,300,100}, {30,300,100} });
	auto* CancelEnemy = Mode->SpawnEnemy(CancelRoute, Wave, TEXT("MainEntry"));
	if (!TestNotNull(TEXT("Cancellation enemy spawned"), CancelEnemy)) return false;
	// Exercise a real scheduled impact without waiting for a long route. The normal
	// movement method reaches contact and arms the same world TimerManager callback.
	CancelEnemy->Tick(1.f);
	TestTrue(TEXT("Cancellation probe has reached contact and armed its first impact"), CancelEnemy->HasReachedObjective());
	TestEqual(TEXT("No contact damage before the delayed impact"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
	TestTrue(TEXT("Lethal damage before first impact is accepted"), CancelEnemy->ApplyDamage(10));
	TestFalse(TEXT("Duplicate lethal damage before timer dispatch is rejected"), CancelEnemy->ApplyDamage(10));
	TArray<ADYKEnemyActor*> Extra;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* Enemy = Mode->SpawnEnemy(RouteA, Wave, TEXT("MainEntry"));
		if (!TestNotNull(TEXT("Formation width never limits active enemy count"), Enemy)) return false;
		TestFalse(TEXT("Enemies never block each other physically"), Enemy->GetActorEnableCollision());
		TestTrue(TEXT("Overlapping spawn is allowed"), Enemy->GetActorLocation().Equals(A->GetActorLocation(), 0.1));
		Extra.Add(Enemy);
	}
	TestEqual(TEXT("More than three enemies can be active on a road"), Session.GetActiveEnemyCount(), 5);
	for (auto* Enemy : Extra) Enemy->Destroy();
	TestEqual(TEXT("Destroyed actors remove registrations without phantom kills"), Session.GetActiveEnemyCount(), 2);
	TestTrue(TEXT("Closing emission retains living enemies"), Session.CompleteSpawnGroup(Wave, TEXT("MainEntry")));
	TestEqual(TEXT("No emission group remains"), Session.GetPendingGroupCount(), 0);
	TestTrue(TEXT("Closed group rejects additional spawn"), Mode->SpawnEnemy(RouteA, Wave, TEXT("MainEntry")) == nullptr);
	const auto Probe = MakeShared<FEnemyRoutesProbe>();
	Probe->Test = this; Probe->Mode = Mode; Probe->A = A; Probe->B = B;
	Probe->RouteA = RouteA; Probe->RouteB = RouteB; Probe->Wave = Wave; Probe->Started = World->GetTimeSeconds();
	AddCommand(new FEnemyRoutesCommand(Probe));
	return true;
}

namespace
{
	struct FEnemyEmissionProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ADYKPolygonGameMode> Mode;
		FGuid Wave;
		double Deadline = FPlatformTime::Seconds() + 20, Started = 0, Defeated = 0;
		TMap<FGuid, double> Seen;
		int32 Finishes = 0;
		FDelegateHandle FinishedHandle;
	};
	class FEnemyEmissionCommand : public IAutomationLatentCommand
	{
	public:
		explicit FEnemyEmissionCommand(TSharedRef<FEnemyEmissionProbe> State) : Probe(State) {}
		virtual bool Update() override
		{
			auto* T = Probe->Test;
			if (!Probe->Mode.IsValid() || FPlatformTime::Seconds() > Probe->Deadline)
			{
				T->AddError(TEXT("Automatic emission/defeat timed out.")); Cleanup(); return true;
			}
			auto* World = Probe->Mode->GetWorld();
			auto& Session = Probe->Mode->GetBattleSession();
			const double Now = World->GetTimeSeconds();
			for (TActorIterator<ADYKEnemyActor> It(World); It; ++It)
			{
				if (!Probe->Seen.Contains(It->GetEnemyId()))
				{
					Probe->Seen.Add(It->GetEnemyId(), Now);
					T->TestEqual(TEXT("Each automatically released enemy has authored HP"), It->GetHP(), 10);
					T->TestTrue(TEXT("Only release registers an active enemy"), Session.IsEnemyActive(Probe->Wave, It->GetEnemyId()) || Session.IsTerminal());
				}
			}
			if (!Session.IsTerminal())
			{
				if (Probe->Seen.Num() < 3 && Probe->Seen.Num() > 0)
					T->TestEqual(TEXT("Release group stays pending until its last scheduled spawn"), Session.GetPendingGroupCount(), 1);
				return false;
			}
			if (Probe->Defeated == 0)
			{
				Probe->Defeated = Now;
				T->TestTrue(TEXT("Real enemy blows cause required-object defeat"), Session.GetPhase() == EDYKBattlePhase::Defeat);
				T->TestEqual(TEXT("Required objective reaches exactly zero HP"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 0);
				T->TestEqual(TEXT("Configured three enemies are released automatically"), Probe->Seen.Num(), 3);
				T->TestEqual(TEXT("Contacting enemies do not disappear on arrival or defeat"), Session.GetActiveEnemyCount(), 3);
				T->TestEqual(TEXT("Scheduled release group closes after actual registration"), Session.GetPendingGroupCount(), 0);
				TArray<double> Times; Probe->Seen.GenerateValueArray(Times); Times.Sort();
				for (int32 Index = 1; Index < Times.Num(); ++Index)
					T->TestTrue(TEXT("Automatic release respects its one game-second cadence"), Times[Index] - Times[Index - 1] > 0.8);
				if (Session.GetResult()) T->TestEqual(TEXT("Defeat counts no phantom kills"), Session.GetResult()->EnemiesKilled, 0);
				for (TActorIterator<ADYKEnemyActor> It(World); It; ++It)
				{
					T->TestFalse(TEXT("Terminal enemies stop their movement ticks"), It->IsActorTickEnabled());
					T->TestFalse(TEXT("Terminal enemies reject delayed damage commands"), It->ApplyDamage(10));
				}
			}
			else if (Now - Probe->Defeated > 2.5)
			{
				T->TestEqual(TEXT("Exactly one terminal event after later attack deadlines"), Probe->Finishes, 1);
				T->TestEqual(TEXT("Late attack timers cannot modify terminal HP"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 0);
				T->TestEqual(TEXT("No repeated or late automatic releases"), Probe->Seen.Num(), 3);
				T->TestFalse(TEXT("Terminal damage event is rejected"), Session.DamageProtectedObject(Probe->Wave, TEXT("PolygonCastle"), 1, Now));
				Cleanup(); return true;
			}
			return false;
		}
	private:
		void Cleanup() { if (Probe->Mode.IsValid()) Probe->Mode->GetBattleSession().OnFinished.Remove(Probe->FinishedHandle); }
		TSharedRef<FEnemyEmissionProbe> Probe;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKEnemyEmissionTest, "DYK.M107.RuntimeAutomaticEmissionAndDefeat",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKEnemyEmissionTest::RunTest(const FString& Parameters)
{
	auto* Mode = RequireEnemyWorld(this);
	if (!Mode) return false;
	ADYKEnemyRoute* Main = nullptr;
	for (TActorIterator<ADYKEnemyRoute> It(Mode->GetWorld()); It; ++It) if (It->RouteId == TEXT("PolygonMain")) Main = *It;
	if (!TestNotNull(TEXT("Map contains the authored main route"), Main)) return false;
	Main->Path->SetSplinePoints({FVector(0,0,100), FVector(90,0,100)}, ESplineCoordinateSpace::World);
	Main->Path->SetSplinePointType(0, ESplinePointType::Linear);
	Main->Path->SetSplinePointType(1, ESplinePointType::Linear);
	auto& Session = Mode->GetBattleSession();
	Session.AddDefense(FGuid::NewGuid());
	if (!TestTrue(TEXT("Automatic release starts only after accepted start"), Session.StartWave(Session.GetPreparationToken(), Mode->GetWorld()->GetTimeSeconds()))) return false;
	TestTrue(TEXT("Lower HP via real battle command for reproducible real attacks"), Session.DamageProtectedObject(Session.GetWaveToken(), TEXT("PolygonCastle"), 97, Mode->GetWorld()->GetTimeSeconds()));
	TestEqual(TEXT("Release is not reentrant inside session notification"), Session.GetActiveEnemyCount(), 0);
	const auto Probe = MakeShared<FEnemyEmissionProbe>();
	Probe->Test = this; Probe->Mode = Mode; Probe->Wave = Session.GetWaveToken(); Probe->Started = Mode->GetWorld()->GetTimeSeconds();
	Probe->FinishedHandle = Session.OnFinished.AddLambda([Probe](const FDYKBattleResult&) { ++Probe->Finishes; });
	AddCommand(new FEnemyEmissionCommand(Probe));
	return true;
}

namespace
{
	struct FEnemyTravelProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<UDYKGameInstance> Instance;
		TWeakObjectPtr<UWorld> OldWorld;
		TWeakObjectPtr<ADYKPolygonGameMode> OldMode;
		TWeakObjectPtr<ADYKEnemyActor> OldEnemy;
		TWeakObjectPtr<ADYKEnemyRoute> OldRoute;
		FGuid OldAttempt;
		double Deadline = FPlatformTime::Seconds() + 35;
		int32 Stage = 0;
	};
	class FEnemyTravelCommand : public IAutomationLatentCommand
	{
	public:
		explicit FEnemyTravelCommand(TSharedRef<FEnemyTravelProbe> State) : Probe(State) {}
		virtual bool Update() override
		{
			auto* T = Probe->Test;
			if (!Probe->Instance.IsValid() || FPlatformTime::Seconds() > Probe->Deadline)
			{
				T->AddError(TEXT("Live-enemy travel scenario timed out.")); return true;
			}
			auto* World = Probe->Instance->GetWorld();
			if (!World) return false;
			if (Probe->Stage == 0)
			{
				if (!Probe->OldMode.IsValid()) { T->AddError(TEXT("Old polygon disappeared before travel.")); return true; }
				for (TActorIterator<ADYKEnemyActor> It(World); It; ++It) { Probe->OldEnemy = *It; break; }
				if (!Probe->OldEnemy.IsValid() || !Probe->OldEnemy->HasReachedObjective()) return false;
				auto& Session = Probe->OldMode->GetBattleSession();
				T->TestEqual(TEXT("Travel happens before the first delayed impact"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				T->TestEqual(TEXT("Travel happens while later automatic emissions remain pending"), Session.GetPendingGroupCount(), 1);
				T->TestEqual(TEXT("One live enemy exists at travel request"), Session.GetActiveEnemyCount(), 1);
				FString Error;
				if (!T->TestTrue(TEXT("Travel with live enemy and two pending timer types is accepted"), Probe->Instance->RequestLevelTravel(EDYKLocalLevel::City, Error)))
				{ T->AddError(Error); return true; }
				Probe->Stage = 1;
			}
			else if (Probe->Stage == 1)
			{
				if (!World->GetOutermost()->GetName().Contains(TEXT("L_Citadel"))) return false;
				CollectGarbage(RF_NoFlags);
				T->TestFalse(TEXT("Unload and GC release old enemy"), Probe->OldEnemy.IsValid());
				T->TestFalse(TEXT("Unload and GC release old route"), Probe->OldRoute.IsValid());
				T->TestFalse(TEXT("Unload and GC release old battle owner"), Probe->OldMode.IsValid());
				T->TestFalse(TEXT("Unload and GC release old world"), Probe->OldWorld.IsValid());
				FString Error;
				if (!T->TestTrue(TEXT("City can load a new polygon attempt"), Probe->Instance->RequestLevelTravel(EDYKLocalLevel::MissionPolygon, Error)))
				{ T->AddError(Error); return true; }
				Probe->Stage = 2;
			}
			else if (Probe->Stage == 2)
			{
				if (!World->GetOutermost()->GetName().Contains(TEXT("L_MissionPolygon"))) return false;
				auto* Mode = Cast<ADYKPolygonGameMode>(World->GetAuthGameMode());
				if (!T->TestNotNull(TEXT("New polygon owns fresh native mode"), Mode)) return true;
				auto& Session = Mode->GetBattleSession();
				T->TestTrue(TEXT("New attempt has a new identity"), Session.GetAttemptId() != Probe->OldAttempt);
				T->TestTrue(TEXT("No stale callback starts the next attempt"), Session.GetPhase() == EDYKBattlePhase::InitialPreparation);
				T->TestEqual(TEXT("New attempt resets HP"), Session.GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				T->TestEqual(TEXT("New attempt has no inherited active registrations"), Session.GetActiveEnemyCount(), 0);
				Probe->Stage = 3;
			}
			else if (World->GetTimeSeconds() > 3)
			{
				auto* Mode = Cast<ADYKPolygonGameMode>(World->GetAuthGameMode());
				if (!T->TestNotNull(TEXT("New owner survives old timer deadlines"), Mode)) return true;
				T->TestEqual(TEXT("Old delayed impacts never damage the new attempt"), Mode->GetBattleSession().GetProtectedObjectHP(TEXT("PolygonCastle")), 100);
				T->TestEqual(TEXT("Old emission timers never create new-attempt enemies"), Mode->GetBattleSession().GetActiveEnemyCount(), 0);
				int32 Actors = 0; for (TActorIterator<ADYKEnemyActor> It(World); It; ++It) ++Actors;
				T->TestEqual(TEXT("No hidden orphan enemy Actors after travel"), Actors, 0);
				return true;
			}
			return false;
		}
	private:
		TSharedRef<FEnemyTravelProbe> Probe;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKEnemyTravelTest, "DYK.M107.RuntimeLiveEnemyTravel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKEnemyTravelTest::RunTest(const FString& Parameters)
{
	auto* Mode = RequireEnemyWorld(this);
	if (!Mode) return false;
	ADYKEnemyRoute* Main = nullptr;
	for (TActorIterator<ADYKEnemyRoute> It(Mode->GetWorld()); It; ++It) if (It->RouteId == TEXT("PolygonMain")) Main = *It;
	if (!TestNotNull(TEXT("Travel scenario uses actual map route"), Main)) return false;
	Main->Path->SetSplinePoints({FVector(0,0,100), FVector(30,0,100)}, ESplineCoordinateSpace::World);
	Main->Path->SetSplinePointType(0, ESplinePointType::Linear);
	Main->Path->SetSplinePointType(1, ESplinePointType::Linear);
	const auto Probe = MakeShared<FEnemyTravelProbe>();
	Probe->Test = this; Probe->OldMode = Mode; Probe->OldWorld = Mode->GetWorld(); Probe->OldRoute = Main;
	Probe->Instance = Cast<UDYKGameInstance>(Mode->GetWorld()->GetGameInstance()); Probe->OldAttempt = Mode->GetBattleSession().GetAttemptId();
	Mode->GetBattleSession().AddDefense(FGuid::NewGuid());
	if (!TestTrue(TEXT("Live-enemy travel starts a normal wave"), Mode->GetBattleSession().StartWave(Mode->GetBattleSession().GetPreparationToken(), Mode->GetWorld()->GetTimeSeconds()))) return false;
	AddCommand(new FEnemyTravelCommand(Probe));
	return true;
}

#endif
