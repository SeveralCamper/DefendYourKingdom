#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Core/Content/DYKUnitDefinition.h"
#include "DYKCombatUnit.generated.h"

class ADYKPolygonGameMode;

// Shared FIFO, health and impact scheduling for both sides. No physical projectiles or LOS.
UCLASS(Abstract)
class DEFENDYOURKINGDOM_API ADYKCombatUnit : public AActor
{
	GENERATED_BODY()
public:
	ADYKCombatUnit();
	int32 GetHP() const { return HP; }
	bool IsAlive() const { return HP > 0 && !IsActorBeingDestroyed(); }
	const FDYKUnitDefinition* GetUnitDefinition() const { return Definition; }
	ADYKCombatUnit* GetCurrentTarget() const;
	ADYKCombatUnit* GetPendingTarget() const { return PendingTarget.Get(); }
	bool ApplyDamage(int32 Damage, EDYKDamageType Type = EDYKDamageType::Physical);
	void RefreshTargets(const TArray<ADYKCombatUnit*>& Candidates);
	virtual void StopCombat();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
	void InitializeCombat(ADYKPolygonGameMode* OwnerMode, const FDYKUnitDefinition* UnitDefinition);
	virtual bool CanAct() const;
	virtual bool HasFallbackTarget() const { return false; }
	virtual void HitFallbackTarget() {}
	virtual void OnDeath() {}
	virtual void OnTargetsUpdated() {}
	void RestoreHealth();
	TWeakObjectPtr<ADYKPolygonGameMode> Mode;

private:
	bool IsInRange(const ADYKCombatUnit* Candidate) const;
	void BeginAttack();
	void RequestAttack();
	void ResolveHit();
	void RestoreIdleFacing();
	const FDYKUnitDefinition* Definition = nullptr;
	int32 HP = 0;
	TArray<TWeakObjectPtr<ADYKCombatUnit>> Targets;
	TWeakObjectPtr<ADYKCombatUnit> PendingTarget;
	bool bPendingFallback = false;
	double NextAttackAt = 0;
	FRotator IdleFacing;
	FTimerHandle SwingTimer, HitTimer;
};
