#include "../Saving/DYKSaveSubsystem.h"
#include "../Saving/DYKProfileSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

// Access only the storage boundary; tests still use production loading and commands.
struct FDYKProfileTestAccess
{
	static void SetPrefix(UDYKSaveSubsystem& Service, const FString& Prefix)
	{
		Service.SlotPrefix = Prefix;
	}
	static void Open(UDYKSaveSubsystem& Service, const FString& Prefix)
	{
		SetPrefix(Service, Prefix);
		Service.LoadOrCreateProfile();
	}
	static FString Path(const UDYKSaveSubsystem& Service, int32 Slot)
	{
		return Service.GetSavePath(Slot);
	}
};

namespace
{
	constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FDYKContentId Id(EDYKContentKind Kind, const TCHAR* Name)
	{
		return {Kind, FName(Name)};
	}

	FDYKPlayerProfileSaveData PopulatedProfile()
	{
		FDYKPlayerProfileSaveData Profile;
		Profile.Parcels.Add({FIntPoint(-2, 3), true});
		Profile.Parcels.Add({FIntPoint(4, -1), false});
		FDYKBuildingSaveData Building;
		Building.BuildingId = Id(EDYKContentKind::Building, TEXT("TEST-FARM"));
		Building.CellOrigin = FIntPoint(2, 7);
		Building.RotationQuarterTurns = 3;
		Building.Level = 2;
		Building.ConstructionStartedAtUnix = 1794700001;
		Building.ConstructionCompletesAtUnix = 1794703601;
		Profile.Buildings.Add(Building);
		Profile.Resources.Add({Id(EDYKContentKind::Resource, TEXT("TEST-FOOD")), 200});
		Profile.Resources.Add({Id(EDYKContentKind::Resource, TEXT("TEST-GOLD")), 12345678901LL});
		Profile.ResearchedContent.Add(Id(EDYKContentKind::Warrior, TEXT("TEST-ARCHER")));
		FDYKUpgradeSaveData Upgrade;
		Upgrade.UpgradeId = Id(EDYKContentKind::Building, TEXT("TEST-FARM-UPGRADE"));
		Upgrade.Level = 3;
		Upgrade.StartedAtUnix = 1794700100;
		Upgrade.CompletesAtUnix = 1794700200;
		Profile.Upgrades.Add(Upgrade);
		Profile.UnlockedContent.Add(Id(EDYKContentKind::Spell, TEXT("TEST-SPELL")));
		Profile.LastDailyRewardDateKey = 20261115;
		Profile.LastOfflineCalculationUnix = 1794700300;
		Profile.CampaignProgress.CampaignId = Id(EDYKContentKind::Campaign, TEXT("TEST-CAMPAIGN"));
		Profile.CampaignProgress.UnlockedMissions.Add(Id(EDYKContentKind::Mission, TEXT("TEST-MISSION-1")));
		Profile.CampaignProgress.UnlockedMissions.Add(Id(EDYKContentKind::Mission, TEXT("TEST-MISSION-2")));
		Profile.CampaignProgress.CompletedMissions.Add(Profile.CampaignProgress.UnlockedMissions[0]);
		Profile.CurrentMission.MissionId = Profile.CampaignProgress.UnlockedMissions[1];
		Profile.CurrentMission.CurrentWaveIndex = 4;
		Profile.CurrentMission.Supplies = 83;
		Profile.CurrentMission.Valor = 27;
		Profile.CurrentMission.bIsActive = true;
		Profile.CurrentMission.Deployment.Add({Id(EDYKContentKind::Slot, TEXT("TEST-SLOT")),
			Id(EDYKContentKind::Warrior, TEXT("TEST-ARCHER"))});
		return Profile;
	}

	bool SameProfile(const FDYKPlayerProfileSaveData* Actual, const FDYKPlayerProfileSaveData& Expected)
	{
		return Actual && FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Actual, &Expected, 0);
	}

	struct FStorageFixture
	{
		TStrongObjectPtr<UGameInstance> Owner{NewObject<UGameInstance>()};
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
			TEXT("M103"), TEXT("test-saves"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FString Prefix = FPaths::Combine(Directory, TEXT("profile"));

		~FStorageFixture()
		{
			IFileManager::Get().DeleteDirectory(*Directory, false, true);
		}

		UDYKSaveSubsystem* Open() const
		{
			auto* Service = NewObject<UDYKSaveSubsystem>(Owner.Get());
			FDYKProfileTestAccess::Open(*Service, Prefix);
			return Service;
		}

		bool Seed(UDYKSaveSubsystem& Service, const FDYKPlayerProfileSaveData& Profile) const
		{
			return Service.ApplyProfileCommand(FGuid::NewGuid(), [&Profile](FDYKPlayerProfileSaveData& Mutable)
			{
				Mutable = Profile;
				return true;
			});
		}

		bool Legacy(const FDYKPlayerProfileSaveData& Profile, int32 Schema = 1) const
		{
			IFileManager::Get().MakeDirectory(*Directory, true);
			auto* Save = NewObject<UDYKProfileSaveGame>();
			Save->SchemaVersion = Schema;
			Save->Profile = Profile;
			TArray<uint8> Bytes;
			return UGameplayStatics::SaveGameToMemory(Save, Bytes)
				&& FFileHelper::SaveArrayToFile(Bytes, *(Prefix + TEXT(".sav")));
		}
	};

	TArray<uint8> Read(const FString& Path)
	{
		TArray<uint8> Bytes;
		FFileHelper::LoadFileToArray(Bytes, *Path);
		return Bytes;
	}

	FString NewestPath(const UDYKSaveSubsystem& Service)
	{
		// Read revisions from the documented persisted header, independent of slot parity.
		FString Selected;
		int64 Highest = -1;
		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			const FString Path = FDYKProfileTestAccess::Path(Service, Slot);
			const TArray<uint8> Bytes = Read(Path);
			int64 Revision = -1;
			if (Bytes.Num() >= 44)
			{
				FMemory::Memcpy(&Revision, Bytes.GetData() + 8, sizeof(Revision));
			}
			if (Revision > Highest)
			{
				Highest = Revision;
				Selected = Path;
			}
		}
		return Selected;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileRoundTripTest, "DYK.M103.RoundTripAndTwoLoads", TestFlags)

bool FDYKProfileRoundTripTest::RunTest(const FString& Parameters)
{
	FStorageFixture Storage;
	auto* First = Storage.Open();
	TestTrue(TEXT("Missing save creates profile"), First->GetLoadStatus() == EDYKProfileLoadStatus::Created);
	const auto Expected = PopulatedProfile();
	if (!TestTrue(TEXT("Persist complete populated profile"), Storage.Seed(*First, Expected))) return false;
	const int64 Revision = First->GetRevision();
	TestTrue(TEXT("Revision increased"), Revision > 0);
	TestTrue(TEXT("First snapshot contains all data"), SameProfile(First->GetProfileSnapshot(), Expected));
	for (int32 Restart = 0; Restart < 2; ++Restart)
	{
		auto* Loaded = Storage.Open();
		TestTrue(TEXT("Restart successfully loads"), Loaded->GetLoadStatus() == EDYKProfileLoadStatus::Loaded);
		TestTrue(TEXT("Every reflected profile field survives restart"), SameProfile(Loaded->GetProfileSnapshot(), Expected));
		TestEqual(TEXT("Reading does not change confirmed revision"), Loaded->GetRevision(), Revision);
	}
	TestTrue(TEXT("Both alternating copies exist"), IFileManager::Get().FileExists(*FDYKProfileTestAccess::Path(*First, 0))
		&& IFileManager::Get().FileExists(*FDYKProfileTestAccess::Path(*First, 1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileMissingCopyTest, "DYK.M103.InitialCopiesAndMissingCopyRepair", TestFlags)

bool FDYKProfileMissingCopyTest::RunTest(const FString& Parameters)
{
	FStorageFixture Storage;
	auto* Service = Storage.Open();
	TestTrue(TEXT("Fresh profile status"), Service->GetLoadStatus() == EDYKProfileLoadStatus::Created);
	TestEqual(TEXT("Both initial writes confirmed"), Service->GetRevision(), int64(2));
	TestTrue(TEXT("Fresh A exists"), !Read(FDYKProfileTestAccess::Path(*Service, 0)).IsEmpty());
	TestTrue(TEXT("Fresh B exists"), !Read(FDYKProfileTestAccess::Path(*Service, 1)).IsEmpty());
	const auto Expected = PopulatedProfile();
	if (!TestTrue(TEXT("Seed profile before missing-copy recovery"), Storage.Seed(*Service, Expected))) return false;
	const FString ValidPath = NewestPath(*Service);
	const FString MissingPath = ValidPath == FDYKProfileTestAccess::Path(*Service, 0)
		? FDYKProfileTestAccess::Path(*Service, 1) : FDYKProfileTestAccess::Path(*Service, 0);
	const auto ValidBytes = Read(ValidPath);
	const int64 Revision = Service->GetRevision();
	TestTrue(TEXT("Remove older test copy only"), IFileManager::Get().Delete(*MissingPath));
	Service = Storage.Open();
	TestTrue(TEXT("Valid data survives missing copy"), SameProfile(Service->GetProfileSnapshot(), Expected));
	TestEqual(TEXT("Missing copy is repaired with next revision"), Service->GetRevision(), Revision + 1);
	TestTrue(TEXT("Missing copy recreated"), !Read(MissingPath).IsEmpty());
	TestTrue(TEXT("Original valid copy unchanged by repair"), Read(ValidPath) == ValidBytes);
	const int64 RepairedRevision = Service->GetRevision();
	Service = Storage.Open();
	TestTrue(TEXT("Repaired pair reloads exactly"), SameProfile(Service->GetProfileSnapshot(), Expected));
	TestEqual(TEXT("Complete pair does not rewrite at next load"), Service->GetRevision(), RepairedRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileIdempotencyTest, "DYK.M103.P18T12.IdempotentCommands", TestFlags)

bool FDYKProfileIdempotencyTest::RunTest(const FString& Parameters)
{
	FStorageFixture Storage;
	auto* Service = Storage.Open();
	if (!TestTrue(TEXT("Seed resources and upgrade"), Storage.Seed(*Service, PopulatedProfile()))) return false;
	const int64 Before = Service->GetRevision();
	const FGuid Event = FGuid::NewGuid();
	int32 Calls = 0;
	const auto Buy = [&Calls](FDYKPlayerProfileSaveData& Profile)
	{
		++Calls;
		Profile.Resources[0].Amount -= 30;
		++Profile.Upgrades[0].Level;
		return true;
	};
	TestTrue(TEXT("Purchase confirmed"), Service->ApplyProfileCommand(Event, Buy));
	TestEqual(TEXT("Purchase revision increments once"), Service->GetRevision(), Before + 1);
	TestTrue(TEXT("Repeated purchase returns prior confirmation"), Service->ApplyProfileCommand(Event, Buy));
	TestEqual(TEXT("Callback ran exactly once"), Calls, 1);
	TestEqual(TEXT("Balance debited once"), Service->GetProfileSnapshot()->Resources[0].Amount, int64(170));
	TestEqual(TEXT("Upgrade granted once"), Service->GetProfileSnapshot()->Upgrades[0].Level, 4);
	TestEqual(TEXT("Repeat cannot advance revision"), Service->GetRevision(), Before + 1);
	Service = Storage.Open();
	if (!TestNotNull(TEXT("Idempotency reload has profile"), Service->GetProfileSnapshot())) return false;
	TestTrue(TEXT("Repeated event after restart confirmed"), Service->ApplyProfileCommand(Event, Buy));
	TestEqual(TEXT("Restart still does not rerun callback"), Calls, 1);
	TestEqual(TEXT("Restart keeps balance"), Service->GetProfileSnapshot()->Resources[0].Amount, int64(170));
	TestEqual(TEXT("Restart keeps revision"), Service->GetRevision(), Before + 1);
	const auto Snapshot = *Service->GetProfileSnapshot();
	TestFalse(TEXT("Rejected command returns failure"), Service->ApplyProfileCommand(FGuid::NewGuid(),
		[](FDYKPlayerProfileSaveData& Profile) { Profile.Resources[0].Amount = -99; return false; }));
	TestTrue(TEXT("Rejected command leaves no partial mutation"), SameProfile(Service->GetProfileSnapshot(), Snapshot));
	TestEqual(TEXT("Rejected command does not increment revision"), Service->GetRevision(), Before + 1);
	TestFalse(TEXT("Empty event ID cannot mutate profile"), Service->ApplyProfileCommand(FGuid(), Buy));
	TestEqual(TEXT("Empty ID does not execute command"), Calls, 1);
	const FGuid NestedEvent = FGuid::NewGuid();
	bool bNestedRan = false;
	TestTrue(TEXT("Outer command commits once"), Service->ApplyProfileCommand(FGuid::NewGuid(),
		[&](FDYKPlayerProfileSaveData& Profile)
		{
			TestFalse(TEXT("Nested checkpoint cannot overwrite transaction"), Service->SaveProfile());
			TestFalse(TEXT("Nested command is refused"), Service->ApplyProfileCommand(NestedEvent,
				[&](FDYKPlayerProfileSaveData&) { bNestedRan = true; return true; }));
			Profile.Resources[0].Amount -= 10;
			return true;
		}));
	TestFalse(TEXT("Refused nested callback did not run"), bNestedRan);
	TestEqual(TEXT("Only outer debit committed"), Service->GetProfileSnapshot()->Resources[0].Amount, int64(160));
	TestEqual(TEXT("Only outer revision committed"), Service->GetRevision(), Before + 2);
	TestTrue(TEXT("Refused nested event remains retryable"), Service->ApplyProfileCommand(NestedEvent,
		[&](FDYKPlayerProfileSaveData& Profile) { bNestedRan = true; Profile.Resources[0].Amount -= 5; return true; }));
	TestTrue(TEXT("Retry executes previously refused event"), bNestedRan);
	TestEqual(TEXT("Nested retry debit committed once"), Service->GetProfileSnapshot()->Resources[0].Amount, int64(155));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileWriteRetryTest, "DYK.M103.WriteFailureAndRetry", TestFlags)

bool FDYKProfileWriteRetryTest::RunTest(const FString& Parameters)
{
	FStorageFixture Storage;
	auto* Service = Storage.Open();
	const auto Before = PopulatedProfile();
	if (!TestTrue(TEXT("Seed confirmed state"), Storage.Seed(*Service, Before))) return false;
	const int64 Revision = Service->GetRevision();
	const FString BlockedParent = FPaths::Combine(Storage.Directory, TEXT("blocked-parent"));
	const FString RetryPrefix = FPaths::Combine(BlockedParent, TEXT("profile"));
	const TArray<uint8> Obstacle{0x71};
	if (!TestTrue(TEXT("Create real filesystem obstruction"), FFileHelper::SaveArrayToFile(Obstacle, *BlockedParent))) return false;
	FDYKProfileTestAccess::SetPrefix(*Service, RetryPrefix);
	const FGuid Event = FGuid::NewGuid();
	const auto Purchase = [](FDYKPlayerProfileSaveData& Profile)
	{
		Profile.Resources[0].Amount -= 30;
		++Profile.Upgrades[0].Level;
		return true;
	};
	TestFalse(TEXT("Failed disk write is not confirmed"), Service->ApplyProfileCommand(Event, Purchase));
	TestTrue(TEXT("Write failure provides error"), !Service->GetLoadError().IsEmpty());
	TestTrue(TEXT("Failed write keeps confirmed snapshot"), SameProfile(Service->GetProfileSnapshot(), Before));
	TestEqual(TEXT("Failed write keeps confirmed revision"), Service->GetRevision(), Revision);
	TestTrue(TEXT("Remove only the test obstruction"), IFileManager::Get().Delete(*BlockedParent));
	TestTrue(TEXT("Same event can retry successfully"), Service->ApplyProfileCommand(Event, Purchase));
	TestEqual(TEXT("Retry debits only once"), Service->GetProfileSnapshot()->Resources[0].Amount, int64(170));
	TestEqual(TEXT("Retry grants upgrade only once"), Service->GetProfileSnapshot()->Upgrades[0].Level, 4);
	TestEqual(TEXT("Retry advances revision once"), Service->GetRevision(), Revision + 1);
	TestTrue(TEXT("Retry clears stale error"), Service->GetLoadError().IsEmpty());
	auto* Restarted = NewObject<UDYKSaveSubsystem>(Storage.Owner.Get());
	FDYKProfileTestAccess::Open(*Restarted, RetryPrefix);
	if (!TestNotNull(TEXT("Retry reload has profile"), Restarted->GetProfileSnapshot())) return false;
	TestEqual(TEXT("Retried purchase survives restart"), Restarted->GetProfileSnapshot()->Resources[0].Amount, int64(170));
	TestTrue(TEXT("Retried event persisted"), Restarted->ApplyProfileCommand(Event, Purchase));
	TestEqual(TEXT("Duplicate retry cannot debit again"), Restarted->GetProfileSnapshot()->Resources[0].Amount, int64(170));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileRecoveryTest, "DYK.M103.P18T14.RecoverCorruptOrInterruptedNewest", TestFlags)

bool FDYKProfileRecoveryTest::RunTest(const FString& Parameters)
{
	for (bool bTruncated : {false, true})
	{
		FStorageFixture Storage;
		auto* Service = Storage.Open();
		const auto Previous = PopulatedProfile();
		if (!TestTrue(TEXT("Seed previous revision"), Storage.Seed(*Service, Previous))) return false;
		const int64 PreviousRevision = Service->GetRevision();
		TestTrue(TEXT("Write next revision"), Service->ApplyProfileCommand(FGuid::NewGuid(),
			[](FDYKPlayerProfileSaveData& Profile) { Profile.Resources[0].Amount = 999; return true; }));
		const FString DamagedPath = NewestPath(*Service);
		const FString OtherPath = DamagedPath == FDYKProfileTestAccess::Path(*Service, 0)
			? FDYKProfileTestAccess::Path(*Service, 1) : FDYKProfileTestAccess::Path(*Service, 0);
		const auto OtherBytes = Read(OtherPath);
		auto Bytes = Read(DamagedPath);
		if (!TestTrue(TEXT("Valid envelope available for corruption"), Bytes.Num() > 44)) return false;
		if (bTruncated) Bytes.SetNum(19);
		else Bytes.Last() ^= 0x7f;
		TestTrue(TEXT("Write simulated interrupted/corrupted save"), FFileHelper::SaveArrayToFile(Bytes, *DamagedPath));
		auto* Recovered = Storage.Open();
		TestTrue(TEXT("Recovery status is explicit"), Recovered->GetLoadStatus() == EDYKProfileLoadStatus::Recovered);
		TestTrue(TEXT("Last valid profile selected exactly"), SameProfile(Recovered->GetProfileSnapshot(), Previous));
		TestEqual(TEXT("Last valid revision selected"), Recovered->GetRevision(), PreviousRevision);
		TestTrue(TEXT("Damaged original preserved byte for byte"), Read(DamagedPath) == Bytes);
		TestTrue(TEXT("Valid backup preserved during recovery"), Read(OtherPath) == OtherBytes);
		TestTrue(TEXT("Recovered profile accepts next command"), Recovered->ApplyProfileCommand(FGuid::NewGuid(),
			[](FDYKPlayerProfileSaveData& Profile) { Profile.Resources[0].Amount -= 7; return true; }));
		TArray<FString> Preserved;
		IFileManager::Get().FindFiles(Preserved, *(DamagedPath + TEXT(".corrupt-*")), true, false);
		if (TestEqual(TEXT("Damaged original quarantined once"), Preserved.Num(), 1))
		{
			TestTrue(TEXT("Quarantined bytes match damaged original"),
				Read(FPaths::Combine(FPaths::GetPath(DamagedPath), Preserved[0])) == Bytes);
		}
		TestTrue(TEXT("Next write preserves valid older copy"), Read(OtherPath) == OtherBytes);
		auto NextExpected = Previous;
		NextExpected.Resources[0].Amount -= 7;
		auto* Restarted = Storage.Open();
		TestTrue(TEXT("Recovery followed by save survives restart"), SameProfile(Restarted->GetProfileSnapshot(), NextExpected));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileCorruptCopiesTest, "DYK.M103.P18T14.BothCopiesCorrupt", TestFlags)

bool FDYKProfileCorruptCopiesTest::RunTest(const FString& Parameters)
{
	FStorageFixture Storage;
	auto* Service = Storage.Open();
	if (!TestTrue(TEXT("Create two populated copies"), Storage.Seed(*Service, PopulatedProfile()))) return false;
	const TArray<uint8> InvalidA{0x11, 0x22, 0x33};
	const TArray<uint8> InvalidB{0x44, 0x55};
	const FString PathA = FDYKProfileTestAccess::Path(*Service, 0);
	const FString PathB = FDYKProfileTestAccess::Path(*Service, 1);
	TestTrue(TEXT("Damage A"), FFileHelper::SaveArrayToFile(InvalidA, *PathA));
	TestTrue(TEXT("Damage B"), FFileHelper::SaveArrayToFile(InvalidB, *PathB));
	Service = Storage.Open();
	TestTrue(TEXT("Both corrupt detected"), Service->GetLoadStatus() == EDYKProfileLoadStatus::Corrupt);
	TestNull(TEXT("No silent empty replacement"), Service->GetProfileSnapshot());
	TestTrue(TEXT("Useful load error provided"), !Service->GetLoadError().IsEmpty());
	TestFalse(TEXT("Explicit save cannot overwrite originals"), Service->SaveProfile());
	TestFalse(TEXT("Command cannot reset corrupt profile"), Service->ApplyProfileCommand(FGuid::NewGuid(),
		[](FDYKPlayerProfileSaveData&) { return true; }));
	TestTrue(TEXT("A untouched"), Read(PathA) == InvalidA);
	TestTrue(TEXT("B untouched"), Read(PathB) == InvalidB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileLegacyTest, "DYK.M103.LegacyMigrationWithoutLoss", TestFlags)

bool FDYKProfileLegacyTest::RunTest(const FString& Parameters)
{
	FStorageFixture Storage;
	const auto Expected = PopulatedProfile();
	if (!TestTrue(TEXT("Create authentic v1 raw USaveGame fixture"), Storage.Legacy(Expected))) return false;
	const FString Path = Storage.Prefix + TEXT(".sav");
	const auto Original = Read(Path);
	auto* Migrated = Storage.Open();
	TestTrue(TEXT("All v1 data migrated without loss"), SameProfile(Migrated->GetProfileSnapshot(), Expected));
	TestTrue(TEXT("Migration produces confirmed revision"), Migrated->GetRevision() > 0);
	const int64 Revision = Migrated->GetRevision();
	TestTrue(TEXT("Legacy original preserved"), Read(Path) == Original);
	for (int32 Restart = 0; Restart < 2; ++Restart)
	{
		auto* Loaded = Storage.Open();
		TestTrue(TEXT("Restart preserves buildings/rewards/research/resources and all other data"), SameProfile(Loaded->GetProfileSnapshot(), Expected));
		TestEqual(TEXT("No repeated migration revision"), Loaded->GetRevision(), Revision);
		TestTrue(TEXT("Legacy bytes remain untouched on restart"), Read(Path) == Original);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileRealLegacyTest, "DYK.M103.RealLegacyFixtureMigration", TestFlags)

bool FDYKProfileRealLegacyTest::RunTest(const FString& Parameters)
{
	TArray<uint8> CurrentEmpty;
	if (UGameplayStatics::SaveGameToMemory(NewObject<UDYKProfileSaveGame>(), CurrentEmpty))
	{
		FFileHelper::SaveArrayToFile(CurrentEmpty, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/M103/current-empty.sav")));
	}
	const FString FixturePath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Source/DefendYourKingdom/Tests/Fixtures/ProfileSchema1.sav"));
	const auto Bytes = Read(FixturePath);
	if (!TestTrue(TEXT("Real pre-change schema1 fixture is available"), !Bytes.IsEmpty())) return false;
	const ANSICHAR* LedgerName = "AppliedCommandIds";
	const int32 LedgerNameLength = FCStringAnsi::Strlen(LedgerName);
	bool bContainsLedger = false;
	for (int32 Index = 0; Index + LedgerNameLength <= Bytes.Num(); ++Index)
	{
		bContainsLedger |= FMemory::Memcmp(Bytes.GetData() + Index, LedgerName, LedgerNameLength) == 0;
	}
	TestFalse(TEXT("Fixture predates command ledger property"), bContainsLedger);
	auto* Original = Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Legacy engine save remains readable"), Original)) return false;
	TestEqual(TEXT("Actual old schema is v1"), Original->SchemaVersion, 1);
	TestEqual(TEXT("Absent old ledger defaults empty"), Original->AppliedCommandIds.Num(), 0);
	const auto Expected = Original->Profile;
	FStorageFixture Storage;
	IFileManager::Get().MakeDirectory(*Storage.Directory, true);
	const FString LegacyPath = Storage.Prefix + TEXT(".sav");
	if (!TestTrue(TEXT("Copy real old bytes to isolated migration input"),
		FFileHelper::SaveArrayToFile(Bytes, *LegacyPath))) return false;
	auto* Migrated = Storage.Open();
	AddInfo(FString::Printf(TEXT("Real legacy migration status=%d, error=%s"),
		static_cast<int32>(Migrated->GetLoadStatus()), *Migrated->GetLoadError()));
	TestTrue(TEXT("Every old profile field preserved by migration"), SameProfile(Migrated->GetProfileSnapshot(), Expected));
	if (!TestNotNull(TEXT("Migrated real fixture active"), Migrated->GetActiveProfile())) return false;
	TestEqual(TEXT("Migration does not invent applied events"), Migrated->GetActiveProfile()->AppliedCommandIds.Num(), 0);
	TestEqual(TEXT("Migration creates both confirmed copies"), Migrated->GetRevision(), int64(2));
	TestTrue(TEXT("Pre-change original bytes preserved"), Read(LegacyPath) == Bytes);
	Migrated = Storage.Open();
	TestTrue(TEXT("Migrated real fixture reloads without loss"), SameProfile(Migrated->GetProfileSnapshot(), Expected));
	TestEqual(TEXT("Real fixture migration runs once"), Migrated->GetRevision(), int64(2));
	TestTrue(TEXT("Real fixture remains untouched after restart"), Read(LegacyPath) == Bytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileLegacyCorruptTest, "DYK.M103.LegacyCorruptAndTruncated", TestFlags)

bool FDYKProfileLegacyCorruptTest::RunTest(const FString& Parameters)
{
	const auto CheckRejected = [this](const TArray<uint8>& Bytes, const FString& Label)
	{
		FStorageFixture Storage;
		IFileManager::Get().MakeDirectory(*Storage.Directory, true);
		const FString Path = Storage.Prefix + TEXT(".sav");
		if (!TestTrue(Label + TEXT(" fixture written"), FFileHelper::SaveArrayToFile(Bytes, *Path))) return;
		auto* Service = Storage.Open();
		TestTrue(Label + TEXT(" corruption detected"), Service->GetLoadStatus() == EDYKProfileLoadStatus::Corrupt);
		TestNull(Label + TEXT(" no partial profile"), Service->GetProfileSnapshot());
		TestFalse(Label + TEXT(" no save allowed"), Service->SaveProfile());
		TestTrue(Label + TEXT(" original bytes retained"), Read(Path) == Bytes);
		TestFalse(Label + TEXT(" no A generated"), IFileManager::Get().FileExists(*FDYKProfileTestAccess::Path(*Service, 0)));
		TestFalse(Label + TEXT(" no B generated"), IFileManager::Get().FileExists(*FDYKProfileTestAccess::Path(*Service, 1)));
	};
	CheckRejected(TArray<uint8>{0x53, 0x41, 0x56, 0x47, 0x77, 0x32, 0x19}, TEXT("Random legacy bytes"));
	const auto OldBytes = Read(FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Source/DefendYourKingdom/Tests/Fixtures/ProfileSchema1.sav")));
	if (!TestTrue(TEXT("Real old fixture available for truncation"), OldBytes.Num() > 17)) return false;
	for (int32 Removed : {1, 17})
	{
		auto Truncated = OldBytes;
		Truncated.SetNum(Truncated.Num() - Removed);
		CheckRejected(Truncated, FString::Printf(TEXT("Real legacy truncated %d bytes"), Removed));
	}
	if (TestTrue(TEXT("Real fixture includes custom-version format header"), OldBytes.Num() > 52))
	{
		auto InvalidFormat = OldBytes;
		const int32 UnsupportedFormat = 7;
		FMemory::Memcpy(InvalidFormat.GetData() + 48, &UnsupportedFormat, sizeof(UnsupportedFormat));
		CheckRejected(InvalidFormat, TEXT("Legacy invalid custom-version header"));
	}
	auto* Populated = NewObject<UDYKProfileSaveGame>();
	Populated->Profile = PopulatedProfile();
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Serialize populated nested-property truncation fixture"),
		UGameplayStatics::SaveGameToMemory(Populated, Bytes))) return false;
	const TArray<uint8> NoneMarker{5, 0, 0, 0, 'N', 'o', 'n', 'e', 0, 0, 0, 0, 0};
	if (!TestTrue(TEXT("Populated profile adds serialized fields beyond default fixture"), Bytes.Num() > OldBytes.Num() + 100)) return false;
	TArray<uint8> TruncatedProfile;
	// Keep the original outer property size but end halfway through its actual data.
	const int32 CutInsideProfile = OldBytes.Num() + (Bytes.Num() - OldBytes.Num()) / 2;
	TruncatedProfile.Append(Bytes.GetData(), CutInsideProfile);
	TruncatedProfile.Append(NoneMarker);
	// Additive saved fields can move the synthetic midpoint inside a nested array.
	// Both diagnostics describe this intentionally truncated Profile payload; all
	// corrupt/no-partial-profile/no-overwrite assertions below remain mandatory.
	AddExpectedError(TEXT("Failed loading tagged StructProperty /Script/DefendYourKingdom.DYKProfileSaveGame:Profile"),
		EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Failed loading tagged ArrayProperty /Script/DefendYourKingdom.DYKPlayerProfileSaveData:Upgrades"),
		EAutomationExpectedErrorFlags::Contains, 1);
	CheckRejected(TruncatedProfile, TEXT("Profile payload cut with valid final None"));
	int32 InteriorMarkers = 0;
	for (int32 Offset = 0; Offset + NoneMarker.Num() < Bytes.Num(); ++Offset)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Offset, NoneMarker.GetData(), NoneMarker.Num()) == 0)
		{
			TArray<uint8> Truncated;
			Truncated.Append(Bytes.GetData(), Offset + NoneMarker.Num());
			CheckRejected(Truncated, FString::Printf(TEXT("Nested None truncation at %d"), Offset));
			++InteriorMarkers;
		}
	}
	AddInfo(FString::Printf(TEXT("Nested None truncation boundaries tested: %d; explicit partial Profile payload tested independently."), InteriorMarkers));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileFutureVersionTest, "DYK.M103.UnknownFutureVersion", TestFlags)

bool FDYKProfileFutureVersionTest::RunTest(const FString& Parameters)
{
	FStorageFixture Storage;
	auto* Service = Storage.Open();
	if (!TestTrue(TEXT("Create supported save"), Storage.Seed(*Service, PopulatedProfile()))) return false;
	const FString Path = NewestPath(*Service);
	auto Bytes = Read(Path);
	if (!TestTrue(TEXT("Envelope header present"), Bytes.Num() > 44)) return false;
	const int32 FutureSchema = UDYKProfileSaveGame::CurrentSchemaVersion + 1;
	FMemory::Memcpy(Bytes.GetData() + 16, &FutureSchema, sizeof(FutureSchema));
	FSHA1 Hash;
	Hash.Update(Bytes.GetData(), 24);
	Hash.Update(Bytes.GetData() + 44, Bytes.Num() - 44);
	Hash.Final();
	Hash.GetHash(Bytes.GetData() + 24);
	TestTrue(TEXT("Write authentic checksum with unknown future schema"), FFileHelper::SaveArrayToFile(Bytes, *Path));
	const auto OtherA = Read(FDYKProfileTestAccess::Path(*Service, 0));
	const auto OtherB = Read(FDYKProfileTestAccess::Path(*Service, 1));
	Service = Storage.Open();
	TestTrue(TEXT("Future version blocks obsolete fallback"), Service->GetLoadStatus() == EDYKProfileLoadStatus::UnsupportedVersion);
	TestNull(TEXT("Future version not silently reset"), Service->GetProfileSnapshot());
	TestFalse(TEXT("Future version cannot be overwritten"), Service->SaveProfile());
	TestTrue(TEXT("A preserved"), Read(FDYKProfileTestAccess::Path(*Service, 0)) == OtherA);
	TestTrue(TEXT("B preserved"), Read(FDYKProfileTestAccess::Path(*Service, 1)) == OtherB);
	FStorageFixture LegacyStorage;
	TestTrue(TEXT("Write unknown legacy schema"), LegacyStorage.Legacy(PopulatedProfile(), FutureSchema));
	const FString LegacyPath = LegacyStorage.Prefix + TEXT(".sav");
	const auto LegacyBytes = Read(LegacyPath);
	auto* Legacy = LegacyStorage.Open();
	TestTrue(TEXT("Unknown legacy schema rejected"), Legacy->GetLoadStatus() == EDYKProfileLoadStatus::UnsupportedVersion);
	TestFalse(TEXT("Unknown legacy schema cannot be overwritten"), Legacy->SaveProfile());
	TestTrue(TEXT("Unknown legacy bytes untouched"), Read(LegacyPath) == LegacyBytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProfileProcessRestartTest, "DYK.M103.ProcessRestart", TestFlags)

bool FDYKProfileProcessRestartTest::RunTest(const FString& Parameters)
{
	FString Phase, Prefix;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-M103RestartPhase="), Phase))
	{
		AddInfo(TEXT("Optional process-restart scenario omitted: run dedicated write/read OS processes with -M103RestartPhase and -M103RestartPrefix."));
		return true;
	}
	if (!TestTrue(TEXT("Dedicated restart prefix supplied"),
		FParse::Value(FCommandLine::Get(), TEXT("-M103RestartPrefix="), Prefix) && !Prefix.IsEmpty())) return false;
	if (!TestTrue(TEXT("Restart phase supported"), Phase == TEXT("write") || Phase == TEXT("read"))) return false;
	TStrongObjectPtr<UGameInstance> Owner(NewObject<UGameInstance>());
	auto* Service = NewObject<UDYKSaveSubsystem>(Owner.Get());
	FDYKProfileTestAccess::Open(*Service, Prefix);
	const FGuid Event(0x4d313033, 0xa52ec109, 0xb16d0372, 0x04060a0c);
	const auto Expected = PopulatedProfile();
	if (Phase == TEXT("write"))
	{
		TestTrue(TEXT("Dedicated write starts with fresh storage"), Service->GetLoadStatus() == EDYKProfileLoadStatus::Created);
		TestTrue(TEXT("Write fixed event and full progress"), Service->ApplyProfileCommand(Event,
			[&Expected](FDYKPlayerProfileSaveData& Profile) { Profile = Expected; return true; }));
		TestTrue(TEXT("Write has exact expected profile"), SameProfile(Service->GetProfileSnapshot(), Expected));
		TestEqual(TEXT("Write revision is confirmed"), Service->GetRevision(), int64(3));
		AddInfo(FString::Printf(TEXT("Process write confirmed revision %lld at %s"), Service->GetRevision(), *Prefix));
		return true;
	}
	TestTrue(TEXT("New process loads existing progress"), Service->GetLoadStatus() == EDYKProfileLoadStatus::Loaded);
	TestTrue(TEXT("New process restores every saved field"), SameProfile(Service->GetProfileSnapshot(), Expected));
	TestEqual(TEXT("New process restores confirmed revision"), Service->GetRevision(), int64(3));
	bool bCallbackRan = false;
	TestTrue(TEXT("New process recognizes persisted event"), Service->ApplyProfileCommand(Event,
		[&bCallbackRan](FDYKPlayerProfileSaveData& Profile)
		{
			bCallbackRan = true;
			Profile.Resources.Reset();
			return true;
		}));
	TestFalse(TEXT("Repeated event cannot execute after OS restart"), bCallbackRan);
	TestTrue(TEXT("Repeated event preserves all progress"), SameProfile(Service->GetProfileSnapshot(), Expected));
	TestEqual(TEXT("Repeated event preserves revision after OS restart"), Service->GetRevision(), int64(3));
	AddInfo(FString::Printf(TEXT("Process read confirmed unchanged revision %lld at %s"), Service->GetRevision(), *Prefix));
	return true;
}

#endif
