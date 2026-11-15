#pragma once

#include "../Core/DYKLocalGameMode.h"
#include "DYKBattleSession.h"
#include "DYKPolygonGameMode.generated.h"

class ADYKEnemyActor;
class ADYKEnemyRoute;

// Technical first-mission blockout; a fresh, transient attempt per loaded world.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKPolygonGameMode : public ADYKLocalGameMode
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FDYKBattleSession& GetBattleSession() { return BattleSession; }
	const FDYKBattleSession& GetBattleSession() const { return BattleSession; }
	ADYKEnemyActor* SpawnEnemy(ADYKEnemyRoute* Route, const FGuid& WaveToken, FName Group);

private:
	void QueueWaveResolution(const FGuid& WaveToken);
	void QueueEmission(const FGuid& WaveToken);
	void EmitEnemy(const FGuid& WaveToken);
	void StopEnemies();
	FDYKBattleSession BattleSession;
	FTimerHandle ResolutionTimer, EmissionTimer;
	int32 EmittedEnemies = 0;
};
