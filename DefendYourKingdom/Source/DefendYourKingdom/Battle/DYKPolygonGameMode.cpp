#include "DYKPolygonGameMode.h"

#include "../DefendYourKingdom.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	FDYKBattleDefinition PolygonDEV_PLACEHOLDER()
	{
		// Replace with authored mission data when available. No automatic combat or victory.
		FDYKBattleDefinition Definition;
		Definition.InitialSupplies = 20;
		Definition.ProtectedObjects.Add({ TEXT("PolygonCastle"), 100, true });
		Definition.Waves.Add({ { TEXT("MainEntry") }, 10 });
		Definition.Waves.Add({ { TEXT("MainEntry") }, 10 });
		return Definition;
	}
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
	BattleSession.OnWaveStarted.AddLambda([](const FGuid& Token)
	{
		UE_LOG(LogDYK, Log, TEXT("Battle wave started: %s"), *Token.ToString());
	});
	BattleSession.OnFinished.AddLambda([](const FDYKBattleResult& Result)
	{
		UE_LOG(LogDYK, Log, TEXT("Battle finished once: attempt=%s outcome=%d waves=%d kills=%d activeSeconds=%.3f"),
			*Result.AttemptId.ToString(), static_cast<int32>(Result.Outcome), Result.CompletedWaves,
			Result.EnemiesKilled, Result.ActiveSeconds);
	});
	UE_LOG(LogDYK, Log, TEXT("Polygon battle ready in initial preparation (DEV_PLACEHOLDER mission data)."));
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
	BattleSession.Abort(GetWorld()->GetTimeSeconds());
	BattleSession.OnResolutionRequested.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}
