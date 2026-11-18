#include "DYKUnitCatalog.h"

namespace
{
	template <typename T>
	constexpr TDYKDefinitionValue<T> DevPlaceholder(T Value)
	{
		return {Value, EDYKDefinitionSource::DEV_PLACEHOLDER};
	}

	template <typename T>
	constexpr TDYKDefinitionValue<T> NotApplicable(T Value)
	{
		return {Value, EDYKDefinitionSource::NotApplicable};
	}

	// spec/catalog/allies/01-peasant.md and 04-archer.md.
	constexpr FDYKUnitBaseStats PeasantBase = {
		{15}, {0}, {0},
		{{4}, {1}, EDYKDamageType::Physical, {1.5f}, {5.f}, EDYKHitTiming::FixedDelay,
			{0.3f}, NotApplicable(0.f), NotApplicable(0.f)},
		{0.f}, {5}, {25.f}, NotApplicable(0)
	};
	constexpr FDYKUnitBaseStats ArcherBase = {
		{10}, {0}, {0},
		{{7}, {1}, EDYKDamageType::Physical, {2.5f}, {50.f}, EDYKHitTiming::DistanceScaled,
			NotApplicable(0.f), NotApplicable(0.f), DevPlaceholder(0.01f)},
		{0.f}, {7}, {30.f}, NotApplicable(0)
	};
	// Authored base metadata is also used by the clickable barracks cards.
	constexpr FDYKUnitBaseStats GuardBase=[] {auto S=PeasantBase; S.MaxHP.Value=25; S.PhysicalArmor.Value=1; S.Attack.DamagePerHit.Value=7; return S;}();
	constexpr FDYKUnitBaseStats RoyalGuardBase=[] {auto S=PeasantBase; S.MaxHP.Value=35; S.PhysicalArmor.Value=2; S.Attack.DamagePerHit.Value=10; return S;}();
	constexpr FDYKUnitBaseStats VeteranArcherBase=[] {auto S=ArcherBase; S.MaxHP.Value=15; S.PhysicalArmor.Value=1; S.Attack.DamagePerHit.Value=11; return S;}();
	constexpr FDYKUnitBaseStats CrossbowmanBase=[] {auto S=ArcherBase; S.MaxHP.Value=25; S.PhysicalArmor.Value=2; S.Attack.DamagePerHit.Value=18; S.Attack.IntervalSeconds.Value=4; S.Attack.Range.Value=60; return S;}();
	// spec/catalog/enemies/01-goblin.md and 03-goblin-assassin.md.
	// Missing impact timings are replaceable DEV_PLACEHOLDER fields, not passport edits.
	constexpr FDYKUnitBaseStats GoblinBase = {
		{10}, {0}, {0},
		{{1}, {1}, EDYKDamageType::Physical, {2.f}, {5.f}, EDYKHitTiming::FixedDelay,
			DevPlaceholder(0.3f), NotApplicable(0.f), NotApplicable(0.f)},
		{10.f}, NotApplicable(0), NotApplicable(0.f), {1}
	};
	constexpr FDYKUnitBaseStats GoblinAssassinBase = {
		{15}, {0}, {0},
		{{3}, {2}, EDYKDamageType::Physical, {2.f}, {5.f}, EDYKHitTiming::FixedDelay,
			DevPlaceholder(0.3f), DevPlaceholder(0.2f), NotApplicable(0.f)},
		{15.f}, NotApplicable(0), NotApplicable(0.f), {3}
	};

	const FDYKUnitDefinition* Definitions(int32& Count)
	{
		// FName is not constexpr; only this small ID layer requires runtime construction.
		static const FDYKUnitDefinition Entries[] = {
			{DYKUnitIds::Peasant(), EDYKUnitSide::Defender, EDYKUnitLine::Infantry,
				1, EDYKUnitMovement::Stationary, PeasantBase},
			{DYKUnitIds::Archer(), EDYKUnitSide::Defender, EDYKUnitLine::Ranged,
				1, EDYKUnitMovement::Stationary, ArcherBase},
			{FDYKUnitTypeId(TEXT("DYK-GUARD")),EDYKUnitSide::Defender,EDYKUnitLine::Infantry,2,EDYKUnitMovement::Stationary,GuardBase},
			{FDYKUnitTypeId(TEXT("DYK-ROYAL-GUARD")),EDYKUnitSide::Defender,EDYKUnitLine::Infantry,3,EDYKUnitMovement::Stationary,RoyalGuardBase},
			{FDYKUnitTypeId(TEXT("DYK-VETERAN-ARCHER")),EDYKUnitSide::Defender,EDYKUnitLine::Ranged,2,EDYKUnitMovement::Stationary,VeteranArcherBase},
			{FDYKUnitTypeId(TEXT("DYK-CROSSBOWMAN")),EDYKUnitSide::Defender,EDYKUnitLine::Ranged,3,EDYKUnitMovement::Stationary,CrossbowmanBase},
			{DYKUnitIds::Goblin(), EDYKUnitSide::Enemy, EDYKUnitLine::None,
				0, EDYKUnitMovement::GroundRoute, GoblinBase},
			{DYKUnitIds::GoblinAssassin(), EDYKUnitSide::Enemy, EDYKUnitLine::None,
				0, EDYKUnitMovement::GroundRoute, GoblinAssassinBase}
		};
		Count = UE_ARRAY_COUNT(Entries);
		return Entries;
	}
}

TConstArrayView<FDYKUnitDefinition> DYKUnitCatalog::GetAll()
{
	int32 Count;
	const FDYKUnitDefinition* Entries = Definitions(Count);
	return {Entries, Count};
}

const FDYKUnitDefinition* DYKUnitCatalog::Find(FDYKUnitTypeId Id)
{
	if (!Id.IsValid())
	{
		return nullptr;
	}
	for (const FDYKUnitDefinition& Definition : GetAll())
	{
		if (Definition.Id == Id)
		{
			return &Definition;
		}
	}
	return nullptr;
}

const FDYKUnitDefinition* DYKUnitCatalog::Find(const FDYKContentId& Id)
{
	const auto TypedId = FDYKUnitTypeId::FromContentId(Id);
	return TypedId.IsSet() ? Find(TypedId.GetValue()) : nullptr;
}

const FDYKUnitTypeId& DYKUnitIds::Peasant()
{
	static const FDYKUnitTypeId Id(TEXT("DYK-PEASANT"));
	return Id;
}

const FDYKUnitTypeId& DYKUnitIds::Archer()
{
	static const FDYKUnitTypeId Id(TEXT("DYK-ARCHER"));
	return Id;
}

const FDYKUnitTypeId& DYKUnitIds::Goblin()
{
	static const FDYKUnitTypeId Id(TEXT("FK-GOBLIN"));
	return Id;
}

const FDYKUnitTypeId& DYKUnitIds::GoblinAssassin()
{
	static const FDYKUnitTypeId Id(TEXT("FK-GOBLIN-ASSASSIN"));
	return Id;
}
