#include "../Meta/DYKCityTerritory.h"
#include "../Meta/DYKCityTerritoryWidget.h"
#include "../Meta/DYKCityTerritoryActor.h"
#include "../Meta/DYKCityBuildingActor.h"
#include "../UI/DYKStartWidget.h"
#include "../Core/DYKGameInstance.h"
#include "../Saving/DYKSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Input/Events.h"
#include "Widgets/SWidget.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	template<typename T> T* TerritoryRuntimeWidget(UWorld* W)
	{
		for (TObjectIterator<T> It; It; ++It) if (It->GetWorld()==W && It->IsInViewport()) return *It;
		return nullptr;
	}
	bool TerritoryPress(FAutomationTestBase* Test,UButton* Button)
	{
		if (!Test->TestNotNull(TEXT("Real native button exists"),Button) || !Test->TestTrue(TEXT("Button enabled"),Button->GetIsEnabled())) return false;
		const auto Slate=Button->TakeWidget();
		const auto Geometry=FGeometry::MakeRoot(FVector2D(200,50),FSlateLayoutTransform());
		const FKeyEvent Accept(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
		return Test->TestTrue(TEXT("Slate key down handled"),Slate->OnKeyDown(Geometry,Accept).IsEventHandled())
			&& Test->TestTrue(TEXT("Slate key up handled"),Slate->OnKeyUp(Geometry,Accept).IsEventHandled());
	}
	class FTerritoryRuntime : public IAutomationLatentCommand
	{
	public:
		FTerritoryRuntime(FAutomationTestBase* T,UDYKGameInstance* G,FString Phase,FString Directory)
			: Test(T),GI(G),Mode(MoveTemp(Phase)),Evidence(MoveTemp(Directory)),Deadline(FPlatformTime::Seconds()+150.) {}
		bool Update() override
		{
			if (!GI.IsValid() || FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("City territory runtime timeout")); return true; }
			if (GFrameCounter<AfterFrame || FPlatformTime::Seconds()<AfterTime) return false;
			auto* W=GI->GetWorld(); if (!W || !W->HasBegunPlay() || GI->IsLevelTravelPending()) return false;
			auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();
			if (!Save || !Save->GetProfileSnapshot()) return false;
			if (!PendingCapture.IsEmpty())
			{
#if WITH_EDITOR
				if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return false;
#endif
				FScreenshotRequest::RequestScreenshot(Evidence/PendingCapture,true,false); PendingCapture.Reset(); AfterFrame=GFrameCounter+10; return false;
			}
			auto* UI=TerritoryRuntimeWidget<UDYKCityTerritoryWidget>(W);
			if (Stage==0)
			{
				if (!UI) return false;
				if (auto* Title=TerritoryRuntimeWidget<UDYKStartWidget>(W)) if (!TerritoryPress(Test,Title->EnterButton)) return true;
				if (Mode==TEXT("reload"))
				{
					TArray<uint8> Bytes; Test->TestTrue(TEXT("Previous process fixture exists"),FFileHelper::LoadFileToArray(Bytes,*(Evidence/TEXT("expected.sav"))));
					auto* Expected=Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
					if (!Test->TestNotNull(TEXT("Expected profile loads"),Expected)) return true;
					Test->TestTrue(TEXT("OS restart retains full profile"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Save->GetProfileSnapshot(),&Expected->Profile,0));
					Test->TestTrue(TEXT("OS restart retains command ledger"),Save->GetActiveProfile()->AppliedCommandIds==Expected->AppliedCommandIds);
					return true;
				}
				Test->TestTrue(TEXT("Full geometry valid in live world"),DYKCityTerritory::Validate(*Save->GetProfileSnapshot(),DYKCityTerritory::DEV_PLACEHOLDER()));
				Test->TestEqual(TEXT("25 interactive plot buttons"),UI->ParcelButtons.Num(),25);
				Test->TestTrue(TEXT("Seed dedicated test profile"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P)
					{ for (auto& R:P.Resources) if (R.ResourceId.Name==TEXT("RES-WOOD")) R.Amount=100; return true; }));
				Stage=1; Capture(TEXT("01-city-grid.png")); return false;
			}
			if (Stage==1)
			{
				if (!Test->TestNotNull(TEXT("City plot UI"),UI)) return true;
				if (!TerritoryPress(Test,UI->ParcelButtons[5]) || !TerritoryPress(Test,UI->UnlockButton)) return true;
				Test->TestTrue(TEXT("Native plot button unlocks western neighbour"),DYKCityTerritory::IsUnlocked(*Save->GetProfileSnapshot(),{0,1}));
				Test->TestFalse(TEXT("Repeat UI unlock disabled"),UI->UnlockButton->GetIsEnabled());
				Test->TestTrue(TEXT("Exact wood debit persisted"),Save->GetProfileSnapshot()->Resources.ContainsByPredicate([](const auto& R) { return R.ResourceId.Name==TEXT("RES-WOOD") && R.Amount==93; }));
				const auto& P=*Save->GetProfileSnapshot();
				for (const auto& S:P.CityEconomy.Instances) if (P.Buildings[S.BuildingIndex].BuildingId.Name==TEXT("CITY-SAWMILL")) Instance=S.InstanceId;
				Test->TestTrue(TEXT("Saved placement rotated on newly opened land"),DYKCityTerritory::Move(*Save,FGuid::NewGuid(),Instance,{2,12},1,DYKCityTerritory::DEV_PLACEHOLDER()));
				Stage=2; FString Error;
				Test->TestTrue(TEXT("Travel away"),GI->RequestLevelTravel(EDYKLocalLevel::MissionPolygon,Error)); return false;
			}
			if (Stage==2)
			{
				if (UI) return false;
				FString Error; Test->TestTrue(TEXT("Travel back"),GI->RequestLevelTravel(EDYKLocalLevel::City,Error)); Stage=3; return false;
			}
			if (Stage==3)
			{
				if (!UI) return false;
				bool Found=false;
				for (TActorIterator<ADYKCityBuildingActor> It(W); It; ++It) if (It->InstanceId==Instance)
				{
					Found=true;
					const auto* S=Save->GetProfileSnapshot()->CityEconomy.Instances.FindByPredicate([this](const auto& Row) { return Row.InstanceId==Instance; });
					const auto& B=Save->GetProfileSnapshot()->Buildings[S->BuildingIndex];
					Test->TestTrue(TEXT("Respawned Actor centered on saved footprint"),It->GetActorLocation().Equals(DYKCityTerritory::BuildingLocation(B,DYKCityTerritory::DEV_PLACEHOLDER())));
					Test->TestEqual(TEXT("Respawned rotation"),It->GetActorRotation().Yaw,90.);
				}
				Test->TestTrue(TEXT("Same instance restored"),Found);
				TArray<uint8> Bytes;
				Test->TestTrue(TEXT("Serialize live confirmed fixture"),UGameplayStatics::SaveGameToMemory(const_cast<UDYKProfileSaveGame*>(Save->GetActiveProfile()),Bytes));
				Test->TestTrue(TEXT("Write restart fixture"),FFileHelper::SaveArrayToFile(Bytes,*(Evidence/TEXT("expected.sav"))));
				Stage=4; Capture(TEXT("02-returned-grid.png")); return false;
			}
			return true;
		}
	private:
		void Capture(const TCHAR* File) { if (FParse::Param(FCommandLine::Get(),TEXT("DYKM201Capture"))) { PendingCapture=File; AfterTime=FPlatformTime::Seconds()+2.; } }
		FAutomationTestBase* Test;
		TWeakObjectPtr<UDYKGameInstance> GI;
		FString Mode,Evidence,PendingCapture;
		double Deadline,AfterTime=0;
		uint64 AfterFrame=0;
		int32 Stage=0;
		FGuid Instance;
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityTerritoryRuntimeTest,"DYK.M201.RuntimeTerritoryAcrossProcesses",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDYKCityTerritoryRuntimeTest::RunTest(const FString& Parameters)
{
	FString Phase,Directory;
	if (!TestTrue(TEXT("Dedicated runtime phase"),FParse::Value(FCommandLine::Get(),TEXT("DYKM201Phase="),Phase) && (Phase==TEXT("play") || Phase==TEXT("reload")))) return false;
	if (!TestTrue(TEXT("Dedicated runtime directory"),FParse::Value(FCommandLine::Get(),TEXT("DYKM201Evidence="),Directory) && !Directory.IsEmpty())) return false;
	IFileManager::Get().MakeDirectory(*Directory,true);
	for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::Game)
	{
		auto* GI=Context.World()->GetGameInstance<UDYKGameInstance>();
		if (!TestNotNull(TEXT("Actual game instance"),GI)) return false;
		AddCommand(new FTerritoryRuntime(this,GI,Phase,Directory)); return true;
	}
	AddError(TEXT("Requires -game process")); return false;
}
#endif
