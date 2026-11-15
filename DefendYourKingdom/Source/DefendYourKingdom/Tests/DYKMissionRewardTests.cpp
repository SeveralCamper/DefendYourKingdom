#include "../Meta/DYKMissionRewards.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Battle/DYKBattleSession.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "DYKMissionRewardTestAccess.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr auto MissionRewardFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FDYKResourceId RewardWood(TEXT("RES-WOOD")), RewardGold(TEXT("RES-GOLD")), RewardCustom(TEXT("QA-RESOURCE"));
	struct FRewardStorage
	{
		TStrongObjectPtr<UGameInstance> Owner{NewObject<UGameInstance>()};
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("M109/test-saves"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FString Prefix = FPaths::Combine(Directory, TEXT("profile"));
		~FRewardStorage() { IFileManager::Get().DeleteDirectory(*Directory, false, true); }
		UDYKSaveSubsystem* Open() const
		{
			auto* Save = NewObject<UDYKSaveSubsystem>(Owner.Get());
			FDYKMissionRewardTestAccess::Open(*Save, Prefix);
			return Save;
		}
	};
	FDYKBattleResult RewardResult(EDYKBattlePhase Outcome = EDYKBattlePhase::Victory)
	{
		FDYKBattleResult Result;
		Result.AttemptId = FGuid::NewGuid(); Result.Outcome = Outcome;
		Result.CompletedWaves = 2; Result.EnemiesKilled = 6; Result.ActiveSeconds = 3.5;
		return Result;
	}
	bool SameRewardProfile(const FDYKPlayerProfileSaveData* Actual, const FDYKPlayerProfileSaveData& Expected)
	{
		return Actual && FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Actual, &Expected, 0);
	}
	int64 RewardRemainder(const FDYKPlayerProfileSaveData& P, FName Source, FDYKResourceId Id)
	{
		const auto* Receipt = P.MissionRewards.FindByPredicate([Source](const auto& R) { return R.RewardId == Source; });
		if (Receipt) for (const auto& R : Receipt->Remaining) if (R.ResourceId == Id.ToContentId()) return R.Amount;
		return 0;
	}
	void RewardSetBalance(FDYKPlayerProfileSaveData& P, FDYKResourceId Id, int64 Value)
	{
		auto* Row = P.Resources.FindByPredicate([Id](const auto& R) { return R.ResourceId == Id.ToContentId(); });
		if (Row) Row->Amount = Value; else P.Resources.Add({Id.ToContentId(), Value});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRewardOutcomeTest, "DYK.M109.P11T15.OutcomeReplayAndPersistence", MissionRewardFlags)
bool FDYKRewardOutcomeTest::RunTest(const FString& Parameters)
{
	FRewardStorage Storage;
	auto* Save = Storage.Open();
	const auto Def = DYKMissionRewards::PolygonDEV_PLACEHOLDER();
	const auto Before = *Save->GetProfileSnapshot();
	const int64 Revision = Save->GetRevision();
	for (auto Phase : {EDYKBattlePhase::Defeat, EDYKBattlePhase::Aborted, EDYKBattlePhase::ActiveWave, EDYKBattlePhase::InitialPreparation})
	{
		TestFalse(TEXT("Only victory may issue permanent resources"), DYKMissionRewards::Settle(*Save, RewardResult(Phase), Def));
		TestTrue(TEXT("Nonvictory leaves whole profile unchanged"), SameRewardProfile(Save->GetProfileSnapshot(), Before));
		TestEqual(TEXT("Nonvictory makes no durable write"), Save->GetRevision(), Revision);
	}
	const auto Win = RewardResult();
	TestTrue(TEXT("First victory durably settles"), DYKMissionRewards::Settle(*Save, Win, Def));
	const auto Awarded = *Save->GetProfileSnapshot();
	TestEqual(TEXT("One mission completion"), Awarded.MissionCompletions.Num(), 1);
	TestTrue(TEXT("Completion belongs to actual configured campaign and mission"), Awarded.MissionCompletions[0].CampaignId == Def.Campaign.ToContentId() && Awarded.MissionCompletions[0].MissionId == Def.Mission.ToContentId());
	TestEqual(TEXT("One receipt even when everything delivered"), Awarded.MissionRewards.Num(), 1);
	TestTrue(TEXT("Delivered receipt retained as issued flag"), Awarded.MissionRewards[0].Remaining.IsEmpty());
	for (const auto& R : Def.Resources) TestEqual(TEXT("Configured resource amounts awarded"), DYKCityEconomy::Balance(Awarded, FDYKResourceId::FromContentId(R.ResourceId).GetValue()), R.Amount);
	const int64 AwardRevision = Save->GetRevision();
	TestTrue(TEXT("Duplicate result callback acknowledged"), DYKMissionRewards::Settle(*Save, Win, Def));
	TestEqual(TEXT("Duplicate callback adds no revision"), Save->GetRevision(), AwardRevision);
	TestTrue(TEXT("Callback preserves every field"), SameRewardProfile(Save->GetProfileSnapshot(), Awarded));
	Save = Storage.Open();
	TestTrue(TEXT("Receipt/completion/resources survive storage reload"), SameRewardProfile(Save->GetProfileSnapshot(), Awarded));
	TestTrue(TEXT("New attempt can finish same mission"), DYKMissionRewards::Settle(*Save, RewardResult(), Def));
	TestTrue(TEXT("New victory cannot farm one-time source"), SameRewardProfile(Save->GetProfileSnapshot(), Awarded));
	TestTrue(TEXT("Original callback after restart remains idempotent"), DYKMissionRewards::Settle(*Save, Win, Def));
	TestTrue(TEXT("Repeated display after restart cannot duplicate reward"), SameRewardProfile(Save->GetProfileSnapshot(), Awarded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRewardOverflowTest, "DYK.M109.CITY1302.PartialOverflowAndCampaignIsolation", MissionRewardFlags)
bool FDYKRewardOverflowTest::RunTest(const FString& Parameters)
{
	FRewardStorage Storage;
	auto* Save = Storage.Open();
	const auto Def = DYKMissionRewards::PolygonDEV_PLACEHOLDER();
	TestTrue(TEXT("Seed city near warehouse limit"), Save->ApplyProfileCommand(FGuid::NewGuid(), [](auto& P)
	{
		if (!DYKCityEconomy::Initialize(P, 1794700000)) return false;
		RewardSetBalance(P, RewardWood, 95); return true;
	}));
	TestTrue(TEXT("Victory saves full entitlement despite capacity"), DYKMissionRewards::Settle(*Save, RewardResult(), Def));
	TestEqual(TEXT("Only free warehouse space transferred"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardWood), int64(100));
	TestEqual(TEXT("Twenty wood remains available in campaign receipt"), RewardRemainder(*Save->GetProfileSnapshot(), Def.RewardId, RewardWood), int64(20));
	TestFalse(TEXT("Full warehouse has no collectable amount"), DYKMissionRewards::HasCollectable(*Save->GetProfileSnapshot(), Def.Campaign));
	const auto Full = *Save->GetProfileSnapshot();
	TestFalse(TEXT("Collect with full warehouse rejected"), DYKMissionRewards::Collect(*Save, FGuid::NewGuid(), Def.Campaign));
	TestTrue(TEXT("Full collection never destroys pending amount"), SameRewardProfile(Save->GetProfileSnapshot(), Full));
	Save = Storage.Open();
	TestTrue(TEXT("Pending entitlement persists"), SameRewardProfile(Save->GetProfileSnapshot(), Full));
	TestTrue(TEXT("Spend seven makes partial room"), DYKCityEconomy::Spend(*Save, FGuid::NewGuid(), RewardWood, 7));
	TestFalse(TEXT("Different campaign cannot collect this source"), DYKMissionRewards::Collect(*Save, FGuid::NewGuid(), FDYKCampaignId(TEXT("OTHER-CAMPAIGN"))));
	TestTrue(TEXT("Correct campaign enables collection"), DYKMissionRewards::HasCollectable(*Save->GetProfileSnapshot(), Def.Campaign));
	const FGuid CollectId = FGuid::NewGuid();
	TestTrue(TEXT("Collect transfers exactly seven"), DYKMissionRewards::Collect(*Save, CollectId, Def.Campaign));
	TestEqual(TEXT("Thirteen remain after partial claim"), RewardRemainder(*Save->GetProfileSnapshot(), Def.RewardId, RewardWood), int64(13));
	TestTrue(TEXT("Further room opened"), DYKCityEconomy::Spend(*Save, FGuid::NewGuid(), RewardWood, 20));
	const auto BeforeDuplicate = *Save->GetProfileSnapshot();
	TestTrue(TEXT("Retried old collect ID acknowledged"), DYKMissionRewards::Collect(*Save, CollectId, Def.Campaign));
	TestTrue(TEXT("Old collection ID cannot claim fresh room"), SameRewardProfile(Save->GetProfileSnapshot(), BeforeDuplicate));
	TestTrue(TEXT("New collection ID claims final thirteen"), DYKMissionRewards::Collect(*Save, FGuid::NewGuid(), Def.Campaign));
	TestEqual(TEXT("Conserved final balance after 27 spent"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardWood), int64(93));
	TestTrue(TEXT("Empty source retained as issued receipt"), Save->GetProfileSnapshot()->MissionRewards[0].Remaining.IsEmpty());
	const auto Final = *Save->GetProfileSnapshot();
	TestTrue(TEXT("Another win cannot restore entitlement"), DYKMissionRewards::Settle(*Save, RewardResult(), Def));
	TestTrue(TEXT("No duplicate remaining or transferred amount"), SameRewardProfile(Save->GetProfileSnapshot(), Final));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRewardValidationTest, "DYK.M109.InvalidConfigurationAndGenericResources", MissionRewardFlags)
bool FDYKRewardValidationTest::RunTest(const FString& Parameters)
{
	FRewardStorage Storage;
	auto* Save = Storage.Open();
	const auto Good = DYKMissionRewards::PolygonDEV_PLACEHOLDER();
	const auto Before = *Save->GetProfileSnapshot();
	for (int32 Case = 0; Case < 7; ++Case)
	{
		auto Def = Good; auto Win = RewardResult();
		if (Case == 0) Def.Campaign = {};
		if (Case == 1) Def.Mission = {};
		if (Case == 2) Def.RewardId = NAME_None;
		if (Case == 3) Def.Resources[0].Amount = -1;
		if (Case == 4) Def.Resources[0].ResourceId.Kind = EDYKContentKind::Building;
		if (Case == 5) { const auto Duplicate = Def.Resources[0]; Def.Resources.Add(Duplicate); }
		if (Case == 6) Win.AttemptId.Invalidate();
		TestFalse(TEXT("Malformed result/definition rejected"), DYKMissionRewards::Settle(*Save, Win, Def));
		TestTrue(TEXT("Rejected definition leaves no partial migration/reward"), SameRewardProfile(Save->GetProfileSnapshot(), Before));
	}
	auto Generic = Good;
	Generic.Resources = {{RewardCustom.ToContentId(), MAX_int64}, {RewardGold.ToContentId(), MAX_int64}};
	TestTrue(TEXT("Arbitrary resource entity accepted without hardcoded whitelist"), DYKMissionRewards::Settle(*Save, RewardResult(), Generic));
	TestEqual(TEXT("Generic resource safely reaches int64 maximum"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardCustom), MAX_int64);
	TestEqual(TEXT("Configured gold cap obeyed"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardGold), int64(MAX_uint32));
	TestEqual(TEXT("Gold overflow conserved without signed wrap"), RewardRemainder(*Save->GetProfileSnapshot(), Generic.RewardId, RewardGold), MAX_int64 - int64(MAX_uint32));
	auto Extra = Generic; Extra.RewardId = TEXT("QA-SECOND-SOURCE"); Extra.Resources = {{RewardCustom.ToContentId(), 1}};
	TestTrue(TEXT("New entitlement at representation limit still saved"), DYKMissionRewards::Settle(*Save, RewardResult(), Extra));
	TestEqual(TEXT("No overflow past int64 max"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardCustom), MAX_int64);
	TestEqual(TEXT("Unrepresentable one resource retained"), RewardRemainder(*Save->GetProfileSnapshot(), Extra.RewardId, RewardCustom), int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRewardWriteFailureTest, "DYK.M109.WriteFailureAtomicRetry", MissionRewardFlags)
bool FDYKRewardWriteFailureTest::RunTest(const FString& Parameters)
{
	FRewardStorage Storage; auto* Save = Storage.Open();
	const auto Def = DYKMissionRewards::PolygonDEV_PLACEHOLDER(); const auto Win = RewardResult();
	const auto Before = *Save->GetProfileSnapshot(); const auto Revision = Save->GetRevision();
	const FString Blocker = FPaths::Combine(Storage.Directory, TEXT("blocked-parent"));
	TestTrue(TEXT("Real filesystem obstruction created"), FFileHelper::SaveStringToFile(TEXT("blocked"), *Blocker));
	FDYKMissionRewardTestAccess::Prefix(*Save, FPaths::Combine(Blocker, TEXT("profile")));
	TestFalse(TEXT("Failed durable write rejects settlement"), DYKMissionRewards::Settle(*Save, Win, Def));
	TestTrue(TEXT("Failure preserves completion/receipt/balances and city migration"), SameRewardProfile(Save->GetProfileSnapshot(), Before));
	TestEqual(TEXT("Failed write does not advance revision"), Save->GetRevision(), Revision);
	FDYKMissionRewardTestAccess::Prefix(*Save, Storage.Prefix);
	TestTrue(TEXT("Same attempt retries after storage restored"), DYKMissionRewards::Settle(*Save, Win, Def));
	const auto Committed = *Save->GetProfileSnapshot();
	TestTrue(TEXT("Successful retry persisted whole transaction"), SameRewardProfile(Storage.Open()->GetProfileSnapshot(), Committed));
	TestTrue(TEXT("Prepare overflow and future collect room"), Save->ApplyProfileCommand(FGuid::NewGuid(), [](auto& P)
	{
		P.MissionRewards[0].Remaining.Add({RewardWood.ToContentId(), 3}); return true;
	}));
	const auto Pending = *Save->GetProfileSnapshot(); const FGuid Collect = FGuid::NewGuid();
	FDYKMissionRewardTestAccess::Prefix(*Save, FPaths::Combine(Blocker, TEXT("profile")));
	TestFalse(TEXT("Collection also requires durable write"), DYKMissionRewards::Collect(*Save, Collect, Def.Campaign));
	TestTrue(TEXT("Failed collection preserves entitlement and balance together"), SameRewardProfile(Save->GetProfileSnapshot(), Pending));
	FDYKMissionRewardTestAccess::Prefix(*Save, Storage.Prefix);
	TestTrue(TEXT("Identical collection command retries safely"), DYKMissionRewards::Collect(*Save, Collect, Def.Campaign));
	TestEqual(TEXT("Retried collection transfers exactly three"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardWood), int64(28));
	TestTrue(TEXT("Collected state survives reopen"), SameRewardProfile(Storage.Open()->GetProfileSnapshot(), *Save->GetProfileSnapshot()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRewardInvalidStateTest, "DYK.M109.InvalidPersistentStateRejectedAtomically", MissionRewardFlags)
bool FDYKRewardInvalidStateTest::RunTest(const FString& Parameters)
{
	const auto Def = DYKMissionRewards::PolygonDEV_PLACEHOLDER();
	for (int32 Case = 0; Case < 5; ++Case)
	{
		FRewardStorage Storage; auto* Save = Storage.Open();
		if (!TestTrue(TEXT("Seed real awarded profile"), DYKMissionRewards::Settle(*Save, RewardResult(), Def))) return false;
		TestTrue(TEXT("Seed deliberately invalid persistent domain state"), Save->ApplyProfileCommand(FGuid::NewGuid(), [Case](auto& P)
		{
			if (Case == 0) P.Resources[0].Amount = -1;
			if (Case == 1) { const auto Duplicate = P.Resources[0]; P.Resources.Add(Duplicate); }
			if (Case == 2) { const auto Duplicate = P.MissionRewards[0]; P.MissionRewards.Add(Duplicate); }
			if (Case == 3) P.MissionRewards[0].Remaining.Add({RewardWood.ToContentId(), -1});
			if (Case == 4) P.MissionRewards[0].CampaignId.Kind = EDYKContentKind::Mission;
			return true;
		}));
		const auto Before = *Save->GetProfileSnapshot(); const int64 Revision = Save->GetRevision();
		TestFalse(TEXT("Invalid state cannot settle a new reward transaction"), DYKMissionRewards::Settle(*Save, RewardResult(), Def));
		TestFalse(TEXT("Invalid state cannot enable collection"), DYKMissionRewards::HasCollectable(*Save->GetProfileSnapshot(), Def.Campaign));
		TestFalse(TEXT("Invalid state cannot collect"), DYKMissionRewards::Collect(*Save, FGuid::NewGuid(), Def.Campaign));
		TestTrue(TEXT("Invalid persisted state never partially repaired/credited by reward command"), SameRewardProfile(Save->GetProfileSnapshot(), Before));
		TestEqual(TEXT("Invalid reward command makes no durable write"), Save->GetRevision(), Revision);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRewardProcessTest, "DYK.M109.ProcessRestart", MissionRewardFlags)
bool FDYKRewardProcessTest::RunTest(const FString& Parameters)
{
	FString Phase, Prefix;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-M109RestartPhase="), Phase))
	{
		AddInfo(TEXT("Separate OS-process reward scenario requires -M109RestartPhase=write/read and -M109RestartPrefix; no process-restart assertion performed here.")); return true;
	}
	if (!TestTrue(TEXT("Explicit isolated prefix supplied"), FParse::Value(FCommandLine::Get(), TEXT("-M109RestartPrefix="), Prefix) && !Prefix.IsEmpty())) return false;
	if (!TestTrue(TEXT("Valid process phase"), Phase == TEXT("write") || Phase == TEXT("read"))) return false;
	TStrongObjectPtr<UGameInstance> Owner(NewObject<UGameInstance>());
	auto* Save = NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKMissionRewardTestAccess::Open(*Save, Prefix);
	const auto Def = DYKMissionRewards::PolygonDEV_PLACEHOLDER(); auto Win = RewardResult();
	Win.AttemptId = FGuid(0x4d313039, 0x61b7203a, 0x51af248d, 0x345912af);
	if (Phase == TEXT("write"))
	{
		TestTrue(TEXT("Dedicated process starts with fresh storage"), Save->GetLoadStatus() == EDYKProfileLoadStatus::Created);
		TestTrue(TEXT("Seed full warehouse"), Save->ApplyProfileCommand(FGuid::NewGuid(), [](auto& P)
		{
			if (!DYKCityEconomy::Initialize(P, 1794700000)) return false;
			RewardSetBalance(P, RewardWood, 100); return true;
		}));
		TestTrue(TEXT("Persist awarded entitlement in writer process"), DYKMissionRewards::Settle(*Save, Win, Def));
		return true;
	}
	TestTrue(TEXT("New OS process loads prior save"), Save->GetLoadStatus() == EDYKProfileLoadStatus::Loaded);
	TestEqual(TEXT("Completion survives OS restart"), Save->GetProfileSnapshot()->MissionCompletions.Num(), 1);
	TestEqual(TEXT("Pending wood survives OS restart"), RewardRemainder(*Save->GetProfileSnapshot(), Def.RewardId, RewardWood), int64(25));
	const auto Before = *Save->GetProfileSnapshot(); const auto Revision = Save->GetRevision();
	TestTrue(TEXT("Original callback id persisted across process restart"), DYKMissionRewards::Settle(*Save, Win, Def));
	TestEqual(TEXT("Replayed result does not commit again"), Save->GetRevision(), Revision);
	TestTrue(TEXT("New attempt replay receives no extra award"), DYKMissionRewards::Settle(*Save, RewardResult(), Def));
	TestTrue(TEXT("OS restart preserves full balance/receipt state without farming"), SameRewardProfile(Save->GetProfileSnapshot(), Before));
	TestTrue(TEXT("Can make room in new process"), DYKCityEconomy::Spend(*Save, FGuid::NewGuid(), RewardWood, 25));
	TestTrue(TEXT("Can claim persisted entitlement in new process"), DYKMissionRewards::Collect(*Save, FGuid::NewGuid(), Def.Campaign));
	TestEqual(TEXT("Exactly original entitlement restores full warehouse"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), RewardWood), int64(100));
	TestEqual(TEXT("Nothing lost or duplicated across process boundary"), RewardRemainder(*Save->GetProfileSnapshot(), Def.RewardId, RewardWood), int64(0));
	return true;
}

#endif
