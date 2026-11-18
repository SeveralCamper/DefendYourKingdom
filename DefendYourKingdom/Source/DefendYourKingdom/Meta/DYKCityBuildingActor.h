#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Core/Content/DYKContentId.h"
#include "../Core/Interaction/DYKSelectionTarget.h"
#include "DYKCityBuildingActor.generated.h"

class UWidgetComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

// One visual per owned instance, created only in the city. Economy stays in C++ profile commands.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityBuildingActor : public AActor, public IDYKSelectionTarget
{
	GENERATED_BODY()
public:
	ADYKCityBuildingActor();
	virtual void BeginPlay() override;
	void SetHovered(bool Hovered);
	bool IsHovered() const {return bHovered;}
	UFUNCTION() void HoverBegin(UPrimitiveComponent* Component);
	UFUNCTION() void HoverEnd(UPrimitiveComponent* Component);
	virtual EDYKSelectionTargetKind GetSelectionTargetKind() const override { return EDYKSelectionTargetKind::CityBuilding; }
	UPROPERTY(VisibleAnywhere) FGuid InstanceId;
	UPROPERTY(VisibleAnywhere) FDYKContentId BuildingId;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> CollectionWidget;
private:
	bool bHovered=false;
};
