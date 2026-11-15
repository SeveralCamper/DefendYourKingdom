#include "DYKPolygonGameMode.h"
#include "DYKEnemyActor.h"
#include "DYKEnemyRoute.h"

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
	BattleSession.OnWaveStarted.AddLambda([](const FGuid& Token)
	{
		UE_LOG(LogDYK, Log, TEXT("Battle wave started: %s"), *Token.ToString());
	});
	BattleSession.OnFinished.AddLambda([this](const FDYKBattleResult& Result)
	{
		StopEnemies();
		UE_LOG(LogDYK, Log, TEXT("Battle finished once: attempt=%s outcome=%d waves=%d kills=%d activeSeconds=%.3f"),
			*Result.AttemptId.ToString(), static_cast<int32>(Result.Outcome), Result.CompletedWaves,
			Result.EnemiesKilled, Result.ActiveSeconds);
	});
	UE_LOG(LogDYK, Log, TEXT("Polygon battle ready in initial preparation (DEV_PLACEHOLDER mission data)."));
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
	for (TActorIterator<ADYKEnemyActor> It(GetWorld()); It; ++It) It->StopCombat();
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
	GetWorldTimerManager().ClearTimer(ResolutionTimer);
	StopEnemies();
	BattleSession.Abort(GetWorld()->GetTimeSeconds());
	BattleSession.OnResolutionRequested.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}
