#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DYKEnemyActor.generated.h"

class ADYKEnemyRoute;
class ADYKPolygonGameMode;

// One transient ordinary goblin. No collision-based navigation or enemy blocking.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKEnemyActor : public AActor
{
	GENERATED_BODY()
public:
	ADYKEnemyActor();
	bool Initialize(ADYKPolygonGameMode* OwnerMode, ADYKEnemyRoute* AssignedRoute, const FGuid& Token, FName Group);
	bool ApplyDamage(int32 Damage);
	void StopCombat();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	int32 GetHP() const { return HP; }
	bool HasReachedObjective() const { return bAtObjective; }
	const FGuid& GetEnemyId() const { return EnemyId; }
	float GetRouteDistance() const { return Distance; }

private:
	bool CanAct() const;
	void HitObjective();
	TWeakObjectPtr<ADYKPolygonGameMode> Mode;
	TWeakObjectPtr<ADYKEnemyRoute> Route;
	FGuid WaveToken, EnemyId;
	FName ObjectiveId;
	int32 HP = 0;
	float Distance = 0;
	bool bRegistered = false, bAtObjective = false;
	FTimerHandle AttackTimer;
};
