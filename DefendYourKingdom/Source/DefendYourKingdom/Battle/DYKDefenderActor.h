#pragma once

#include "DYKCombatUnit.h"
#include "../Meta/DYKHeroes.h"
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
	// None retains legacy map peasants; the paladin uses the same actor and slot.
	UPROPERTY(EditAnywhere, Category="Mission") FName HeroType;
	bool IsHero() const { return !HeroType.IsNone(); }
	bool CanActivateAbility() const;
	bool ActivateAbility();
	double AbilityCooldownRemaining() const;
	virtual void StopCombat() override;
	bool Initialize(ADYKPolygonGameMode* OwnerMode, const FGuid& Id);
	const FGuid& GetDefenseId() const { return DefenseId; }
	bool Resurrect();
	bool CanResurrect() const;
	void RestoreAfterWave();
protected:
	virtual void OnDeath() override;
private:
	void ResolveAbility();
	FDYKHeroAbilityDefinition AbilityDefinition;
	FTimerHandle AbilityTimer;
	double AbilityReadyAt=0;
	FGuid AbilityWave;
	FVector AbilityPoint=FVector::ZeroVector;
	FGuid DefenseId;
	double ResurrectAt = 0;
};
