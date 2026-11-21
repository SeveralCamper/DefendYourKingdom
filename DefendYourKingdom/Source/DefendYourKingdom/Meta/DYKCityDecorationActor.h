#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DYKCityRoads.h"
#include "DYKCityDecorationActor.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityDecorationActor : public AActor
{
	GENERATED_BODY()
public:
	ADYKCityDecorationActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void Refresh(const FDYKPlayerProfileSaveData& Profile,const FDYKCityLayoutEditor* Editor=nullptr);
	const FDYKCityRoadNetwork& GetRoads() const { return Network; }
	const FDYKCityPedestrians& GetPedestrians() const { return Pedestrians; }
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Roads;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Residents;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> HeroResidents;
private:
	void UpdateResidents();
	FDYKCityRoadNetwork Network;
	FDYKCityPedestrians Pedestrians;
	FDYKCityPedestrians HeroPedestrians;
	TArray<FIntPoint> RenderedCells;
	FRandomStream Random;
	FTimerHandle Timer;
	double LastUpdate=0;
	bool bRandomInitialized=false;
};
