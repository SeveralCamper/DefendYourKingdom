#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DYKEnemyRoute.generated.h"

class USplineComponent;

// Authored, reusable 3D path. Its endpoint is the logical contact point,
// independent of decorative objective geometry and blocking collision.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKEnemyRoute : public AActor
{
	GENERATED_BODY()
public:
	ADYKEnemyRoute();
	bool IsUsable() const;
	UPROPERTY(EditAnywhere, Category="Route") FName RouteId;
	UPROPERTY(EditAnywhere, Category="Route") FName ProtectedObjectId;
	UPROPERTY(VisibleAnywhere, Category="Route") TObjectPtr<USplineComponent> Path;
};
