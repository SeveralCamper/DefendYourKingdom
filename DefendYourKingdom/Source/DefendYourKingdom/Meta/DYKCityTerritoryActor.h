#pragma once
#include "GameFramework/Actor.h"
#include "DYKCityTerritoryActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityTerritoryActor : public AActor
{
	GENERATED_BODY()
public:
	ADYKCityTerritoryActor();
	void Refresh();
protected:
	virtual void BeginPlay() override;
private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lines;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Ground;
};
