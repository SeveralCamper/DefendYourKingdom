// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DYKPlacementSlot.h"
#include "DYKPlacementSlotObject.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API UDYKPlacementSlotObject : public UObject, public IDYKPlacementSlot
{
	GENERATED_BODY()

public:
	virtual const FDYKPlacementSlotDescriptor& GetDescriptor() const override;
	virtual bool CanAccept(const TScriptInterface<IDYKInteractableUnit>& Candidate) const override;
	virtual bool TryOccupy(const TScriptInterface<IDYKInteractableUnit>& Candidate) override;
	virtual TScriptInterface<IDYKInteractableUnit> ReleaseOccupant() override;
	virtual TScriptInterface<IDYKInteractableUnit> GetOccupant() const override;
	virtual TArray<FName> GetAvailableActions() const override;

	void Initialize(const FDYKPlacementSlotDescriptor& InDescriptor);

private:
	UPROPERTY()
	FDYKPlacementSlotDescriptor Descriptor;

	UPROPERTY()
	TScriptInterface<IDYKInteractableUnit> Occupant;
};
