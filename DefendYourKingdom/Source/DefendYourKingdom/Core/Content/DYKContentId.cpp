// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKContentId.h"

bool FDYKContentId::IsValid() const
{
	return Kind != EDYKContentKind::None && !Name.IsNone();
}

FPrimaryAssetId FDYKContentId::ToPrimaryAssetId() const
{
	if (!IsValid())
	{
		return FPrimaryAssetId();
	}

	const UEnum* KindEnum = StaticEnum<EDYKContentKind>();
	if (!KindEnum)
	{
		return FPrimaryAssetId();
	}

	const FName TypeName(*KindEnum->GetNameStringByValue(static_cast<int64>(Kind)));
	return FPrimaryAssetId(FPrimaryAssetType(TypeName), Name);
}

bool FDYKContentId::operator==(const FDYKContentId& Other) const
{
	return Kind == Other.Kind && Name == Other.Name;
}
