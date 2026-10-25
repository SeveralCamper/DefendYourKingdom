// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DYKContentId.h"
#include "DYKContentCatalogDataAsset.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API UDYKContentCatalogDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool Contains(const FDYKContentId& ContentId) const;
	TArray<FDYKContentId> GetIdsOfKind(EDYKContentKind Kind) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Content")
	TArray<FDYKContentId> ContentIds;
};
