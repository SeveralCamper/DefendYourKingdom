// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DYKInteractableUnit.h"
#include "DYKPlaceholderInteractableUnit.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API UDYKPlaceholderInteractableUnit : public UObject, public IDYKInteractableUnit
{
	GENERATED_BODY()

public:
	virtual bool CanOccupySlot(const FDYKPlacementSlotDescriptor& Slot) const override;
	virtual TArray<FName> GetAvailableActions() const override;
	virtual void OnPlacedInSlot(const FDYKPlacementSlotDescriptor& Slot) override;
	virtual void OnRemovedFromSlot(const FDYKPlacementSlotDescriptor& Slot) override;

	UPROPERTY()
	TSet<FName> AcceptedSlotTypes;

	UPROPERTY()
	TArray<FName> AvailableActions;

private:
	UPROPERTY()
	FName OccupiedSlotId = NAME_None;

	UPROPERTY()
	bool bIsPlaced = false;
};
