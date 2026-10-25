// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DYKInteractableUnit.h"
#include "DYKPlacementSlot.generated.h"

UINTERFACE(MinimalAPI)
class UDYKPlacementSlot : public UInterface
{
	GENERATED_BODY()
};

class DEFENDYOURKINGDOM_API IDYKPlacementSlot
{
	GENERATED_BODY()

public:
	virtual const FDYKPlacementSlotDescriptor& GetDescriptor() const = 0;
	virtual bool CanAccept(const TScriptInterface<IDYKInteractableUnit>& Candidate) const = 0;
	virtual bool TryOccupy(const TScriptInterface<IDYKInteractableUnit>& Candidate) = 0;
	virtual TScriptInterface<IDYKInteractableUnit> ReleaseOccupant() = 0;
	virtual TScriptInterface<IDYKInteractableUnit> GetOccupant() const = 0;
	virtual TArray<FName> GetAvailableActions() const = 0;
};
