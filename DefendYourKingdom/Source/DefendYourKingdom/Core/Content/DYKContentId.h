// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "DYKContentId.generated.h"

UENUM()
enum class EDYKContentKind : uint8
{
	None = 0,
	Resource = 1,
	Building = 2,
	Trap = 3,
	Spell = 4,
	Warrior = 5,
	Hero = 6,
	Slot = 7,
	Campaign = 8,
	Mission = 9
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKContentId
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EDYKContentKind Kind = EDYKContentKind::None;

	// Treat Kind and Name as persistent identifiers; don't rename or reuse assigned values.
	UPROPERTY(SaveGame)
	FName Name = NAME_None;

	bool IsValid() const;
	FPrimaryAssetId ToPrimaryAssetId() const;

	bool operator==(const FDYKContentId& Other) const;
};
