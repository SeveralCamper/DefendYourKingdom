#pragma once

#include "DYKContentId.h"

// Native API boundary; FDYKContentId remains the reflected, persistent wire/save format.
// No conversion between categories, or implicit conversion to/from an untyped ID.
template <EDYKContentKind ContentKind>
class TDYKTypedContentId
{
	static_assert(ContentKind >= EDYKContentKind::Resource && ContentKind <= EDYKContentKind::Mission);

public:
	TDYKTypedContentId() = default;
	explicit TDYKTypedContentId(FName InName) : Name(InName) {}

	static TOptional<TDYKTypedContentId> FromContentId(const FDYKContentId& Id)
	{
		if (!Id.IsValid() || Id.Kind != ContentKind)
		{
			return {};
		}
		return TDYKTypedContentId(Id.Name);
	}

	bool IsValid() const { return !Name.IsNone(); }
	FName GetName() const { return Name; }
	FDYKContentId ToContentId() const { return {ContentKind, Name}; }
	bool operator==(const TDYKTypedContentId& Other) const { return Name == Other.Name; }
	bool operator!=(const TDYKTypedContentId& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const TDYKTypedContentId& Id) { return GetTypeHash(Id.ToContentId()); }

private:
	FName Name = NAME_None;
};

using FDYKResourceId = TDYKTypedContentId<EDYKContentKind::Resource>;
using FDYKBuildingId = TDYKTypedContentId<EDYKContentKind::Building>;
using FDYKTrapId = TDYKTypedContentId<EDYKContentKind::Trap>;
using FDYKSpellId = TDYKTypedContentId<EDYKContentKind::Spell>;
// Warrior identifies both defenders and enemies; allegiance belongs to the definition.
using FDYKUnitTypeId = TDYKTypedContentId<EDYKContentKind::Warrior>;
using FDYKHeroId = TDYKTypedContentId<EDYKContentKind::Hero>;
using FDYKSlotId = TDYKTypedContentId<EDYKContentKind::Slot>;
using FDYKCampaignId = TDYKTypedContentId<EDYKContentKind::Campaign>;
using FDYKMissionId = TDYKTypedContentId<EDYKContentKind::Mission>;
