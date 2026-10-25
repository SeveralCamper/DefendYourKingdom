// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DYKPlacementSlotDescriptor.generated.h"

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKPlacementSlotDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	FName SlotId = NAME_None;

	UPROPERTY()
	FName SlotType = NAME_None;

	UPROPERTY()
	FIntPoint SizeInCells = FIntPoint(1, 1);
};
