// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DYKContentId.h"
#include "DYKContentCatalogDataAsset.generated.h"

// Optional asset-side ID list only. Numeric base definitions live in DYKUnitCatalog
// and cannot be supplied or overwritten by an editable asset.
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
