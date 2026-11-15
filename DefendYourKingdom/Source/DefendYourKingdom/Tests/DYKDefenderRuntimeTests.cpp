#include "../Battle/DYKCombatUnit.h"
#include "../Battle/DYKDefenderActor.h"
#include "../Battle/DYKEnemyActor.h"
#include "../Battle/DYKEnemyRoute.h"
#include "../Battle/DYKPolygonGameMode.h"
#include "../Battle/DYKUnitSlot.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

namespace
{
	ADYKPolygonGameMode* RequireDefenderWorld(FAutomationTestBase* Test)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				auto* Mode = Cast<ADYKPolygonGameMode>(Context.World()->GetAuthGameMode());
				if (!Test->TestNotNull(TEXT("M1.08 runtime requires actual polygon mode"), Mode)) return nullptr;
				if (!Test->TestTrue(TEXT("Each runtime case requires a fresh preparation"), Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::InitialPreparation)) return nullptr;
				return Mode;
			}
		Test->AddError(TEXT("M1.08 runtime requires its own fresh L_MissionPolygon -game process; missing runtime is not PASS."));
		return nullptr;
	}

	ADYKUnitSlot* MakeDefenderSlot(UWorld* World, FName Id, const FVector& Position)
	{
		auto* Slot = World->SpawnActor<ADYKUnitSlot>(Position, FRotator::ZeroRotator);
		if (Slot) { Slot->SlotId = Id; Slot->bPlayerEditable = true; }
		return Slot;
	}

	ADYKEnemyRoute* MakeDefenderRoute(UWorld* World, FName Id, const FVector& Start, const FVector& End)
	{
		auto* Route = World->SpawnActor<ADYKEnemyRoute>();
		if (!Route) return nullptr;
		Route->RouteId = Id; Route->ProtectedObjectId = TEXT("PolygonCastle");
		Route->Path->SetSplinePoints({Start, End}, ESplineCoordinateSpace::World, false);
		Route->Path->SetSplinePointType(0, ESplinePointType::Linear, false);
		Route->Path->SetSplinePointType(1, ESplinePointType::Linear, false);
		Route->Path->UpdateSpline();
		return Route;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKDefenderPlacementTest, "DYK.M108.RuntimePlacementTransactions",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDYKDefenderPlacementTest::RunTest(const FString& Parameters)
{
	auto* Mode = RequireDefenderWorld(this);
	if (!Mode) return false;
	auto* World = Mode->GetWorld();
	auto& Battle = Mode->GetBattleSession();
	const FGuid Preparation = Battle.GetPreparationToken();
	const int32 InitialSupplies = Battle.GetSupplies();
	TestEqual(TEXT("Fixture uses twenty initial supplies"), InitialSupplies, 20);
	TestFalse(TEXT("An empty actual field cannot start a wave"), Battle.CanStartWave());
	const FTransform MissionTransform(FRotator(0,137,0), FVector(-25000,0,500));
	auto* MissionUnit = World->SpawnActorDeferred<ADYKDefenderActor>(ADYKDefenderActor::StaticClass(), MissionTransform);
	if (!TestNotNull(TEXT("Author can create a unit without any player slot"), MissionUnit)) return false;
	MissionUnit->bMissionPlaced = true;
	MissionUnit->FinishSpawning(MissionTransform);
	TestEqual(TEXT("Mission-placed defender initializes authored HP"), MissionUnit->GetHP(), 15);
	TestTrue(TEXT("Mission-placed defender preserves exact authored transform"), MissionUnit->GetActorTransform().Equals(MissionTransform));
	TestEqual(TEXT("Mission-placed defender never spends player supplies"), Battle.GetSupplies(), InitialSupplies);
	TestTrue(TEXT("Mission-placed defender registers for normal wave gate"), Battle.CanStartWave());
	MissionUnit->Destroy();
	TestFalse(TEXT("Destroy removes mission-placed defense from the wave-start gate"), Battle.CanStartWave());
	TestEqual(TEXT("Destroying a free mission unit never refunds player supplies"), Battle.GetSupplies(), InitialSupplies);
	auto* Slot = MakeDefenderSlot(World, TEXT("M108Price"), FVector(-20000,0,100));
	if (!TestNotNull(TEXT("Player unit slot exists"), Slot)) return false;
	TestTrue(TEXT("Null slot cannot be purchased"), Mode->PlaceDefender(nullptr, Preparation) == nullptr);
	TestFalse(TEXT("Null slot cannot be sold"), Mode->SellDefender(nullptr, Preparation));
	TestTrue(TEXT("Stale preparation token cannot purchase"), Mode->PlaceDefender(Slot, FGuid::NewGuid()) == nullptr);
	Slot->bPlayerEditable = false;
	TestTrue(TEXT("Authored locked slot cannot be purchased"), Mode->PlaceDefender(Slot, Preparation) == nullptr);
	TestEqual(TEXT("Rejected placement preserves all supplies"), Battle.GetSupplies(), InitialSupplies);
	Slot->bPlayerEditable = true;
	auto* Defender = Mode->PlaceDefender(Slot, Preparation);
	if (!TestNotNull(TEXT("Available T1 peasant can be placed"), Defender)) return false;
	TestTrue(TEXT("One unit occupies its map slot"), Slot->GetOccupant() == Defender);
	TestEqual(TEXT("QA-P02-01 placement costs authored five supplies"), Battle.GetSupplies(), InitialSupplies - 5);
	TestEqual(TEXT("Peasant begins with authored fifteen HP"), Defender->GetHP(), 15);
	TestTrue(TEXT("Placement uses exact authored logical map position"), Defender->GetActorLocation().Equals(Slot->GetActorLocation(), 0.1));
	TestFalse(TEXT("Stationary defender does not need movement ticks"), Defender->IsActorTickEnabled());
	TestTrue(TEXT("Real placement registers defense for wave gate"), Battle.CanStartWave());
	TestTrue(TEXT("Occupied slot rejects second purchase"), Mode->PlaceDefender(Slot, Preparation) == nullptr);
	TestEqual(TEXT("Rejected duplicate purchase does not charge"), Battle.GetSupplies(), InitialSupplies - 5);
	TestFalse(TEXT("Stale sell command rejected"), Mode->SellDefender(Slot, FGuid::NewGuid()));
	TestTrue(TEXT("QA-P02-01 sale accepted in preparation"), Mode->SellDefender(Slot, Preparation));
	TestTrue(TEXT("Sale releases slot"), Slot->GetOccupant() == nullptr);
	TestEqual(TEXT("QA-P02-01 full placement price refunded"), Battle.GetSupplies(), InitialSupplies);
	TestFalse(TEXT("Sale removes real defense registration"), Battle.CanStartWave());
	TestFalse(TEXT("Repeated sale cannot create supplies"), Mode->SellDefender(Slot, Preparation));
	TestEqual(TEXT("Repeated sale preserves supply total"), Battle.GetSupplies(), InitialSupplies);

	TArray<ADYKUnitSlot*> Purchased;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Candidate = MakeDefenderSlot(World, FName(*FString::Printf(TEXT("M108Budget%d"), Index)), FVector(-20000, Index * 500, 100));
		if (!TestNotNull(TEXT("Budget slot exists"), Candidate) || !TestNotNull(TEXT("Budget placement succeeds"), Mode->PlaceDefender(Candidate, Preparation))) return false;
		Purchased.Add(Candidate);
	}
	TestEqual(TEXT("Four defenders exactly exhaust supplies"), Battle.GetSupplies(), 0);
	TestTrue(TEXT("Insufficient supplies reject purchase atomically"), Mode->PlaceDefender(Slot, Preparation) == nullptr);
	TestTrue(TEXT("Rejected budget purchase keeps slot empty"), Slot->GetOccupant() == nullptr);
	TestEqual(TEXT("Balance cannot become negative"), Battle.GetSupplies(), 0);
	if (!TestTrue(TEXT("Real purchased defense starts wave"), Battle.StartWave(Preparation, World->GetTimeSeconds()))) return false;
	const FGuid OldPreparation = Preparation;
	TestTrue(TEXT("Active-wave placement is rejected"), Mode->PlaceDefender(Slot, OldPreparation) == nullptr);
	TestFalse(TEXT("Active-wave sale is rejected"), Mode->SellDefender(Purchased[0], OldPreparation));
	TestEqual(TEXT("Wave rejects commands without changing supplies"), Battle.GetSupplies(), 0);
	TestTrue(TEXT("Wave command rejection keeps original occupant"), Purchased[0]->GetOccupant() != nullptr);
	Battle.Abort(World->GetTimeSeconds());
	TestFalse(TEXT("Terminal result forbids selling"), Mode->SellDefender(Purchased[0], OldPreparation));
	return true;
}

namespace
{
	ADYKEnemyActor* SpawnContactGoblin(ADYKPolygonGameMode* Mode, FName Id, const FVector& Position)
	{
		auto* Route = MakeDefenderRoute(Mode->GetWorld(), Id, Position - FVector(30,0,0), Position);
		auto* Enemy = Mode->SpawnEnemy(Route, Mode->GetBattleSession().GetWaveToken(), TEXT("MainEntry"));
		if (Enemy) Enemy->Tick(1.f); // Real route movement reaches its stationary logical contact.
		return Enemy;
	}

	struct FDefenderImpactProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ADYKPolygonGameMode> Mode;
		TWeakObjectPtr<ADYKDefenderActor> First, Second;
		TWeakObjectPtr<ADYKEnemyActor> A, B, C, D, E;
		FVector FirstPosition, SecondPosition;
		double Deadline = FPlatformTime::Seconds() + 25;
		double StageStarted = 0;
		int32 Stage = 0;
	};

	class FDefenderImpactCommand : public IAutomationLatentCommand
	{
	public:
		explicit FDefenderImpactCommand(TSharedRef<FDefenderImpactProbe> State) : Probe(State) {}
		virtual bool Update() override
		{
			auto* T = Probe->Test;
			if (!Probe->Mode.IsValid() || !Probe->First.IsValid() || !Probe->Second.IsValid() || FPlatformTime::Seconds() > Probe->Deadline)
			{
				T->AddError(TEXT("FIFO/impact runtime timed out or lost its actual world/defenders.")); return true;
			}
			auto* Mode = Probe->Mode.Get();
			const double Now = Mode->GetWorld()->GetTimeSeconds();
			auto* First = Probe->First.Get();
			auto* Second = Probe->Second.Get();
			if (Probe->Stage <= 2 && (!Probe->A.IsValid() || !Probe->B.IsValid()))
			{
				T->AddError(TEXT("First FIFO targets disappeared before their scripted death.")); return true;
			}
			if (Probe->Stage == 0 && First->GetPendingTarget() == Probe->A.Get())
			{
				T->TestEqual(TEXT("P05-T09 delayed contact does not damage at swing start"), Probe->A->GetHP(), 10);
				T->TestTrue(TEXT("Three-dimensional separation excludes the elevated goblin"), Probe->B->GetCurrentTarget() == nullptr);
				T->TestTrue(TEXT("P05-T01 out-of-range defender has no attack target"), Second->GetCurrentTarget() == nullptr);
				T->TestEqual(TEXT("P05-T01 out-of-range defender remains unharmed"), Second->GetHP(), 15);
				T->TestTrue(TEXT("P05-T01 out-of-range defender remains at its slot"), Second->GetActorLocation().Equals(Probe->SecondPosition, 0.1));
				T->TestTrue(TEXT("First entrant becomes current target"), First->GetCurrentTarget() == Probe->A.Get());
				T->TestTrue(TEXT("Different route target is valid inside actual radius"), Probe->A->GetCurrentTarget() == First);
				Probe->B->SetActorLocation(Probe->FirstPosition + FVector(0,30,0));
				Mode->MarkCombatTargetsDirty();
				Probe->StageStarted = Now; Probe->Stage = 1;
			}
			else if (Probe->Stage == 1 && Now > Probe->StageStarted)
			{
				T->TestTrue(TEXT("P05-T05 closer newcomer cannot replace first FIFO target"), First->GetCurrentTarget() == Probe->A.Get());
				T->TestTrue(TEXT("Pending swing remains pinned to first entrant"), First->GetPendingTarget() == Probe->A.Get());
				Probe->A->SetActorLocation(Probe->FirstPosition + FVector(600,0,0));
				Mode->MarkCombatTargetsDirty();
				Probe->StageStarted = Now; Probe->Stage = 2;
			}
			else if (Probe->Stage == 2 && Now - Probe->StageStarted > 0.45)
			{
				T->TestEqual(TEXT("P05-T07 committed swing hits after target exits radius"), Probe->A->GetHP(), 6);
				T->TestEqual(TEXT("Committed swing does not redirect to closer target"), Probe->B->GetHP(), 10);
				T->TestTrue(TEXT("Range exit advances current FIFO to second entrant"), First->GetCurrentTarget() == Probe->B.Get());
				T->TestTrue(TEXT("Stationary defender never chases its departed target"), First->GetActorLocation().Equals(Probe->FirstPosition, 0.1));
				T->TestFalse(TEXT("Zero damage is rejected"), First->ApplyDamage(0));
				T->TestFalse(TEXT("Negative damage is rejected"), First->ApplyDamage(-10));
				First->StopCombat();
				T->TestTrue(TEXT("First goblin dies exactly once"), Probe->A->ApplyDamage(MAX_int32));
				T->TestTrue(TEXT("Second goblin can die independently"), Probe->B->ApplyDamage(MAX_int32));
				if (!T->TestTrue(TEXT("Simultaneous targets remain live"), Probe->C.IsValid() && Probe->D.IsValid())) return true;
				Probe->C->SetActorLocation(Probe->FirstPosition + FVector(140,0,0));
				Probe->D->SetActorLocation(Probe->FirstPosition + FVector(50,0,0));
				Mode->MarkCombatTargetsDirty();
				Probe->Stage = 3;
			}
			else if (Probe->Stage == 3 && First->GetPendingTarget() == Probe->C.Get())
			{
				T->TestTrue(TEXT("P05-T04 equal-step entries use stable registration order, not distance"), First->GetCurrentTarget() == Probe->C.Get());
				T->TestEqual(TEXT("Second same-step target remains untouched"), Probe->D->GetHP(), 10);
				auto* DeadTarget = Probe->C.Get();
				T->TestTrue(TEXT("Pending target can die before logical contact"), DeadTarget->ApplyDamage(MAX_int32));
				T->TestFalse(TEXT("Repeated target death is rejected before teardown"), DeadTarget->ApplyDamage(1));
				Probe->StageStarted = Now; Probe->Stage = 4;
			}
			else if (Probe->Stage == 4 && Now - Probe->StageStarted > 0.45)
			{
				if (!T->TestTrue(TEXT("Next FIFO target remains live"), Probe->D.IsValid())) return true;
				T->TestEqual(TEXT("P05-T08 dead swing target does not transfer its damage to another"), Probe->D->GetHP(), 10);
				T->TestTrue(TEXT("Current target advances after death"), First->GetCurrentTarget() == Probe->D.Get());
				T->TestTrue(TEXT("First defender can die independently"), First->ApplyDamage(MAX_int32));
				if (!T->TestTrue(TEXT("Attacker cancellation goblin remains live"), Probe->E.IsValid())) return true;
				Probe->E->SetActorLocation(Probe->SecondPosition + FVector(140,0,0));
				Mode->MarkCombatTargetsDirty();
				Probe->Stage = 5;
			}
			else if (Probe->Stage == 5 && Second->GetPendingTarget() == Probe->E.Get())
			{
				T->TestEqual(TEXT("New attacker has not hit before configured delay"), Probe->E->GetHP(), 10);
				T->TestTrue(TEXT("Attacker dies while its real impact timer is armed"), Second->ApplyDamage(MAX_int32));
				T->TestFalse(TEXT("Dead defender is immediately unavailable"), Second->IsAlive());
				T->TestTrue(TEXT("Death clears pending attack reference"), Second->GetPendingTarget() == nullptr);
				Probe->StageStarted = Now; Probe->Stage = 6;
			}
			else if (Probe->Stage == 6 && Now - Probe->StageStarted > 0.45)
			{
				if (!T->TestTrue(TEXT("Cancellation target remains live"), Probe->E.IsValid() && Probe->D.IsValid())) return true;
				T->TestEqual(TEXT("Death cancels the attacker's pending impact"), Probe->E->GetHP(), 10);
				T->TestTrue(TEXT("Enemy immediately removes dead defender from targets"), Probe->E->GetCurrentTarget() == nullptr);
				T->TestTrue(TEXT("Dead defender remains in its unit slot for recovery"), IsValid(Second));
				T->TestTrue(TEXT("Fourth test goblin killed once"), Probe->D->ApplyDamage(MAX_int32));
				T->TestTrue(TEXT("Fifth test goblin killed once"), Probe->E->ApplyDamage(MAX_int32));
				auto& Battle = Mode->GetBattleSession();
				Battle.Abort(Now);
				if (T->TestNotNull(TEXT("Test produces one immutable result"), Battle.GetResult()))
					T->TestEqual(TEXT("Only five actual enemy deaths count; dead defenders do not"), Battle.GetResult()->EnemiesKilled, 5);
				return true;
			}
			return false;
		}
	private:
		TSharedRef<FDefenderImpactProbe> Probe;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKDefenderImpactTest, "DYK.M108.RuntimeFIFOAndImpacts",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDYKDefenderImpactTest::RunTest(const FString& Parameters)
{
	auto* Mode = RequireDefenderWorld(this);
	if (!Mode) return false;
	auto* World = Mode->GetWorld();
	auto& Battle = Mode->GetBattleSession();
	const auto Probe = MakeShared<FDefenderImpactProbe>();
	Probe->Test = this; Probe->Mode = Mode;
	Probe->FirstPosition = FVector(-20000,0,100); Probe->SecondPosition = FVector(-20000,1000,100);
	auto* FirstSlot = MakeDefenderSlot(World, TEXT("M108FIFOFirst"), Probe->FirstPosition);
	auto* SecondSlot = MakeDefenderSlot(World, TEXT("M108FIFOSecond"), Probe->SecondPosition);
	Probe->First = Mode->PlaceDefender(FirstSlot, Battle.GetPreparationToken());
	Probe->Second = Mode->PlaceDefender(SecondSlot, Battle.GetPreparationToken());
	if (!TestTrue(TEXT("FIFO fixtures use real purchased defenders"), Probe->First.IsValid() && Probe->Second.IsValid())) return false;
	Probe->FirstPosition = Probe->First->GetActorLocation(); Probe->SecondPosition = Probe->Second->GetActorLocation();
	if (!TestTrue(TEXT("FIFO starts an actual wave"), Battle.StartWave(Battle.GetPreparationToken(), World->GetTimeSeconds()))) return false;
	Probe->A = SpawnContactGoblin(Mode, TEXT("M108FirstEntrance"), Probe->FirstPosition + FVector(140,0,0));
	Probe->B = SpawnContactGoblin(Mode, TEXT("M108SecondEntrance"), Probe->FirstPosition + FVector(0,0,300));
	Probe->C = SpawnContactGoblin(Mode, TEXT("M108SameStepFirst"), Probe->FirstPosition + FVector(5000,0,0));
	Probe->D = SpawnContactGoblin(Mode, TEXT("M108SameStepCloser"), Probe->FirstPosition + FVector(6000,0,0));
	Probe->E = SpawnContactGoblin(Mode, TEXT("M108AttackerDeath"), Probe->FirstPosition + FVector(7000,0,0));
	if (!TestTrue(TEXT("FIFO fixtures use actual live goblins registered before emission group closes"),
		Probe->A.IsValid() && Probe->B.IsValid() && Probe->C.IsValid() && Probe->D.IsValid() && Probe->E.IsValid())) return false;
	AddCommand(new FDefenderImpactCommand(Probe));
	return true;
}

namespace
{
	struct FDefenderRecoveryProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ADYKPolygonGameMode> Mode;
		TWeakObjectPtr<ADYKDefenderActor> Blocker, Recovering, Survivor;
		TWeakObjectPtr<ADYKUnitSlot> RecoverySlot;
		TWeakObjectPtr<ADYKEnemyActor> Enemy;
		FVector Contact;
		double Deadline = FPlatformTime::Seconds() + 50;
		double DeathAt = 0, StageAt = 0;
		float StoppedDistance = 0;
		int32 Stage = 0, CastleAtRevival = 0, LastCastleHP = 100, SuppliesBeforeRevival = 0;
		bool bCheckedLateCooldown = false;
	};

	class FDefenderRecoveryCommand : public IAutomationLatentCommand
	{
	public:
		explicit FDefenderRecoveryCommand(TSharedRef<FDefenderRecoveryProbe> State) : Probe(State) {}
		virtual bool Update() override
		{
			auto* T = Probe->Test;
			if (!Probe->Mode.IsValid() || !Probe->Blocker.IsValid() || !Probe->Recovering.IsValid() || !Probe->Survivor.IsValid() ||
				FPlatformTime::Seconds() > Probe->Deadline)
			{
				T->AddError(TEXT("Death/recovery/route runtime timed out or lost its real world.")); return true;
			}
			auto* Mode = Probe->Mode.Get();
			auto* World = Mode->GetWorld();
			auto& Battle = Mode->GetBattleSession();
			const double Now = World->GetTimeSeconds();
			auto* Recovering = Probe->Recovering.Get();
			if (Probe->Stage < 6 && !Probe->Enemy.IsValid())
			{
				T->AddError(TEXT("Route/resurrection enemy disappeared before its scripted death.")); return true;
			}
			if (Probe->Stage == 0 && Probe->Enemy->GetCurrentTarget() == Probe->Blocker.Get())
			{
				T->TestFalse(TEXT("Enemy stops before objective to attack nearby defender"), Probe->Enemy->HasReachedObjective());
				Probe->StoppedDistance = Probe->Enemy->GetRouteDistance();
				Probe->StageAt = Now; Probe->Stage = 1;
			}
			else if (Probe->Stage == 1 && Now - Probe->StageAt > 0.4)
			{
				T->TestEqual(TEXT("Enemy route distance remains fixed during combat"), Probe->Enemy->GetRouteDistance(), Probe->StoppedDistance);
				T->TestEqual(TEXT("Actual enemy swing damages defender with authored one damage"), Probe->Blocker->GetHP(), 14);
				T->TestTrue(TEXT("Blocker dies before enemy has finished its route"), Probe->Blocker->ApplyDamage(MAX_int32));
				T->TestTrue(TEXT("Slot is retained while recovery unit is dead"), Probe->RecoverySlot->GetOccupant() == Recovering);
				Probe->StageAt = Now; Probe->Stage = 2;
			}
			else if (Probe->Stage == 2 && Now - Probe->StageAt > 0.4)
			{
				T->TestTrue(TEXT("Enemy resumes same route after defender death"), Probe->Enemy->GetRouteDistance() > Probe->StoppedDistance + 50);
				T->TestTrue(TEXT("Dead defender is immediately removed from enemy FIFO"), Probe->Enemy->GetCurrentTarget() == nullptr);
				Probe->Stage = 3;
			}
			else if (Probe->Stage == 3 && Probe->Enemy->HasReachedObjective() && Battle.GetProtectedObjectHP(TEXT("PolygonCastle")) < 100)
			{
				T->TestTrue(TEXT("Route continuation reaches real fallback objective"), Probe->Enemy->GetActorLocation().Equals(Probe->Contact, 0.1));
				T->TestFalse(TEXT("Recovery before authored 25 game seconds is rejected"), Recovering->Resurrect());
				Recovering->SetActorLocation(Probe->Contact); // Dead until its player-issued resurrection.
				Mode->MarkCombatTargetsDirty();
				Probe->LastCastleHP = Battle.GetProtectedObjectHP(TEXT("PolygonCastle"));
				Probe->Stage = 4;
			}
			else if (Probe->Stage == 4)
			{
				if (!Probe->bCheckedLateCooldown && Now - Probe->DeathAt >= 24.5 && Now - Probe->DeathAt < 25)
				{
					T->TestFalse(TEXT("Author cooldown cannot finish half a second early"), Recovering->Resurrect());
					Probe->bCheckedLateCooldown = true;
				}
				const int32 CastleHP = Battle.GetProtectedObjectHP(TEXT("PolygonCastle"));
				// Revive just after a completed objective hit, so there is no already-committed
				// fallback swing to confuse the next-target priority assertion.
				if (Now - Probe->DeathAt > 25.1 && CastleHP < Probe->LastCastleHP)
				{
					T->TestTrue(TEXT("Full authored 25-second cooldown actually elapsed"), Now - Probe->DeathAt >= 25);
					Probe->SuppliesBeforeRevival = Battle.GetSupplies();
					T->TestTrue(TEXT("Player command resurrects for free after cooldown"), Recovering->Resurrect());
					T->TestEqual(TEXT("Manual resurrection restores full authored HP"), Recovering->GetHP(), 15);
					T->TestEqual(TEXT("Manual resurrection never charges supplies"), Battle.GetSupplies(), Probe->SuppliesBeforeRevival);
					T->TestFalse(TEXT("Repeated resurrection of a living unit is rejected"), Recovering->Resurrect());
					T->TestTrue(TEXT("Revived defender is immediately vulnerable"), Recovering->ApplyDamage(1));
					T->TestEqual(TEXT("Immediate post-revival damage is not shielded"), Recovering->GetHP(), 14);
					Probe->CastleAtRevival = CastleHP; Probe->StageAt = Now; Probe->Stage = 5;
				}
				Probe->LastCastleHP = CastleHP;
			}
			else if (Probe->Stage == 5)
			{
				// Cancel only the defender's swings in this harness, allowing the goblin's
				// real timer to prove targeting priority before the defender kills it.
				Recovering->StopCombat();
				if (Recovering->GetHP() < 14)
				{
					T->TestTrue(TEXT("Objective-contact enemy prioritizes a revived nearby ally"), Probe->Enemy->GetCurrentTarget() == Recovering);
					T->TestEqual(TEXT("Real enemy attack damages revived defender"), Recovering->GetHP(), 13);
					T->TestEqual(TEXT("Objective is not attacked while live defender is available"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), Probe->CastleAtRevival);
					T->TestTrue(TEXT("Second death still retains unit for automatic wave recovery"), Recovering->ApplyDamage(MAX_int32));
					TArray<ADYKEnemyActor*> Enemies;
					for (TActorIterator<ADYKEnemyActor> It(World); It; ++It) Enemies.Add(*It);
					for (auto* Enemy : Enemies) T->TestTrue(TEXT("Every real enemy is removed by damage"), Enemy->ApplyDamage(MAX_int32));
					Probe->Stage = 6;
				}
			}
			else if (Probe->Stage == 6 && Battle.GetPhase() == EDYKBattlePhase::InterwavePreparation)
			{
				T->TestEqual(TEXT("Wave boundary automatically restores dead blocker"), Probe->Blocker->GetHP(), 15);
				T->TestEqual(TEXT("Wave boundary ignores a fresh death cooldown"), Recovering->GetHP(), 15);
				T->TestEqual(TEXT("Wave boundary fully heals surviving defender"), Probe->Survivor->GetHP(), 15);
				T->TestTrue(TEXT("Automatic recovery leaves same occupant in same slot"), Probe->RecoverySlot->GetOccupant() == Recovering);
				T->TestTrue(TEXT("Recovered units allow next wave"), Battle.CanStartWave());
				T->TestEqual(TEXT("Only the fixed wave reward changes supplies"), Battle.GetSupplies(), Probe->SuppliesBeforeRevival + 10);
				Battle.Abort(Now);
				if (T->TestNotNull(TEXT("Recovery scenario has final result"), Battle.GetResult()))
					T->TestEqual(TEXT("Three automatic enemies and one route enemy count once"), Battle.GetResult()->EnemiesKilled, 4);
				return true;
			}
			return false;
		}
	private:
		TSharedRef<FDefenderRecoveryProbe> Probe;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKDefenderRecoveryTest, "DYK.M108.RuntimeDeathRecoveryAndEnemyPriority",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDYKDefenderRecoveryTest::RunTest(const FString& Parameters)
{
	auto* Mode = RequireDefenderWorld(this);
	if (!Mode) return false;
	auto* World = Mode->GetWorld(); auto& Battle = Mode->GetBattleSession();
	const auto Probe = MakeShared<FDefenderRecoveryProbe>();
	Probe->Test = this; Probe->Mode = Mode;
	auto* BlockerSlot = MakeDefenderSlot(World, TEXT("M108RouteBlocker"), FVector(-20000,0,100));
	auto* RecoverySlot = MakeDefenderSlot(World, TEXT("M108Recovery"), FVector(-20000,1000,100));
	auto* SurvivorSlot = MakeDefenderSlot(World, TEXT("M108Survivor"), FVector(-20000,2000,100));
	Probe->RecoverySlot = RecoverySlot;
	Probe->Blocker = Mode->PlaceDefender(BlockerSlot, Battle.GetPreparationToken());
	Probe->Recovering = Mode->PlaceDefender(RecoverySlot, Battle.GetPreparationToken());
	Probe->Survivor = Mode->PlaceDefender(SurvivorSlot, Battle.GetPreparationToken());
	if (!TestTrue(TEXT("Recovery fixtures use three actual placed units"), Probe->Blocker.IsValid() && Probe->Recovering.IsValid() && Probe->Survivor.IsValid())) return false;
	if (!TestTrue(TEXT("Recovery starts an actual wave"), Battle.StartWave(Battle.GetPreparationToken(), World->GetTimeSeconds()))) return false;
	Probe->DeathAt = World->GetTimeSeconds();
	TestTrue(TEXT("Recovery fixture receives real lethal damage"), Probe->Recovering->ApplyDamage(MAX_int32));
	TestTrue(TEXT("Survivor fixture receives real nonlethal damage"), Probe->Survivor->ApplyDamage(4));
	TestEqual(TEXT("Survivor keeps per-instance damage during wave"), Probe->Survivor->GetHP(), 11);
	TestFalse(TEXT("Immediate manual resurrection is rejected"), Probe->Recovering->Resurrect());
	const FVector BlockerPosition = Probe->Blocker->GetActorLocation();
	Probe->Contact = BlockerPosition + FVector(500,0,0);
	auto* Route = MakeDefenderRoute(World, TEXT("M108StopResume"), BlockerPosition - FVector(350,0,0), Probe->Contact);
	Probe->Enemy = Mode->SpawnEnemy(Route, Battle.GetWaveToken(), TEXT("MainEntry"));
	if (!TestTrue(TEXT("Recovery route has an actual moving goblin"), Probe->Enemy.IsValid())) return false;
	AddCommand(new FDefenderRecoveryCommand(Probe));
	return true;
}

namespace
{
	struct FDefenderBoundaryProbe
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ADYKPolygonGameMode> Mode;
		TWeakObjectPtr<ADYKDefenderActor> Defender;
		TWeakObjectPtr<ADYKEnemyActor> Enemy;
		FVector EnterPosition;
		FRotator IdleFacing;
		double Deadline = FPlatformTime::Seconds() + 20, FirstSwingAt = -1, EnteredAt = 0;
		uint64 EnteredFrame = 0;
		int32 Stage = 0;
		bool bEntered = false, bCheckedEntryFrame = false;
		FDelegateHandle PreTickHandle, PostTickHandle;
		void Cleanup()
		{
			FWorldDelegates::OnWorldPreActorTick.Remove(PreTickHandle);
			FWorldDelegates::OnWorldPostActorTick.Remove(PostTickHandle);
		}
	};

	class FDefenderBoundaryCommand : public IAutomationLatentCommand
	{
	public:
		explicit FDefenderBoundaryCommand(TSharedRef<FDefenderBoundaryProbe> State) : Probe(State) {}
		virtual bool Update() override
		{
			auto* T = Probe->Test;
			if (!Probe->Mode.IsValid() || !Probe->Defender.IsValid() || !Probe->Enemy.IsValid() || FPlatformTime::Seconds() > Probe->Deadline)
			{
				T->AddError(TEXT("Same-frame target/withdrawal runtime timed out or lost its actual actors.")); Probe->Cleanup(); return true;
			}
			auto* Mode = Probe->Mode.Get(); auto* Defender = Probe->Defender.Get(); auto* Enemy = Probe->Enemy.Get();
			auto& Battle = Mode->GetBattleSession(); const double Now = Mode->GetWorld()->GetTimeSeconds();
			if (Probe->Stage == 0 && Probe->bCheckedEntryFrame && Now - Probe->EnteredAt > 0.45)
			{
				T->TestEqual(TEXT("Same-frame entrant receives the real next impact"), Defender->GetHP(), 14);
				T->TestEqual(TEXT("Stale objective pool cannot schedule a second fallback hit"), Battle.GetProtectedObjectHP(TEXT("PolygonCastle")), 99);
				T->TestTrue(TEXT("Defender turned away from authored idle direction while attacking"), !Defender->GetActorRotation().Equals(Probe->IdleFacing, 0.1));
				T->TestTrue(TEXT("Session can withdraw a live enemy without Actor destruction"), Battle.RemoveEnemy(Battle.GetWaveToken(), Enemy->GetEnemyId(), false));
				Mode->MarkCombatTargetsDirty();
				T->TestTrue(TEXT("Withdrawn enemy Actor is still physically alive"), Enemy->IsAlive());
				T->TestTrue(TEXT("Withdrawn live actor immediately ceases to be an eligible target"), Defender->GetCurrentTarget() == nullptr);
				T->TestFalse(TEXT("Withdrawn enemy cannot receive combat damage"), Enemy->ApplyDamage(1));
				Probe->Stage = 1;
			}
			else if (Probe->Stage == 1)
			{
				T->TestTrue(TEXT("Post-step registry removes session-withdrawn actor"), Defender->GetCurrentTarget() == nullptr);
				T->TestTrue(TEXT("Stationary defender restores exact authored idle facing"), Defender->GetActorRotation().Equals(Probe->IdleFacing, 0.1));
				T->TestTrue(TEXT("Idle facing restoration never moves unit"), Defender->GetActorLocation().Equals(Probe->EnterPosition, 0.1));
				Battle.Abort(Now);
				if (T->TestNotNull(TEXT("Boundary regression returns immutable outcome"), Battle.GetResult()))
					T->TestEqual(TEXT("Withdrawal is not an enemy kill"), Battle.GetResult()->EnemiesKilled, 0);
				Probe->Cleanup(); return true;
			}
			return false;
		}
	private:
		TSharedRef<FDefenderBoundaryProbe> Probe;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKDefenderBoundaryTest, "DYK.M108.RuntimeSameFrameEntryAndWithdrawal",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDYKDefenderBoundaryTest::RunTest(const FString& Parameters)
{
	auto* Mode = RequireDefenderWorld(this);
	if (!Mode) return false;
	auto* World = Mode->GetWorld(); auto& Battle = Mode->GetBattleSession();
	const auto Probe = MakeShared<FDefenderBoundaryProbe>(); Probe->Test = this; Probe->Mode = Mode;
	auto* Slot = MakeDefenderSlot(World, TEXT("M108Boundary"), FVector(-20000,1000,100));
	if (!TestNotNull(TEXT("Boundary regression has an authored slot"), Slot)) return false;
	Probe->IdleFacing = FRotator(0,137,0); Slot->SetActorRotation(Probe->IdleFacing);
	Probe->Defender = Mode->PlaceDefender(Slot, Battle.GetPreparationToken());
	if (!TestTrue(TEXT("Boundary regression uses a purchased live defender"), Probe->Defender.IsValid())) return false;
	if (!TestTrue(TEXT("Boundary regression starts an actual wave"), Battle.StartWave(Battle.GetPreparationToken(), World->GetTimeSeconds()))) return false;
	const FVector Contact(-20000,0,100); Probe->EnterPosition = Contact - FVector(0,100,0);
	Probe->Enemy = SpawnContactGoblin(Mode, TEXT("M108BoundaryContact"), Contact);
	if (!TestTrue(TEXT("Boundary regression has an objective-contact goblin"), Probe->Enemy.IsValid())) return false;
	// UE5.5 advances game time before PreActorTick, ticks TimerManager after actor
	// ticks, then broadcasts PostActorTick. Enter on the first repeat's due frame.
	Probe->PreTickHandle = FWorldDelegates::OnWorldPreActorTick.AddLambda([Probe](UWorld* TickedWorld, ELevelTick, float)
	{
		if (!Probe->Mode.IsValid() || TickedWorld != Probe->Mode->GetWorld() || Probe->FirstSwingAt < 0) return;
		const double Now = TickedWorld->GetTimeSeconds();
		if (Probe->bEntered)
		{
			if (!Probe->bCheckedEntryFrame && GFrameCounter > Probe->EnteredFrame)
			{
				// Multicast broadcast order is unspecified (UE5.5 traverses backwards).
				// This is before any actors/timers in the next frame, after ALL of the
				// previous frame's post-step handlers have completed.
				Probe->Test->TestTrue(TEXT("FIFO observation is in the next frame before logical impact"),
					GFrameCounter == Probe->EnteredFrame + 1 && Now - Probe->EnteredAt < 0.3);
				Probe->Test->TestTrue(TEXT("Completed entry step chooses refreshed ally before objective fallback"), Probe->Enemy->GetPendingTarget() == Probe->Defender.Get());
				Probe->Test->TestTrue(TEXT("Completed entry step FIFO contains newly available defender"), Probe->Enemy->GetCurrentTarget() == Probe->Defender.Get());
				Probe->Test->AddInfo(FString::Printf(TEXT("Entry frame=%llu time=%.6f; observed next pre-actor frame=%llu time=%.6f before impact."),
					Probe->EnteredFrame, Probe->EnteredAt, GFrameCounter, Now));
				Probe->bCheckedEntryFrame = true;
			}
			return;
		}
		if (Now < Probe->FirstSwingAt + 2.0) return;
		Probe->bEntered = true; Probe->EnteredAt = Now; Probe->EnteredFrame = GFrameCounter;
		Probe->Defender->SetActorLocation(Probe->EnterPosition);
		Probe->Mode->MarkCombatTargetsDirty();
	});
	// Capture the first post-step's game time; handler ordering does not affect that time.
	Probe->PostTickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Probe](UWorld* TickedWorld, ELevelTick, float)
	{
		if (!Probe->Mode.IsValid() || TickedWorld != Probe->Mode->GetWorld()) return;
		if (Probe->FirstSwingAt < 0) Probe->FirstSwingAt = TickedWorld->GetTimeSeconds();
	});
	AddCommand(new FDefenderBoundaryCommand(Probe));
	return true;
}

#endif
