// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKContentCatalogDataAsset.h"

bool UDYKContentCatalogDataAsset::Contains(const FDYKContentId& ContentId) const
{
	return ContentId.IsValid() && ContentIds.Contains(ContentId);
}

TArray<FDYKContentId> UDYKContentCatalogDataAsset::GetIdsOfKind(EDYKContentKind Kind) const
{
	TArray<FDYKContentId> MatchingIds;
	for (const FDYKContentId& ContentId : ContentIds)
	{
		if (ContentId.Kind == Kind && ContentId.IsValid())
		{
			MatchingIds.Add(ContentId);
		}
	}

	return MatchingIds;
}
