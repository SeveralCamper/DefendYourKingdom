// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKPlaceholderInteractableUnit.h"

bool UDYKPlaceholderInteractableUnit::CanOccupySlot(const FDYKPlacementSlotDescriptor& Slot) const
{
	return !bIsPlaced
		&& (AcceptedSlotTypes.IsEmpty() || AcceptedSlotTypes.Contains(Slot.SlotType));
}

TArray<FName> UDYKPlaceholderInteractableUnit::GetAvailableActions() const
{
	return AvailableActions;
}

void UDYKPlaceholderInteractableUnit::OnPlacedInSlot(const FDYKPlacementSlotDescriptor& Slot)
{
	OccupiedSlotId = Slot.SlotId;
	bIsPlaced = true;
}

void UDYKPlaceholderInteractableUnit::OnRemovedFromSlot(const FDYKPlacementSlotDescriptor& Slot)
{
	if (OccupiedSlotId == Slot.SlotId)
	{
		OccupiedSlotId = NAME_None;
		bIsPlaced = false;
	}
}
