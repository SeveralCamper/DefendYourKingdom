#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Core/Interaction/DYKSelectionTarget.h"
#include "DYKUnitSlot.generated.h"

class ADYKDefenderActor;
class ADYKPolygonGameMode;
class UStaticMeshComponent;

// The map owns location/editability. Every ordinary unit uses this same slot kind.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKUnitSlot : public AActor, public IDYKSelectionTarget
{
	GENERATED_BODY()
public:
	ADYKUnitSlot();
	void UpdateMarker();
	UPROPERTY(EditAnywhere, Category="Placement") FName SlotId;
	UPROPERTY(EditAnywhere, Category="Placement") bool bPlayerEditable = true;
	ADYKDefenderActor* GetOccupant() const { return Occupant.Get(); }
	virtual EDYKSelectionTargetKind GetSelectionTargetKind() const override { return EDYKSelectionTargetKind::CombatSlot; }
private:
	friend class ADYKPolygonGameMode;
	UPROPERTY(Transient) TObjectPtr<ADYKDefenderActor> Occupant;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Marker;
};
