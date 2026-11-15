#pragma once

#include "../Core/DYKLocalGameMode.h"
#include "DYKBattleSession.h"
#include "DYKPolygonGameMode.generated.h"

class ADYKEnemyActor;
class ADYKEnemyRoute;
class ADYKCombatUnit;
class ADYKDefenderActor;
class ADYKUnitSlot;

// Technical first-mission blockout; a fresh, transient attempt per loaded world.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKPolygonGameMode : public ADYKLocalGameMode
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FDYKBattleSession& GetBattleSession() { return BattleSession; }
	const FDYKBattleSession& GetBattleSession() const { return BattleSession; }
	ADYKEnemyActor* SpawnEnemy(ADYKEnemyRoute* Route, const FGuid& WaveToken, FName Group);
	ADYKDefenderActor* PlaceDefender(ADYKUnitSlot* Slot, const FGuid& PreparationToken);
	bool SellDefender(ADYKUnitSlot* Slot, const FGuid& PreparationToken);
	void RegisterCombatUnit(ADYKCombatUnit* Unit);
	void MarkCombatTargetsDirty() { bCombatTargetsDirty = true; }
	bool SettleMissionResult();
	bool ReturnToCity(FString& OutError);
	bool IsMissionResultSaved() const { return bMissionResultSaved; }
	const FString& GetMissionResultError() const { return MissionResultError; }

private:
	void QueueWaveResolution(const FGuid& WaveToken);
	void QueueEmission(const FGuid& WaveToken);
	void EmitEnemy(const FGuid& WaveToken);
	void StopEnemies();
	bool IsEditableSlot(const ADYKUnitSlot* Slot) const;
	void RefreshCombatTargets(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void RestoreDefenders(int32 WaveNumber);
	FDYKBattleSession BattleSession;
	FTimerHandle ResolutionTimer, EmissionTimer;
	int32 EmittedEnemies = 0;
	bool bCombatTargetsDirty = false;
	TArray<TWeakObjectPtr<ADYKCombatUnit>> CombatUnits;
	FDelegateHandle CombatStepHandle;
	bool bMissionResultSaved = false;
	FString MissionResultError;
};
