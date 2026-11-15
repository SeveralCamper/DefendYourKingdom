#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Core/Content/DYKContentId.h"
#include "DYKCityBuildingActor.generated.h"

class UWidgetComponent;
class UStaticMeshComponent;

// One visual per owned instance, created only in the city. Economy stays in C++ profile commands.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityBuildingActor : public AActor
{
	GENERATED_BODY()
public:
	ADYKCityBuildingActor();
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere) FGuid InstanceId;
	UPROPERTY(VisibleAnywhere) FDYKContentId BuildingId;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> CollectionWidget;
};
