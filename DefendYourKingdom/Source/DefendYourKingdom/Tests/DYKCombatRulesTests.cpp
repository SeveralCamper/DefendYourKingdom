#include "../Battle/DYKCombatRules.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKFlatArmorTest, "DYK.M108.FlatArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDYKFlatArmorTest::RunTest(const FString& Parameters)
{
	using ED = EDYKDamageType;
	TestEqual(TEXT("P18-T06 physical ten minus two"), DYKCombatRules::DamageAfterArmor(10, 2, 7, ED::Physical), 8);
	TestEqual(TEXT("P18-T06 magical ten minus two"), DYKCombatRules::DamageAfterArmor(10, 7, 2, ED::Magical), 8);
	TestEqual(TEXT("Physical attack does not use magical armor"), DYKCombatRules::DamageAfterArmor(10, 0, 9, ED::Physical), 10);
	TestEqual(TEXT("Magical attack does not use physical armor"), DYKCombatRules::DamageAfterArmor(10, 9, 0, ED::Magical), 10);
	TestEqual(TEXT("Armor can fully absorb a hit"), DYKCombatRules::DamageAfterArmor(2, 2, 0, ED::Physical), 0);
	TestEqual(TEXT("Armor never produces healing"), DYKCombatRules::DamageAfterArmor(1, 5, 0, ED::Physical), 0);
	TestEqual(TEXT("Zero incoming damage stays zero"), DYKCombatRules::DamageAfterArmor(0, 2, 2, ED::Magical), 0);
	TestEqual(TEXT("Negative incoming damage cannot heal"), DYKCombatRules::DamageAfterArmor(-1, 0, 0, ED::Physical), 0);
	TestEqual(TEXT("Maximum integer hit stays safe"), DYKCombatRules::DamageAfterArmor(MAX_int32, 2, 0, ED::Physical), MAX_int32 - 2);
	return true;
}

#endif
