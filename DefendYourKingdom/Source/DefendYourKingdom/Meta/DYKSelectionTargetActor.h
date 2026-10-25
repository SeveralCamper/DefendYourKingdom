// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "../Core/Interaction/DYKSelectionTarget.h"
#include "DYKSelectionTargetActor.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API ADYKSelectionTargetActor : public AStaticMeshActor, public IDYKSelectionTarget
{
	GENERATED_BODY()

public:
	ADYKSelectionTargetActor();
	virtual EDYKSelectionTargetKind GetSelectionTargetKind() const override;

private:
	UPROPERTY(EditInstanceOnly, Category="Selection")
	EDYKSelectionTargetKind SelectionTargetKind = EDYKSelectionTargetKind::CityBuilding;
};
