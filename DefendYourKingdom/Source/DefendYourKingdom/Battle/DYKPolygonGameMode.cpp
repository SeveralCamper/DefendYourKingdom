#include "DYKPolygonGameMode.h"
#include "DYKEnemyActor.h"
#include "DYKEnemyRoute.h"
#include "DYKCombatUnit.h"
#include "DYKDefenderActor.h"
#include "DYKUnitSlot.h"
#include "DYKPlacementWidget.h"
#include "DYKCombatRules.h"
#include "../Core/DYKGameInstance.h"
#include "../Meta/DYKMissionRewards.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "Kismet/GameplayStatics.h"

#include "../DefendYourKingdom.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"

namespace
{
	FDYKBattleDefinition PolygonDEV_PLACEHOLDER()
	{
		// Replace with authored mission data when available.
		FDYKBattleDefinition Definition;
		Definition.InitialSupplies = 20;
		Definition.ProtectedObjects.Add({ TEXT("PolygonCastle"), 100, true });
		Definition.Waves.Add({ { TEXT("MainEntry") }, 10 });
		Definition.Waves.Add({ { TEXT("MainEntry") }, 10 });
		return Definition;
	}
	// Temporary wave composition and release cadence, separate from the unit passport.
	constexpr int32 EnemyCountDEV_PLACEHOLDER = 3;
	constexpr float ReleaseSecondsDEV_PLACEHOLDER = 1.f;
}

ADYKPolygonGameMode::ADYKPolygonGameMode()
{
	BattleSession.OnWaveCompleted.AddUObject(this,&ThisClass::RestoreDefenders);
}

void ADYKPolygonGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (!StartupError.IsEmpty()) return;
	if (!BattleSession.Initialize(PolygonDEV_PLACEHOLDER(), StartupError))
	{
		ErrorMessage = StartupError;
		SpectatorClass = nullptr;
		UE_LOG(LogDYK, Error, TEXT("Battle startup blocked: %s"), *StartupError);
		return;
	}
	BattleSession.OnResolutionRequested.AddUObject(this, &ThisClass::QueueWaveResolution);
	BattleSession.OnWaveStarted.AddUObject(this, &ThisClass::QueueEmission);
	CombatStepHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::RefreshCombatTargets);
	BattleSession.OnWaveStarted.AddLambda([](const FGuid& Token)
	{
		UE_LOG(LogDYK, Log, TEXT("Battle wave started: %s"), *Token.ToString());
	});
	BattleSession.OnFinished.AddLambda([this](const FDYKBattleResult& Result)
	{
		StopEnemies();
		SettleMissionResult();
		UE_LOG(LogDYK, Log, TEXT("Battle finished once: attempt=%s outcome=%d waves=%d kills=%d activeSeconds=%.3f"),
			*Result.AttemptId.ToString(), static_cast<int32>(Result.Outcome), Result.CompletedWaves,
			Result.EnemiesKilled, Result.ActiveSeconds);
	});
	UE_LOG(LogDYK, Log, TEXT("Polygon battle ready in initial preparation (DEV_PLACEHOLDER mission data)."));
}

bool ADYKPolygonGameMode::SettleMissionResult()
{
	if (bMissionResultSaved) return true;
	const auto* Result = BattleSession.GetResult();
	if (!Result) return false;
	MissionResultError.Reset();
	if (Result->Outcome != EDYKBattlePhase::Victory)
	{
		bMissionResultSaved = true;
		return true;
	}
	auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto& Reward = DYKMissionRewards::PolygonDEV_PLACEHOLDER();
	const auto* Before = Save ? Save->GetProfileSnapshot() : nullptr;
	const bool bPreviouslyIssued = Before && Before->MissionRewards.ContainsByPredicate([&](const auto& R) { return R.RewardId == Reward.RewardId; });
	TArray<int64> Balances;
	for (const auto& Row : Reward.Resources)
	{
		const auto Id = FDYKResourceId::FromContentId(Row.ResourceId);
		Balances.Add(Before && Id ? DYKCityEconomy::Balance(*Before, Id.GetValue()) : 0);
	}
	bMissionResultSaved = Save && DYKMissionRewards::Settle(*Save, *Result, Reward);
	if (bMissionResultSaved && !bPreviouslyIssued)
	{
		const auto* After = Save->GetProfileSnapshot();
		for (int32 I = 0; I < Reward.Resources.Num(); ++I)
		{
			const auto& Row = Reward.Resources[I];
			const auto Id = FDYKResourceId::FromContentId(Row.ResourceId);
			const int64 Delivered = Id ? DYKCityEconomy::Balance(*After, Id.GetValue()) - Balances[I] : 0;
			if (Delivered > 0) AwardedResources.Add({Row.ResourceId, Delivered});
		}
		if (const auto* Receipt = After->MissionRewards.FindByPredicate([&](const auto& R) { return R.RewardId == Reward.RewardId; })) DeferredResources = Receipt->Remaining;
	}
	if (!bMissionResultSaved) MissionResultError = TEXT("Не удалось сохранить награду. Повторите возврат в город.");
	return bMissionResultSaved;
}

bool ADYKPolygonGameMode::ReturnToCity(FString& OutError)
{
	OutError.Reset();
	const auto* Result = BattleSession.GetResult();
	if (!Result || (Result->Outcome != EDYKBattlePhase::Victory && Result->Outcome != EDYKBattlePhase::Defeat))
	{
		OutError = TEXT("Возврат из результата доступен после победы или поражения.");
		return false;
	}
	if (!SettleMissionResult()) { OutError = MissionResultError; return false; }
	auto* Session = GetGameInstance<UDYKGameInstance>();
	if (!Session) { OutError = TEXT("Локальная сессия недоступна."); return false; }
	return Session->RequestLevelTravel(EDYKLocalLevel::City, OutError);
}

void ADYKPolygonGameMode::StartPlay()
{
	Super::StartPlay();
	if (!StartupError.IsEmpty()) return;
	if (auto* PC = GetWorld()->GetFirstPlayerController(); PC && PC->IsLocalController())
		if (auto* Controls = CreateWidget<UDYKPlacementWidget>(PC, DYKUI::ScreenClass(UDYKPlacementWidget::StaticClass(), TEXT("WBP_Battle")))) Controls->AddToViewport();
}

bool ADYKPolygonGameMode::IsEditableSlot(const ADYKUnitSlot* Slot) const
{
	if (!IsValid(Slot) || Slot->IsActorBeingDestroyed() || Slot->GetWorld() != GetWorld() ||
		!Slot->bPlayerEditable || Slot->SlotId.IsNone()) return false;
	// Duplicate authored identifiers are invalid rather than selecting an arbitrary slot.
	for (TActorIterator<ADYKUnitSlot> It(GetWorld()); It; ++It)
		if (*It != Slot && It->SlotId == Slot->SlotId) return false;
	return true;
}

ADYKDefenderActor* ADYKPolygonGameMode::PlaceDefender(ADYKUnitSlot* Slot, const FGuid& PreparationToken, bool bPaladin)
{
	if (!IsEditableSlot(Slot) || Slot->GetOccupant() || !BattleSession.IsPreparation() ||
		PreparationToken != BattleSession.GetPreparationToken() || UGameplayStatics::IsGamePaused(this)) return nullptr;
	if(bPaladin && !CanDeployHero(DYKUnitIds::Paladin().GetName())) return nullptr;
	const int32 Cost = DYKUnitCatalog::Find(bPaladin ? DYKUnitIds::Paladin() : DYKUnitIds::Peasant())->Base.PlacementSupplies.Value;
	if (BattleSession.GetSupplies() < Cost) return nullptr;
	auto* Unit = GetWorld()->SpawnActor<ADYKDefenderActor>(Slot->GetActorLocation(), Slot->GetActorRotation());
	if (!Unit) return nullptr;
	if(bPaladin) Unit->HeroType=DYKUnitIds::Paladin().GetName();
	const FGuid Id = FGuid::NewGuid();
	if(!Unit->Initialize(this,Id)) {Unit->Destroy(); return nullptr;}
	if (!BattleSession.PurchaseDefense(Id, Cost, PreparationToken)) { Unit->Destroy(); return nullptr; }
	Slot->Occupant = Unit;
	Slot->UpdateMarker();
	return Unit;
}

bool ADYKPolygonGameMode::CanDeployHero(FName Hero,bool bStory) const
{
	TArray<FName> Player,Story;
	for(const auto& Weak:CombatUnits) if(const auto* Unit=Cast<ADYKDefenderActor>(Weak.Get()); Unit && !Unit->IsActorBeingDestroyed() && Unit->IsHero())
		(Unit->bMissionPlaced ? Story : Player).Add(Unit->HeroType);
	if(!DYKHeroes::CanDeploy(Hero,Player,Story,bStory)) return false;
	if(bStory) return true;
	const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P=Save ? Save->GetProfileSnapshot() : nullptr;
	return bAllowPlayerHeroes && (AllowedHeroIds.IsEmpty() || AllowedHeroIds.Contains(Hero))
		&& Hero==DYKUnitIds::Paladin().GetName() && P && DYKHeroes::Owns(*P);
}

void ADYKPolygonGameMode::ApplyHeroAreaDamage(FVector Center,float Radius,int32 Damage,EDYKDamageType Type)
{
	if(BattleSession.GetPhase()!=EDYKBattlePhase::ActiveWave || UGameplayStatics::IsGamePaused(this)) return;
	// Snapshot recipients before death callbacks update the registry.
	const auto Units=CombatUnits;
	for(const auto& Weak:Units) if(auto* Unit=Weak.Get(); Unit && Unit->IsAlive() && Unit->GetUnitDefinition()
		&& Unit->GetUnitDefinition()->Side==EDYKUnitSide::Enemy
		&& FVector::DistSquared(Unit->GetActorLocation(),Center)<=FMath::Square(Radius*DYKCombatRules::CentimetersPerUnitDEV_PLACEHOLDER))
		Unit->ApplyDamage(Damage,Type);
}

bool ADYKPolygonGameMode::SellDefender(ADYKUnitSlot* Slot, const FGuid& PreparationToken)
{
	if (!IsEditableSlot(Slot) || !IsValid(Slot->GetOccupant()) || UGameplayStatics::IsGamePaused(this) ||
		!BattleSession.SellDefense(Slot->GetOccupant()->GetDefenseId(), PreparationToken)) return false;
	auto* Unit = Slot->GetOccupant();
	Slot->Occupant = nullptr;
	Slot->UpdateMarker();
	Unit->StopCombat();
	Unit->Destroy();
	MarkCombatTargetsDirty();
	return true;
}

void ADYKPolygonGameMode::RegisterCombatUnit(ADYKCombatUnit* Unit)
{
	if (IsValid(Unit) && Unit->GetWorld() == GetWorld()) CombatUnits.AddUnique(Unit);
	MarkCombatTargetsDirty();
}

void ADYKPolygonGameMode::RefreshCombatTargets(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld() || !bCombatTargetsDirty || UGameplayStatics::IsGamePaused(this)) return;
	bCombatTargetsDirty = false;
	CombatUnits.RemoveAll([](const auto& Unit) { return !Unit.IsValid() || Unit->IsActorBeingDestroyed(); });
	// Immediate attacks can destroy later entries or register new units during this pass.
	// Retain a stable iteration and resolve each weak pointer immediately before use.
	const auto Units=CombatUnits;
	TArray<ADYKCombatUnit*> Candidates;
	for (const auto& Weak : Units) if (auto* Unit=Weak.Get(); IsValid(Unit) && Unit->IsAlive()) Candidates.Add(Unit);
	// Movement/spawn/death/revival mark the registry dirty. Sample once after the game
	// step, so simultaneous entrants share a stable registration-order tie-break.
	for (const auto& Weak : Units)
		if (auto* Unit=Weak.Get(); IsValid(Unit) && !Unit->IsActorBeingDestroyed()) Unit->RefreshTargets(Candidates);
}

void ADYKPolygonGameMode::RestoreDefenders(int32 WaveNumber)
{
	for (const auto& Unit : CombatUnits)
		if (auto* Defender = Cast<ADYKDefenderActor>(Unit.Get())) Defender->RestoreAfterWave();
	MarkCombatTargetsDirty();
}

ADYKEnemyActor* ADYKPolygonGameMode::SpawnEnemy(ADYKEnemyRoute* Route, const FGuid& WaveToken, FName Group)
{
	if (!IsValid(Route) || !BattleSession.IsSpawnGroupPending(WaveToken, Group)) return nullptr;
	auto* Enemy = GetWorld()->SpawnActor<ADYKEnemyActor>();
	if (Enemy && !Enemy->Initialize(this, Route, WaveToken, Group))
	{
		Enemy->Destroy();
		return nullptr;
	}
	return Enemy;
}

void ADYKPolygonGameMode::QueueEmission(const FGuid& WaveToken)
{
	EmittedEnemies = 0;
	MarkCombatTargetsDirty();
	// Session notifications reject reentrant commands. Release only after dispatch returns.
	EmissionTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WaveToken]() { EmitEnemy(WaveToken); }));
}

void ADYKPolygonGameMode::EmitEnemy(const FGuid& WaveToken)
{
	const FName Group(TEXT("MainEntry"));
	if (!BattleSession.IsSpawnGroupPending(WaveToken, Group)) return;
	ADYKEnemyRoute* AssignedRoute = nullptr;
	for (TActorIterator<ADYKEnemyRoute> It(GetWorld()); It; ++It)
	{
		if (It->RouteId == TEXT("PolygonMain"))
		{
			if (AssignedRoute) { AssignedRoute = nullptr; break; }
			AssignedRoute = *It;
		}
	}
	if (!SpawnEnemy(AssignedRoute, WaveToken, Group))
	{
		UE_LOG(LogDYK, Error, TEXT("Enemy emission failed: require one valid authored PolygonMain route and a live objective."));
		BattleSession.Abort(GetWorld()->GetTimeSeconds());
		return;
	}
	if (++EmittedEnemies == EnemyCountDEV_PLACEHOLDER)
	{
		BattleSession.CompleteSpawnGroup(WaveToken, Group);
		return;
	}
	GetWorldTimerManager().SetTimer(EmissionTimer, FTimerDelegate::CreateWeakLambda(this, [this, WaveToken]() { EmitEnemy(WaveToken); }), ReleaseSecondsDEV_PLACEHOLDER, false);
}

void ADYKPolygonGameMode::StopEnemies()
{
	GetWorldTimerManager().ClearTimer(EmissionTimer);
	for (const auto& Unit : CombatUnits) if (Unit.IsValid()) Unit->StopCombat();
}

void ADYKPolygonGameMode::QueueWaveResolution(const FGuid& WaveToken)
{
	// SetTimerForNextTick can run later in this SAME frame (UE5.5). Enforce a frame
	// boundary so late actor ticks/timers can still report a destroyed required object.
	const uint64 RequestedFrame = GFrameCounter;
	ResolutionTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WaveToken, RequestedFrame]()
	{
		if (GFrameCounter == RequestedFrame)
		{
			QueueWaveResolution(WaveToken);
			return;
		}
		BattleSession.ResolveWave(WaveToken, GetWorld()->GetTimeSeconds());
	}));
}

void ADYKPolygonGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::OnWorldPostActorTick.Remove(CombatStepHandle);
	GetWorldTimerManager().ClearTimer(ResolutionTimer);
	StopEnemies();
	BattleSession.Abort(GetWorld()->GetTimeSeconds());
	BattleSession.OnResolutionRequested.RemoveAll(this);
	BattleSession.OnWaveCompleted.RemoveAll(this);
	BattleSession.OnWaveStarted.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}
