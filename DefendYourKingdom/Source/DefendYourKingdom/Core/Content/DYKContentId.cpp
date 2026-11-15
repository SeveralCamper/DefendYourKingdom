// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKContentId.h"

bool FDYKContentId::IsValid() const
{
	switch (Kind)
	{
	case EDYKContentKind::Resource:
	case EDYKContentKind::Building:
	case EDYKContentKind::Trap:
	case EDYKContentKind::Spell:
	case EDYKContentKind::Warrior:
	case EDYKContentKind::Hero:
	case EDYKContentKind::Slot:
	case EDYKContentKind::Campaign:
	case EDYKContentKind::Mission:
		return !Name.IsNone();
	default:
		return false;
	}
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
