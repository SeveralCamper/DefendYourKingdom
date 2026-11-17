#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DYKCityBuildingPreviewActor.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

// Presentation only: the complete existing building model, without collection UI or selection.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityBuildingPreviewActor : public AActor
{
	GENERATED_BODY()
public:
	ADYKCityBuildingPreviewActor();
	bool Configure(FName BuildingType,bool Ghost);
	UTextureRenderTarget2D* CaptureThumbnail(UObject* TextureOwner);
	bool IsGhost() const { return bGhost; }
	FBox GetModelBounds() const;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneCaptureComponent2D> Capture;
private:
	bool bGhost=false;
	bool bConfigured=false;
};
