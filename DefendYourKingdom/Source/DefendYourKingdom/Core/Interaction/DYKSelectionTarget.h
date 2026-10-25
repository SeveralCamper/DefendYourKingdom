// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DYKSelectionTarget.generated.h"

UENUM()
enum class EDYKSelectionTargetKind : uint8
{
	None,
	CityCell,
	CityBuilding,
	CombatSlot
};

UINTERFACE(MinimalAPI)
class UDYKSelectionTarget : public UInterface
{
	GENERATED_BODY()
};

class DEFENDYOURKINGDOM_API IDYKSelectionTarget
{
	GENERATED_BODY()

public:
	virtual EDYKSelectionTargetKind GetSelectionTargetKind() const = 0;
};
