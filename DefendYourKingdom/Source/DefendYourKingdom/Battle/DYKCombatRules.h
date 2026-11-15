#pragma once

#include "../Core/Content/DYKUnitDefinition.h"

namespace DYKCombatRules
{
	// Only the world conversion is provisional; passport ranges and speeds are unchanged.
	constexpr float CentimetersPerUnitDEV_PLACEHOLDER = 30.f;
	inline int32 DamageAfterArmor(int32 Damage, int32 PhysicalArmor, int32 MagicalArmor, EDYKDamageType Type)
	{
		const int32 Armor = Type == EDYKDamageType::Physical ? PhysicalArmor : MagicalArmor;
		return static_cast<int32>(FMath::Max<int64>(0, int64(Damage) - FMath::Max(0, Armor)));
	}
}
