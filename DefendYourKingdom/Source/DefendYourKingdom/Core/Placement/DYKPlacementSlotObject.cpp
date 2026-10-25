// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKPlacementSlotObject.h"

const FDYKPlacementSlotDescriptor& UDYKPlacementSlotObject::GetDescriptor() const
{
	return Descriptor;
}

bool UDYKPlacementSlotObject::CanAccept(const TScriptInterface<IDYKInteractableUnit>& Candidate) const
{
	const IDYKInteractableUnit* CandidateInterface = Candidate.GetInterface();
	return !Occupant.GetObject() && Candidate.GetObject() && CandidateInterface
		&& CandidateInterface->CanOccupySlot(Descriptor);
}

bool UDYKPlacementSlotObject::TryOccupy(const TScriptInterface<IDYKInteractableUnit>& Candidate)
{
	if (!CanAccept(Candidate))
	{
		return false;
	}

	Occupant = Candidate;
	Occupant.GetInterface()->OnPlacedInSlot(Descriptor);
	return true;
}

TScriptInterface<IDYKInteractableUnit> UDYKPlacementSlotObject::ReleaseOccupant()
{
	TScriptInterface<IDYKInteractableUnit> ReleasedOccupant = Occupant;
	if (IDYKInteractableUnit* OccupantInterface = Occupant.GetInterface())
	{
		OccupantInterface->OnRemovedFromSlot(Descriptor);
	}

	Occupant.SetObject(nullptr);
	Occupant.SetInterface(nullptr);
	return ReleasedOccupant;
}

TScriptInterface<IDYKInteractableUnit> UDYKPlacementSlotObject::GetOccupant() const
{
	return Occupant;
}

TArray<FName> UDYKPlacementSlotObject::GetAvailableActions() const
{
	const IDYKInteractableUnit* OccupantInterface = Occupant.GetInterface();
	return OccupantInterface ? OccupantInterface->GetAvailableActions() : TArray<FName>();
}

void UDYKPlacementSlotObject::Initialize(const FDYKPlacementSlotDescriptor& InDescriptor)
{
	Descriptor = InDescriptor;
}
