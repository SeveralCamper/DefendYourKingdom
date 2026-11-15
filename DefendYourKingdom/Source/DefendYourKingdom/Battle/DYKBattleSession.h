#pragma once

#include "CoreMinimal.h"

enum class EDYKBattlePhase : uint8
{
	Uninitialized, InitialPreparation, ActiveWave, InterwavePreparation, Victory, Defeat, Aborted
};

struct FDYKProtectedObjectDefinition
{
	FName Id;
	int32 MaxHP = 0;
	bool bRequired = true;
};

struct FDYKWaveDefinition
{
	// Include every entry/group and reserved event reinforcement before starting.
	TArray<FName> SpawnGroups;
	int32 CompletionSupplies = 10;
};

struct FDYKBattleDefinition
{
	TArray<FDYKProtectedObjectDefinition> ProtectedObjects;
	TArray<FDYKWaveDefinition> Waves;
	int32 InitialSupplies = 0;
};

struct FDYKBattleResult
{
	FGuid AttemptId;
	EDYKBattlePhase Outcome = EDYKBattlePhase::Uninitialized;
	int32 CompletedWaves = 0;
	int32 EnemiesKilled = 0;
	double ActiveSeconds = 0;
};

// One local attempt owned by the loaded battle world. All commands run on the game thread.
// No profile writes/rewards here: M1.09 consumes the immutable result and its AttemptId.
class DEFENDYOURKINGDOM_API FDYKBattleSession
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FWaveStarted, const FGuid&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FWaveCompleted, int32);
	DECLARE_MULTICAST_DELEGATE_OneParam(FFinished, const FDYKBattleResult&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FResolutionRequested, const FGuid&);

	bool Initialize(const FDYKBattleDefinition& Definition, FString& OutError);
	bool AddDefense(const FGuid& DefenseId);
	bool RemoveDefense(const FGuid& DefenseId);
	bool PurchaseDefense(const FGuid& DefenseId, int32 Cost, const FGuid& PreparationToken);
	bool SellDefense(const FGuid& DefenseId, const FGuid& PreparationToken);
	bool CanStartWave() const;
	// Token identifies this preparation; a queued old click cannot start a later wave.
	bool StartWave(const FGuid& PreparationToken, double GameSeconds);
	bool RegisterEnemy(const FGuid& WaveToken, FName GroupId, const FGuid& EnemyId);
	bool CompleteSpawnGroup(const FGuid& WaveToken, FName GroupId);
	bool RemoveEnemy(const FGuid& WaveToken, const FGuid& EnemyId, bool bKilled);
	bool DamageProtectedObject(const FGuid& WaveToken, FName ObjectId, int32 Damage, double GameSeconds);
	bool Abort(double GameSeconds);
	// World owner calls at the next frame boundary, after this frame's damage/death batch.
	bool ResolveWave(const FGuid& WaveToken, double GameSeconds);

	EDYKBattlePhase GetPhase() const { return Phase; }
	const FGuid& GetPreparationToken() const { return PreparationId; }
	const FGuid& GetWaveToken() const { return WaveId; }
	const FGuid& GetAttemptId() const { return AttemptId; }
	int32 GetWaveNumber() const { return WaveIndex + 1; }
	int32 GetSupplies() const { return Supplies; }
	int32 GetProtectedObjectHP(FName ObjectId) const;
	int32 GetActiveEnemyCount() const { return ActiveEnemies.Num(); }
	int32 GetPendingGroupCount() const { return PendingGroups.Num(); }
	bool IsSpawnGroupPending(const FGuid& Token, FName Group) const { return IsActiveToken(Token) && PendingGroups.Contains(Group); }
	bool IsEnemyActive(const FGuid& Token, const FGuid& Enemy) const { return IsActiveToken(Token) && ActiveEnemies.Contains(Enemy); }
	const FDYKBattleResult* GetResult() const { return IsTerminal() ? &Result : nullptr; }
	bool IsPreparation() const;
	bool IsTerminal() const;

	// Notifications observe committed state. Reentrant commands are rejected;
	// a spawner schedules work after OnWaveStarted returns, rather than inside it.
	FWaveStarted OnWaveStarted;
	FWaveCompleted OnWaveCompleted;
	FFinished OnFinished;
	FResolutionRequested OnResolutionRequested;

private:
	bool IsActiveToken(const FGuid& Token) const;
	bool IsWaveClear() const;
	void RequestResolution();
	void Finish(EDYKBattlePhase Outcome, double GameSeconds, bool bWaveCompleted = false);
	FDYKBattleDefinition Config;
	EDYKBattlePhase Phase = EDYKBattlePhase::Uninitialized;
	FGuid AttemptId, PreparationId, WaveId;
	int32 WaveIndex = -1;
	int32 CompletedWaves = 0;
	int32 Supplies = 0;
	int32 EnemiesKilled = 0;
	double WaveStartedAt = 0;
	double ActiveSeconds = 0;
	bool bDispatching = false;
	bool bResolutionRequested = false;
	TMap<FName, int32> ProtectedHP;
	TSet<FGuid> Defenses, ActiveEnemies, SeenEnemies;
	TMap<FGuid, int32> PurchasedDefenses;
	TSet<FName> PendingGroups;
	FDYKBattleResult Result;
};
