#pragma once

#include "DYKCombatUnit.h"
#include "DYKDefenderActor.generated.h"

// The first available troop: immutable peasant definition, individual HP/cooldown.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKDefenderActor : public ADYKCombatUnit
{
	GENERATED_BODY()
public:
	ADYKDefenderActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	// Mission authors may put a defender anywhere, without a player slot or cost.
	UPROPERTY(EditAnywhere, Category="Mission") bool bMissionPlaced = false;
	bool Initialize(ADYKPolygonGameMode* OwnerMode, const FGuid& Id);
	const FGuid& GetDefenseId() const { return DefenseId; }
	bool Resurrect();
	bool CanResurrect() const;
	void RestoreAfterWave();
protected:
	virtual void OnDeath() override;
private:
	FGuid DefenseId;
	double ResurrectAt = 0;
};
