#pragma once

#include "CoreMinimal.h"
#include "DYKCombatUnit.h"
#include "DYKEnemyActor.generated.h"

class ADYKEnemyRoute;
class ADYKPolygonGameMode;

// One transient ordinary goblin. No collision-based navigation or enemy blocking.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKEnemyActor : public ADYKCombatUnit
{
	GENERATED_BODY()
public:
	ADYKEnemyActor();
	bool Initialize(ADYKPolygonGameMode* OwnerMode, ADYKEnemyRoute* AssignedRoute, const FGuid& Token, FName Group);
	virtual void StopCombat() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool HasReachedObjective() const { return bAtObjective; }
	const FGuid& GetEnemyId() const { return EnemyId; }
	float GetRouteDistance() const { return Distance; }

protected:
	virtual bool CanAct() const override;
	virtual bool HasFallbackTarget() const override;
	virtual void HitFallbackTarget() override;
	virtual void OnDeath() override;
	virtual void OnTargetsUpdated() override;
private:
	TWeakObjectPtr<ADYKEnemyRoute> Route;
	FGuid WaveToken, EnemyId;
	FName ObjectiveId;
	float Distance = 0;
	bool bRegistered = false, bAtObjective = false;
};
