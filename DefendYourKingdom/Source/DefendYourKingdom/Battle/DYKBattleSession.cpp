#include "DYKBattleSession.h"

bool FDYKBattleSession::Initialize(const FDYKBattleDefinition& Definition, FString& OutError)
{
	OutError.Reset();
	if (Phase != EDYKBattlePhase::Uninitialized || bDispatching)
	{
		OutError = TEXT("An attempt can only be initialized once; retry requires a new session.");
		return false;
	}
	bool bHasRequiredObject = false;
	TSet<FName> ObjectIds;
	bool bValid = !Definition.Waves.IsEmpty() && Definition.InitialSupplies >= 0;
	int64 MaximumSupplies = Definition.InitialSupplies;
	for (const FDYKProtectedObjectDefinition& Object : Definition.ProtectedObjects)
	{
		bValid &= !Object.Id.IsNone() && Object.MaxHP > 0 && !ObjectIds.Contains(Object.Id);
		ObjectIds.Add(Object.Id);
		bHasRequiredObject |= Object.bRequired;
	}
	for (int32 Index = 0; Index < Definition.Waves.Num(); ++Index)
	{
		const FDYKWaveDefinition& Wave = Definition.Waves[Index];
		TSet<FName> Groups;
		bValid &= !Wave.SpawnGroups.IsEmpty() && Wave.CompletionSupplies >= 0;
		for (const FName Group : Wave.SpawnGroups)
		{
			bValid &= !Group.IsNone() && !Groups.Contains(Group);
			Groups.Add(Group);
		}
		if (Index + 1 < Definition.Waves.Num())
		{
			MaximumSupplies += Wave.CompletionSupplies;
		}
	}
	if (!bValid || !bHasRequiredObject || MaximumSupplies > MAX_int32)
	{
		OutError = TEXT("Invalid mission: require unique positive-HP objectives, a required objective, waves/groups and safe nonnegative supplies.");
		return false;
	}
	Config = Definition;
	for (const FDYKProtectedObjectDefinition& Object : Config.ProtectedObjects)
	{
		ProtectedHP.Add(Object.Id, Object.MaxHP);
	}
	Supplies = Config.InitialSupplies;
	AttemptId = FGuid::NewGuid();
	PreparationId = FGuid::NewGuid();
	Phase = EDYKBattlePhase::InitialPreparation;
	return true;
}

bool FDYKBattleSession::IsPreparation() const
{
	return Phase == EDYKBattlePhase::InitialPreparation || Phase == EDYKBattlePhase::InterwavePreparation;
}

bool FDYKBattleSession::IsTerminal() const
{
	return Phase == EDYKBattlePhase::Victory || Phase == EDYKBattlePhase::Defeat || Phase == EDYKBattlePhase::Aborted;
}

bool FDYKBattleSession::AddDefense(const FGuid& DefenseId)
{
	if (bDispatching || !IsPreparation() || !DefenseId.IsValid() || Defenses.Contains(DefenseId)) return false;
	Defenses.Add(DefenseId);
	return true;
}

bool FDYKBattleSession::RemoveDefense(const FGuid& DefenseId)
{
	return !bDispatching && IsPreparation() && !PurchasedDefenses.Contains(DefenseId) && Defenses.Remove(DefenseId) > 0;
}

bool FDYKBattleSession::PurchaseDefense(const FGuid& DefenseId, int32 Cost, const FGuid& PreparationToken)
{
	if (bDispatching || !IsPreparation() || PreparationToken != PreparationId || Cost < 0 || Supplies < Cost ||
		!DefenseId.IsValid() || Defenses.Contains(DefenseId)) return false;
	Defenses.Add(DefenseId);
	PurchasedDefenses.Add(DefenseId, Cost);
	Supplies -= Cost;
	return true;
}

bool FDYKBattleSession::SellDefense(const FGuid& DefenseId, const FGuid& PreparationToken)
{
	const int32* Cost = PurchasedDefenses.Find(DefenseId);
	if (bDispatching || !IsPreparation() || PreparationToken != PreparationId || !Cost ||
		!Defenses.Contains(DefenseId) || int64(Supplies) + *Cost > MAX_int32) return false;
	Supplies += *Cost;
	PurchasedDefenses.Remove(DefenseId);
	Defenses.Remove(DefenseId);
	return true;
}

bool FDYKBattleSession::CanStartWave() const
{
	return IsPreparation() && !Defenses.IsEmpty();
}

bool FDYKBattleSession::StartWave(const FGuid& PreparationToken, double GameSeconds)
{
	if (bDispatching || !CanStartWave() || PreparationToken != PreparationId || !FMath::IsFinite(GameSeconds) || GameSeconds < 0) return false;
	++WaveIndex;
	WaveId = FGuid::NewGuid();
	PreparationId.Invalidate();
	PendingGroups.Reset();
	for (const FName Group : Config.Waves[WaveIndex].SpawnGroups) PendingGroups.Add(Group);
	ActiveEnemies.Reset();
	SeenEnemies.Reset();
	bResolutionRequested = false;
	WaveStartedAt = GameSeconds;
	Phase = EDYKBattlePhase::ActiveWave;
	TGuardValue<bool> Guard(bDispatching, true);
	OnWaveStarted.Broadcast(WaveId);
	return true;
}

bool FDYKBattleSession::IsActiveToken(const FGuid& Token) const
{
	return !bDispatching && Phase == EDYKBattlePhase::ActiveWave && Token.IsValid() && Token == WaveId;
}

bool FDYKBattleSession::RegisterEnemy(const FGuid& WaveToken, FName GroupId, const FGuid& EnemyId)
{
	if (!IsActiveToken(WaveToken) || !PendingGroups.Contains(GroupId) || !EnemyId.IsValid() || SeenEnemies.Contains(EnemyId)) return false;
	ActiveEnemies.Add(EnemyId);
	SeenEnemies.Add(EnemyId);
	return true;
}

bool FDYKBattleSession::CompleteSpawnGroup(const FGuid& WaveToken, FName GroupId)
{
	if (!IsActiveToken(WaveToken) || PendingGroups.Remove(GroupId) == 0) return false;
	RequestResolution();
	return true;
}

bool FDYKBattleSession::RemoveEnemy(const FGuid& WaveToken, const FGuid& EnemyId, bool bKilled)
{
	if (!IsActiveToken(WaveToken) || ActiveEnemies.Remove(EnemyId) == 0) return false;
	if (bKilled) ++EnemiesKilled;
	RequestResolution();
	return true;
}

bool FDYKBattleSession::IsWaveClear() const
{
	return Phase == EDYKBattlePhase::ActiveWave && PendingGroups.IsEmpty() && ActiveEnemies.IsEmpty();
}

void FDYKBattleSession::RequestResolution()
{
	if (IsWaveClear() && !bResolutionRequested)
	{
		bResolutionRequested = true;
		TGuardValue<bool> Guard(bDispatching, true);
		OnResolutionRequested.Broadcast(WaveId);
	}
}

bool FDYKBattleSession::DamageProtectedObject(const FGuid& WaveToken, FName ObjectId, int32 Damage, double GameSeconds)
{
	int32* HP = ProtectedHP.Find(ObjectId);
	if (!IsActiveToken(WaveToken) || !HP || *HP <= 0 || Damage <= 0 || !FMath::IsFinite(GameSeconds) || GameSeconds < WaveStartedAt) return false;
	*HP = FMath::Max(0, *HP - Damage);
	const FDYKProtectedObjectDefinition* Object = Config.ProtectedObjects.FindByPredicate(
		[ObjectId](const FDYKProtectedObjectDefinition& Value) { return Value.Id == ObjectId; });
	if (*HP == 0 && Object->bRequired) Finish(EDYKBattlePhase::Defeat, GameSeconds);
	return true;
}

int32 FDYKBattleSession::GetProtectedObjectHP(FName ObjectId) const
{
	const int32* HP = ProtectedHP.Find(ObjectId);
	return HP ? *HP : 0;
}

bool FDYKBattleSession::ResolveWave(const FGuid& WaveToken, double GameSeconds)
{
	if (!IsActiveToken(WaveToken) || !IsWaveClear() || !FMath::IsFinite(GameSeconds) || GameSeconds < WaveStartedAt) return false;
	ActiveSeconds += GameSeconds - WaveStartedAt;
	++CompletedWaves;
	if (CompletedWaves == Config.Waves.Num())
	{
		Finish(EDYKBattlePhase::Victory, GameSeconds, true);
	}
	else
	{
		Supplies += Config.Waves[WaveIndex].CompletionSupplies;
		Phase = EDYKBattlePhase::InterwavePreparation;
		PreparationId = FGuid::NewGuid();
		// Boundary consumers restore allies/cooldowns and clear effects; never refill traps.
		TGuardValue<bool> Guard(bDispatching, true);
		OnWaveCompleted.Broadcast(CompletedWaves);
	}
	return true;
}

void FDYKBattleSession::Finish(EDYKBattlePhase Outcome, double GameSeconds, bool bWaveCompleted)
{
	const double Seconds = ActiveSeconds + (Phase == EDYKBattlePhase::ActiveWave && Outcome != EDYKBattlePhase::Victory
		? GameSeconds - WaveStartedAt : 0);
	Phase = Outcome;
	Result = { AttemptId, Outcome, CompletedWaves, EnemiesKilled, Seconds };
	TGuardValue<bool> Guard(bDispatching, true);
	if (bWaveCompleted) OnWaveCompleted.Broadcast(CompletedWaves);
	OnFinished.Broadcast(Result);
}

bool FDYKBattleSession::Abort(double GameSeconds)
{
	if (bDispatching || IsTerminal() || Phase == EDYKBattlePhase::Uninitialized || !FMath::IsFinite(GameSeconds)
		|| GameSeconds < 0 || (Phase == EDYKBattlePhase::ActiveWave && GameSeconds < WaveStartedAt)) return false;
	Finish(EDYKBattlePhase::Aborted, GameSeconds);
	return true;
}
