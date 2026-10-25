// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DYKPlacementSlotDescriptor.h"
#include "DYKInteractableUnit.generated.h"

UINTERFACE(MinimalAPI)
class UDYKInteractableUnit : public UInterface
{
	GENERATED_BODY()
};

class DEFENDYOURKINGDOM_API IDYKInteractableUnit
{
	GENERATED_BODY()

public:
	virtual bool CanOccupySlot(const FDYKPlacementSlotDescriptor& Slot) const = 0;
	virtual TArray<FName> GetAvailableActions() const = 0;
	virtual void OnPlacedInSlot(const FDYKPlacementSlotDescriptor& Slot) = 0;
	virtual void OnRemovedFromSlot(const FDYKPlacementSlotDescriptor& Slot) = 0;
};
