#include "../Core/Content/DYKUnitCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Class.h"
#include <type_traits>
#include <utility>

namespace
{
	static_assert(!std::is_convertible_v<FDYKResourceId, FDYKUnitTypeId>);
	static_assert(!std::is_constructible_v<FDYKUnitTypeId, FDYKResourceId>);
	static_assert(!std::is_convertible_v<FDYKContentId, FDYKUnitTypeId>);
	static_assert(!std::is_convertible_v<FDYKUnitTypeId, FDYKContentId>);
	static_assert(std::is_same_v<decltype(DYKUnitCatalog::Find(std::declval<FDYKUnitTypeId>())),
		const FDYKUnitDefinition*>);
	using FCatalogEntry = decltype(DYKUnitCatalog::GetAll()[0]);
	static_assert(std::is_const_v<std::remove_reference_t<FCatalogEntry>>);
	static_assert(!std::is_assignable_v<decltype((DYKUnitCatalog::GetAll()[0].Base.MaxHP.Value)), int32>);
	static_assert(!std::is_assignable_v<decltype((DYKUnitCatalog::GetAll()[0].Id)), FDYKUnitTypeId>);

	template <typename T>
	void ExpectValue(FAutomationTestBase& Test, const TCHAR* Label,
		const TDYKDefinitionValue<T>& Actual, T Expected,
		EDYKDefinitionSource Source = EDYKDefinitionSource::Author)
	{
		Test.TestEqual(Label, Actual.Value, Expected);
		Test.TestTrue(FString(Label) + TEXT(" provenance"), Actual.Source == Source);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKContentIdsTest, "DYK.M102.ContentIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDYKContentIdsTest::RunTest(const FString& Parameters)
{
	const FName SharedName(TEXT("SamePersistentName"));
	const FDYKContentId Unit{EDYKContentKind::Warrior, SharedName};
	const FDYKContentId Resource{EDYKContentKind::Resource, SharedName};
	const FDYKContentId InvalidIds[] = {
		{}, {EDYKContentKind::None, SharedName}, {EDYKContentKind::Warrior, NAME_None},
		{static_cast<EDYKContentKind>(255), SharedName}
	};
	for (const FDYKContentId& Invalid : InvalidIds)
	{
		TestFalse(TEXT("Invalid ID rejected"), Invalid.IsValid());
		TestFalse(TEXT("Invalid primary asset ID rejected"), Invalid.ToPrimaryAssetId().IsValid());
		TestFalse(TEXT("Invalid typed conversion rejected"), FDYKUnitTypeId::FromContentId(Invalid).IsSet());
		TestNull(TEXT("Invalid lookup returns null"), DYKUnitCatalog::Find(Invalid));
	}
	TestFalse(TEXT("Default typed ID invalid"), FDYKUnitTypeId().IsValid());
	TestTrue(TEXT("Unit ID valid"), Unit.IsValid());
	TestTrue(TEXT("Category contributes to identity"), Unit != Resource);
	TestFalse(TEXT("Resource cannot be interpreted as unit"), FDYKUnitTypeId::FromContentId(Resource).IsSet());
	const auto Typed = FDYKUnitTypeId::FromContentId(Unit);
	if (!TestTrue(TEXT("Unit boundary conversion succeeds"), Typed.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("Typed conversion round trip"), Typed->ToContentId() == Unit);
	TestTrue(TEXT("Typed equality"), *Typed == FDYKUnitTypeId(SharedName));
	TestEqual(TEXT("Equal IDs hash equally"), GetTypeHash(*Typed), GetTypeHash(FDYKUnitTypeId(SharedName)));
	TSet<FDYKContentId> Set;
	Set.Add(Unit);
	Set.Add(Resource);
	Set.Add(Typed->ToContentId());
	TestEqual(TEXT("Hash set preserves category and deduplicates equal ID"), Set.Num(), 2);
	TestEqual(TEXT("Persisted Warrior enum unchanged"), static_cast<uint8>(EDYKContentKind::Warrior), uint8(5));
	TestEqual(TEXT("Persisted Mission enum unchanged"), static_cast<uint8>(EDYKContentKind::Mission), uint8(9));
	TestEqual(TEXT("Primary asset category"), Unit.ToPrimaryAssetId().PrimaryAssetType.GetName(), FName(TEXT("Warrior")));
	TestEqual(TEXT("Primary asset key"), Unit.ToPrimaryAssetId().PrimaryAssetName, SharedName);
	TestTrue(TEXT("Primary asset category distinguishes equal name"), Unit.ToPrimaryAssetId() != Resource.ToPrimaryAssetId());

	TArray<uint8> Bytes;
	FDYKContentId Saved = DYKUnitIds::Goblin().ToContentId();
	{
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FDYKContentId::StaticStruct()->SerializeItem(Archive, &Saved, nullptr);
		TestFalse(TEXT("Save serialization succeeds"), Archive.IsError());
	}
	FDYKContentId Loaded;
	{
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		Archive.ArIsSaveGame = true;
		FDYKContentId::StaticStruct()->SerializeItem(Archive, &Loaded, nullptr);
		TestFalse(TEXT("Load serialization succeeds"), Archive.IsError());
	}
	TestTrue(TEXT("Reflected ID save round trip"), Loaded == Saved);
	TestTrue(TEXT("Loaded ID resolves same shared definition"), DYKUnitCatalog::Find(Loaded) == DYKUnitCatalog::Find(DYKUnitIds::Goblin()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKUnitCatalogValuesTest, "DYK.M102.CatalogValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDYKUnitCatalogValuesTest::RunTest(const FString& Parameters)
{
	const auto Entries = DYKUnitCatalog::GetAll();
	TestTrue(TEXT("Catalog contains the four M1 types"), Entries.Num() >= 4);
	TSet<FDYKUnitTypeId> UniqueIds;
	for (const FDYKUnitDefinition& Entry : Entries)
	{
		TestTrue(TEXT("Every definition has valid ID"), Entry.Id.IsValid());
		TestFalse(TEXT("No duplicate definition key"), UniqueIds.Contains(Entry.Id));
		UniqueIds.Add(Entry.Id);
		TestTrue(TEXT("Typed lookup returns exact catalog address"), DYKUnitCatalog::Find(Entry.Id) == &Entry);
		TestTrue(TEXT("Persistent lookup returns exact catalog address"), DYKUnitCatalog::Find(Entry.Id.ToContentId()) == &Entry);
	}
	TestNull(TEXT("Unknown unit ID returns null"), DYKUnitCatalog::Find(FDYKUnitTypeId(TEXT("TEST-UNKNOWN-UNIT"))));
	TestNull(TEXT("No fallback for empty ID"), DYKUnitCatalog::Find(FDYKUnitTypeId()));
	TestNull(TEXT("Known name in wrong category returns null"), DYKUnitCatalog::Find(FDYKContentId{EDYKContentKind::Hero, DYKUnitIds::Peasant().GetName()}));

	const auto* Peasant = DYKUnitCatalog::Find(DYKUnitIds::Peasant());
	const auto* Archer = DYKUnitCatalog::Find(DYKUnitIds::Archer());
	const auto* Goblin = DYKUnitCatalog::Find(DYKUnitIds::Goblin());
	const auto* Assassin = DYKUnitCatalog::Find(DYKUnitIds::GoblinAssassin());
	if (!TestNotNull(TEXT("Peasant exists"), Peasant) || !TestNotNull(TEXT("Archer exists"), Archer)
		|| !TestNotNull(TEXT("Goblin exists"), Goblin) || !TestNotNull(TEXT("Assassin exists"), Assassin))
	{
		return false;
	}
	TestEqual(TEXT("Peasant stable key"), Peasant->Id.GetName(), FName(TEXT("DYK-PEASANT")));
	TestEqual(TEXT("Archer stable key"), Archer->Id.GetName(), FName(TEXT("DYK-ARCHER")));
	TestEqual(TEXT("Goblin stable key"), Goblin->Id.GetName(), FName(TEXT("FK-GOBLIN")));
	TestEqual(TEXT("Assassin stable key"), Assassin->Id.GetName(), FName(TEXT("FK-GOBLIN-ASSASSIN")));
	ExpectValue(*this, TEXT("Peasant HP"), Peasant->Base.MaxHP, 15);
	ExpectValue(*this, TEXT("Peasant damage"), Peasant->Base.Attack.DamagePerHit, 4);
	ExpectValue(*this, TEXT("Peasant interval"), Peasant->Base.Attack.IntervalSeconds, 1.5f);
	ExpectValue(*this, TEXT("Peasant range"), Peasant->Base.Attack.Range, 5.f);
	ExpectValue(*this, TEXT("Peasant impact"), Peasant->Base.Attack.FirstHitDelaySeconds, 0.3f);
	ExpectValue(*this, TEXT("Peasant supplies"), Peasant->Base.PlacementSupplies, 5);
	ExpectValue(*this, TEXT("Peasant death cooldown"), Peasant->Base.DeathCooldownSeconds, 25.f);
	ExpectValue(*this, TEXT("Archer HP"), Archer->Base.MaxHP, 10);
	ExpectValue(*this, TEXT("Archer damage"), Archer->Base.Attack.DamagePerHit, 7);
	ExpectValue(*this, TEXT("Archer interval"), Archer->Base.Attack.IntervalSeconds, 2.5f);
	ExpectValue(*this, TEXT("Archer range"), Archer->Base.Attack.Range, 50.f);
	ExpectValue(*this, TEXT("Archer supplies"), Archer->Base.PlacementSupplies, 7);
	ExpectValue(*this, TEXT("Archer death cooldown"), Archer->Base.DeathCooldownSeconds, 30.f);
	TestTrue(TEXT("Archer timing depends on distance"), Archer->Base.Attack.HitTiming == EDYKHitTiming::DistanceScaled);
	ExpectValue(*this, TEXT("Archer timing placeholder"), Archer->Base.Attack.SecondsPerRangeUnit, 0.01f, EDYKDefinitionSource::DEV_PLACEHOLDER);
	ExpectValue(*this, TEXT("Goblin HP"), Goblin->Base.MaxHP, 10);
	ExpectValue(*this, TEXT("Goblin damage"), Goblin->Base.Attack.DamagePerHit, 1);
	ExpectValue(*this, TEXT("Goblin speed"), Goblin->Base.MovementSpeed, 10.f);
	ExpectValue(*this, TEXT("Goblin kill resource"), Goblin->Base.KillReloadResource, 1);
	ExpectValue(*this, TEXT("Goblin timing placeholder"), Goblin->Base.Attack.FirstHitDelaySeconds, 0.3f, EDYKDefinitionSource::DEV_PLACEHOLDER);
	ExpectValue(*this, TEXT("Assassin HP"), Assassin->Base.MaxHP, 15);
	ExpectValue(*this, TEXT("Assassin separate hit count"), Assassin->Base.Attack.HitCount, 2);
	ExpectValue(*this, TEXT("Assassin damage per component"), Assassin->Base.Attack.DamagePerHit, 3);
	ExpectValue(*this, TEXT("Assassin speed"), Assassin->Base.MovementSpeed, 15.f);
	ExpectValue(*this, TEXT("Assassin kill resource"), Assassin->Base.KillReloadResource, 3);
	ExpectValue(*this, TEXT("Assassin first hit placeholder"), Assassin->Base.Attack.FirstHitDelaySeconds, 0.3f, EDYKDefinitionSource::DEV_PLACEHOLDER);
	ExpectValue(*this, TEXT("Assassin second hit placeholder"), Assassin->Base.Attack.FollowUpHitDelaySeconds, 0.2f, EDYKDefinitionSource::DEV_PLACEHOLDER);
	for (const auto* Enemy : {Goblin, Assassin})
	{
		ExpectValue(*this, TEXT("Enemy has no placement price"), Enemy->Base.PlacementSupplies, 0, EDYKDefinitionSource::NotApplicable);
		ExpectValue(*this, TEXT("Enemy has no defender death cooldown"), Enemy->Base.DeathCooldownSeconds, 0.f, EDYKDefinitionSource::NotApplicable);
		ExpectValue(*this, TEXT("Enemy attack interval"), Enemy->Base.Attack.IntervalSeconds, 2.f);
		ExpectValue(*this, TEXT("Enemy range"), Enemy->Base.Attack.Range, 5.f);
		TestTrue(TEXT("Enemy route movement"), Enemy->Movement == EDYKUnitMovement::GroundRoute);
		TestTrue(TEXT("Enemy allegiance"), Enemy->Side == EDYKUnitSide::Enemy);
	}
	for (const auto* Defender : {Peasant, Archer})
	{
		ExpectValue(*this, TEXT("Defender has no kill reward"), Defender->Base.KillReloadResource, 0, EDYKDefinitionSource::NotApplicable);
		TestTrue(TEXT("Defender stationary"), Defender->Movement == EDYKUnitMovement::Stationary);
		TestTrue(TEXT("Defender allegiance"), Defender->Side == EDYKUnitSide::Defender);
		TestEqual(TEXT("Initial defender tier"), Defender->Tier, 1);
	}
	for (const auto* Entry : {Peasant, Archer, Goblin, Assassin})
	{
		ExpectValue(*this, TEXT("Physical armor"), Entry->Base.PhysicalArmor, 0);
		ExpectValue(*this, TEXT("Magical armor"), Entry->Base.MagicalArmor, 0);
		TestTrue(TEXT("Physical attack"), Entry->Base.Attack.DamageType == EDYKDamageType::Physical);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKUnitCatalogSharedDataTest, "DYK.M102.SharedDefinitionsDataLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDYKUnitCatalogSharedDataTest::RunTest(const FString& Parameters)
{
	// A data-consumer fixture only: production health/effects/attack systems are later tasks.
	struct FConsumer
	{
		const FDYKUnitDefinition* Definition;
		int32 CurrentHP;
		bool bHasEffect = false;
	};
	const auto MakeConsumer = [](const FDYKUnitDefinition& Definition)
	{
		return FConsumer{&Definition, Definition.Base.MaxHP.Value};
	};
	for (const FDYKUnitDefinition& Definition : DYKUnitCatalog::GetAll())
	{
		TArray<FConsumer> Consumers;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Consumers.Add(MakeConsumer(*DYKUnitCatalog::Find(Definition.Id)));
		}
		Consumers[0].CurrentHP = 0;
		Consumers[0].bHasEffect = true;
		for (int32 Index = 0; Index < Consumers.Num(); ++Index)
		{
			TestTrue(TEXT("Twelve consumers share exact base address"), Consumers[Index].Definition == &Definition);
			TestEqual(TEXT("Individual current HP"), Consumers[Index].CurrentHP, Index == 0 ? 0 : Definition.Base.MaxHP.Value);
			TestEqual(TEXT("Individual effect state"), Consumers[Index].bHasEffect, Index == 0);
		}
		TestTrue(TEXT("Repeated lookup stable after local state mutations"), DYKUnitCatalog::Find(Definition.Id) == &Definition);
	}

	const auto* Original = DYKUnitCatalog::Find(DYKUnitIds::Peasant());
	if (!TestNotNull(TEXT("Base type for extension fixture"), Original))
	{
		return false;
	}
	FDYKUnitDefinition NewRecord = *Original;
	NewRecord.Id = FDYKUnitTypeId(TEXT("TEST-ONLY-DATA-EXTENSION"));
	NewRecord.Base.MaxHP = {31, EDYKDefinitionSource::DEV_PLACEHOLDER};
	NewRecord.Base.Attack.DamagePerHit = {9, EDYKDefinitionSource::DEV_PLACEHOLDER};
	const FDYKUnitDefinition ExtendedRecords[] = {*Original, NewRecord};
	// The same consumer accepts a newly compiled record using the same mechanism.
	TestEqual(TEXT("Original record consumed"), MakeConsumer(ExtendedRecords[0]).CurrentHP, 15);
	TestEqual(TEXT("New data record consumed without changing consumer"), MakeConsumer(ExtendedRecords[1]).CurrentHP, 31);
	TestTrue(TEXT("Extension retains mechanism"), ExtendedRecords[1].Movement == Original->Movement
		&& ExtendedRecords[1].Base.Attack.HitTiming == Original->Base.Attack.HitTiming);
	ExpectValue(*this, TEXT("Extension provenance retained"), ExtendedRecords[1].Base.MaxHP, 31, EDYKDefinitionSource::DEV_PLACEHOLDER);
	ExpectValue(*this, TEXT("Original remains immutable after copy modification"), Original->Base.MaxHP, 15);
	TestNull(TEXT("Test fixture does not register runtime content"), DYKUnitCatalog::Find(NewRecord.Id));
	return true;
}

#endif
