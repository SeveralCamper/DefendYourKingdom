#include "../Meta/DYKCityGameMode.h"
#include "../Meta/DYKCityEditorWidget.h"
#include "../Meta/DYKCityTerritoryWidget.h"
#include "../Meta/DYKCityBuildingActor.h"
#include "../Meta/DYKCityWidgets.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../Core/DYKGameInstance.h"
#include "../Core/Interaction/DYKSelectionPlayerController.h"
#include "../UI/DYKStartWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Input/Events.h"
#include "Widgets/SWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	template<class T> T* LayoutUI(UWorld* W)
	{
		for (TObjectIterator<T> It; It; ++It) if (It->GetWorld()==W && It->IsInViewport()) return *It;
		return nullptr;
	}
	bool LayoutPress(FAutomationTestBase* Test,UButton* B)
	{
		if (!Test->TestNotNull(TEXT("Native editor button exists"),B) || !Test->TestTrue(TEXT("Native editor button enabled"),B->GetIsEnabled())) return false;
		const auto Slate=B->TakeWidget();
		const FKeyEvent Key(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
		const auto Geometry=FGeometry::MakeRoot(FVector2D(300,40),FSlateLayoutTransform());
		return Test->TestTrue(TEXT("Slate down handled"),Slate->OnKeyDown(Geometry,Key).IsEventHandled())
			&& Test->TestTrue(TEXT("Slate up handled"),Slate->OnKeyUp(Geometry,Key).IsEventHandled());
	}
	bool SameLayoutProfile(const FDYKPlayerProfileSaveData& A,const FDYKPlayerProfileSaveData& B)
	{
		return FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&A,&B,0);
	}
	class FCityEditorRuntime : public IAutomationLatentCommand
	{
	public:
		FCityEditorRuntime(FAutomationTestBase* T,UDYKGameInstance* G,FString P,FString E)
			: Test(T),GI(G),Phase(MoveTemp(P)),Evidence(MoveTemp(E)),Deadline(FPlatformTime::Seconds()+150) {}
		bool Update() override
		{
			if (!GI.IsValid() || FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("M203 runtime timeout")); return true; }
			if (GFrameCounter<AfterFrame || FPlatformTime::Seconds()<AfterTime) return false;
			auto* W=GI->GetWorld(); if (!W || !W->HasBegunPlay() || GI->IsLevelTravelPending()) return false;
			auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>(); if (!Save || !Save->GetProfileSnapshot()) return false;
			if (!CaptureName.IsEmpty())
			{
#if WITH_EDITOR
				if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return false;
#endif
				FScreenshotRequest::RequestScreenshot(Evidence/CaptureName,true,false); CaptureName.Reset(); Wait(); return false;
			}
			auto* City=W->GetAuthGameMode<ADYKCityGameMode>();
			auto* UI=LayoutUI<UDYKCityEditorWidget>(W);
			if (Stage==0)
			{
				if (!City || !UI) return false;
				if (auto* Title=LayoutUI<UDYKStartWidget>(W)) { if (!LayoutPress(Test,Title->EnterButton)) return true; Wait(); return false; }
				if (Phase==TEXT("reload"))
				{
					TArray<uint8> Bytes;
					if (!Test->TestTrue(TEXT("Committed expected fixture exists"),FFileHelper::LoadFileToArray(Bytes,*(Evidence/TEXT("expected.sav"))))) return true;
					auto* Expected=Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
					if (!Test->TestNotNull(TEXT("Committed expected fixture loads"),Expected)) return true;
					Test->TestTrue(TEXT("OS restart discards unfinished draft and retains full committed profile"),SameLayoutProfile(*Save->GetProfileSnapshot(),Expected->Profile));
					Test->TestTrue(TEXT("OS restart retains command ledger"),Save->GetActiveProfile()->AppliedCommandIds==Expected->AppliedCommandIds);
					Test->TestFalse(TEXT("Editor does not resume incomplete buffer"),City->GetLayoutEditor().IsActive());
					CheckVisuals(W,*Save->GetProfileSnapshot());
					return true;
				}
				Before=*Save->GetProfileSnapshot(); Revision=Save->GetRevision();
				for (const auto& S:Before.CityEconomy.Instances) if (Before.Buildings[S.BuildingIndex].BuildingId.Name==TEXT("CITY-SAWMILL")) Saw=S.InstanceId;
				if (!Test->TestTrue(TEXT("Sawmill owned"),Saw.IsValid())) return true;
				// Focus the actual strategy camera, then select through its registered world input action.
				auto* Pawn=W->GetFirstPlayerController()->GetPawn();
				Pawn->SetActorLocation(FVector(2000,1000,Pawn->GetActorLocation().Z));
				Stage=1; Wait(); return false;
			}
			if (Stage==1)
			{
				auto* Actor=Building(W,Saw); if (!Test->TestNotNull(TEXT("Real city building actor exists"),Actor)) return true;
				HoverWorld(W,Actor->Body->Bounds.Origin); Stage=2; Wait(); return false;
			}
			if (Stage==2)
			{
				WorldAction(W);
				if (!Test->TestTrue(TEXT("World ray selects actual city building"),City->GetSelectedBuilding()==Saw)) return true;
				Test->TestTrue(TEXT("Move action offered after selection"),UI->MoveButton->IsVisible());
				LayoutPress(Test,UI->MoveButton);
				Test->TestTrue(TEXT("Move opens transaction without saving"),City->GetLayoutEditor().IsActive() && Save->GetRevision()==Revision);
				LayoutPress(Test,UI->StoreButton);
				Test->TestTrue(TEXT("Buffer hides actor and collision"),Building(W,Saw)->IsHidden() && !Building(W,Saw)->GetActorEnableCollision());
				Test->TestFalse(TEXT("Buffered actor collection UI hidden"),Building(W,Saw)->CollectionWidget->IsVisible());
				LayoutPress(Test,UI->ApplyButton);
				Test->TestTrue(TEXT("Incomplete Apply remains in editor"),City->GetLayoutEditor().IsActive());
				FString Error;
				Test->TestFalse(TEXT("Direct travel cannot bypass incomplete editor"),GI->RequestLevelTravel(EDYKLocalLevel::MissionPolygon,Error));
				LayoutPress(Test,LayoutUI<UDYKCityResourceWidget>(W)->MissionButton);
				Test->TestFalse(TEXT("Mission button cannot unload editor"),GI->IsLevelTravelPending());
				Test->TestTrue(TEXT("Failed Apply leaves saved profile byte-equivalent"),SameLayoutProfile(*Save->GetProfileSnapshot(),Before));
				LayoutUI<UDYKCityTerritoryWidget>(W)->Select({0,1});
				Test->TestFalse(TEXT("Land popup suppressed during editor"),LayoutUI<UDYKCityTerritoryWidget>(W)->IsPopupVisible());
				LayoutPress(Test,UI->RotateButton);
				Stage=10; Wait(); return false;
			}
			if (Stage==10)
			{
				// Text replacement changes the auto-sized panel; wait for Slate layout before reading geometry.
				HoverHUD(W,UI->Details); Stage=8; Wait(); return false;
			}
			if (Stage==8)
			{
				Test->TestTrue(TEXT("Panel text blocks world input"),UI->BlocksWorldInput());
				WorldAction(W);
				Test->TestTrue(TEXT("Click through editor text cannot place buffered building"),Building(W,Saw)->IsHidden());
				HoverHUD(W,LayoutUI<UDYKCityResourceWidget>(W)->ResourceText); Stage=9; Wait(); return false;
			}
			if (Stage==9)
			{
				Test->TestTrue(TEXT("Resource HUD blocks world input"),UI->BlocksWorldInput());
				WorldAction(W);
				Test->TestTrue(TEXT("Click through resource text cannot place buffered building"),Building(W,Saw)->IsHidden());
				HoverCell(W,{30,20}); Stage=3; Capture(TEXT("01-buffer.png")); Wait(); return false;
			}
			if (Stage==3)
			{
				WorldAction(W);
				Test->TestFalse(TEXT("Placed draft actor visible"),Building(W,Saw)->IsHidden());
				Test->TestTrue(TEXT("Draft world transform moved with rotated 2x3 footprint"),Building(W,Saw)->GetActorLocation().Equals(DYKCityTerritory::CellCorner({31,21})+FVector(0,100,0),1));
				LayoutPress(Test,UI->NextButton);
				Test->TestTrue(TEXT("Can select another owned building in editor"),City->GetSelectedBuilding()!=Saw);
				LayoutPress(Test,UI->StoreButton);
				LayoutPress(Test,UI->CancelButton);
				Test->TestFalse(TEXT("Cancel exits even with buffer"),City->GetLayoutEditor().IsActive());
				Test->TestEqual(TEXT("Cancel performs no save"),Save->GetRevision(),Revision);
				Test->TestTrue(TEXT("Cancel restores all saved fields"),SameLayoutProfile(*Save->GetProfileSnapshot(),Before));
				CheckVisuals(W,Before);
				// Second session commits a rotation and move through the same actual controls.
				City->SelectBuilding(Saw); UI->Refresh(); LayoutPress(Test,UI->MoveButton); LayoutPress(Test,UI->RotateButton);
				HoverCell(W,{30,20}); Stage=4; Wait(); return false;
			}
			if (Stage==4)
			{
				WorldAction(W); LayoutPress(Test,UI->ApplyButton);
				Test->TestFalse(TEXT("Complete Apply exits editor"),City->GetLayoutEditor().IsActive());
				Test->TestEqual(TEXT("Entire layout committed once"),Save->GetRevision(),Revision+1);
				auto Expected=Before;
				for (const auto& S:Expected.CityEconomy.Instances) if (S.InstanceId==Saw)
				{ Expected.Buildings[S.BuildingIndex].CellOrigin={30,20}; Expected.Buildings[S.BuildingIndex].RotationQuarterTurns=1; }
				Test->TestTrue(TEXT("Commit changes only requested placement"),SameLayoutProfile(*Save->GetProfileSnapshot(),Expected));
				CheckVisuals(W,Expected);
				TArray<uint8> Bytes;
				Test->TestTrue(TEXT("Serialize expected profile and ledger"),UGameplayStatics::SaveGameToMemory(const_cast<UDYKProfileSaveGame*>(Save->GetActiveProfile()),Bytes));
				Test->TestTrue(TEXT("Write OS restart fixture"),FFileHelper::SaveArrayToFile(Bytes,*(Evidence/TEXT("expected.sav"))));
				Stage=5; Capture(TEXT("02-committed.png")); Wait(); return false;
			}
			if (Stage==5)
			{
				OldWorld=W; OldUI=UI;
				FString Error; if (!Test->TestTrue(TEXT("Travel allowed after complete commit"),GI->RequestLevelTravel(EDYKLocalLevel::MissionPolygon,Error))) return true;
				Stage=6; return false;
			}
			if (Stage==6)
			{
				if (City) return false;
				CollectGarbage(RF_NoFlags);
				Test->TestFalse(TEXT("Editor UI releases unloaded city"),OldUI.IsValid());
				Test->TestFalse(TEXT("City world collected after exit"),OldWorld.IsValid());
				FString Error; if (!Test->TestTrue(TEXT("Return to city"),GI->RequestLevelTravel(EDYKLocalLevel::City,Error))) return true;
				Stage=7; return false;
			}
			if (Stage==7)
			{
				if (!City || !UI) return false;
				CheckVisuals(W,*Save->GetProfileSnapshot());
				Test->TestTrue(TEXT("Select building for interrupted edit"),City->SelectBuilding(Saw)); UI->Refresh(); LayoutPress(Test,UI->MoveButton);
				LayoutPress(Test,UI->StoreButton);
				Test->TestTrue(TEXT("Process ends with a transient buffered building"),City->GetLayoutEditor().IsActive() && Building(W,Saw)->IsHidden());
				Test->TestEqual(TEXT("No autosave for unfinished editor"),Save->GetRevision(),Revision+1);
				return true;
			}
			return false;
		}
	private:
		ADYKCityBuildingActor* Building(UWorld* W,FGuid Id)
		{ for (TActorIterator<ADYKCityBuildingActor> It(W); It; ++It) if (It->InstanceId==Id) return *It; return nullptr; }
		void CheckVisuals(UWorld* W,const FDYKPlayerProfileSaveData& P)
		{
			int32 Count=0; for (TActorIterator<ADYKCityBuildingActor> It(W); It; ++It) ++Count;
			Test->TestEqual(TEXT("Exactly one actor per owned building"),Count,P.Buildings.Num());
			for (const auto& S:P.CityEconomy.Instances)
			{
				auto* A=Building(W,S.InstanceId); if (!Test->TestNotNull(TEXT("Ownership actor retained"),A)) continue;
				const auto& B=P.Buildings[S.BuildingIndex];
				Test->TestFalse(TEXT("Owned actor placed visibly"),A->IsHidden());
				Test->TestTrue(TEXT("Owned actor collision restored"),A->GetActorEnableCollision());
				Test->TestTrue(TEXT("Actor position matches committed profile"),A->GetActorLocation().Equals(DYKCityTerritory::BuildingLocation(B,DYKCityTerritory::DEV_PLACEHOLDER()),1));
				Test->TestTrue(TEXT("Actor rotation matches committed profile"),A->GetActorRotation().Equals(FRotator(0,B.RotationQuarterTurns*90.,0),.1));
			}
		}
		void WorldAction(UWorld* W)
		{
			auto* PC=W->GetFirstPlayerController();
			for (int32 I=0;I<PC->InputComponent->GetNumActionBindings();++I)
			{
				const auto& Binding=PC->InputComponent->GetActionBinding(I);
				if (Binding.GetActionName()==TEXT("DYK_SelectWorld") && Binding.KeyEvent==IE_Pressed)
				{ Binding.ActionDelegate.Execute(EKeys::LeftMouseButton); return; }
			}
			Test->AddError(TEXT("World input action missing"));
		}
		void HoverWorld(UWorld* W,FVector Point)
		{
			FVector2D Screen; auto* PC=W->GetFirstPlayerController();
			Test->TestTrue(TEXT("World point projects through real camera"),PC->ProjectWorldLocationToScreen(Point,Screen));
			PC->SetMouseLocation(FMath::RoundToInt(Screen.X),FMath::RoundToInt(Screen.Y));
		}
		void HoverHUD(UWorld* W,UWidget* Widget)
		{
			FVector2D Pixel,Viewport;
			const auto Geometry=Widget->GetCachedGeometry();
			USlateBlueprintLibrary::LocalToViewport(W,Geometry,Geometry.GetLocalSize()*.5,Pixel,Viewport);
			W->GetFirstPlayerController()->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));
		}
		void HoverCell(UWorld* W,FIntPoint Cell) { HoverWorld(W,DYKCityTerritory::CellCorner(Cell)+FVector(100,100,0)); }
		void Wait() { AfterTime=FPlatformTime::Seconds()+.7; AfterFrame=GFrameCounter+5; }
		void Capture(const TCHAR* Name) { if (FParse::Param(FCommandLine::Get(),TEXT("DYKM203Capture"))) CaptureName=Name; }
		FAutomationTestBase* Test;
		TWeakObjectPtr<UDYKGameInstance> GI;
		FString Phase,Evidence,CaptureName;
		FDYKPlayerProfileSaveData Before;
		FGuid Saw;
		int64 Revision=0;
		int32 Stage=0;
		double Deadline,AfterTime=0;
		uint64 AfterFrame=0;
		TWeakObjectPtr<UWorld> OldWorld;
		TWeakObjectPtr<UDYKCityEditorWidget> OldUI;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityEditorRuntimeTest,"DYK.M203.RuntimeEditorAcrossProcesses",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDYKCityEditorRuntimeTest::RunTest(const FString& Parameters)
{
	FString Phase,Directory;
	if (!TestTrue(TEXT("Dedicated phase"),FParse::Value(FCommandLine::Get(),TEXT("DYKM203Phase="),Phase) && (Phase==TEXT("play") || Phase==TEXT("reload")))) return false;
	if (!TestTrue(TEXT("Dedicated evidence directory"),FParse::Value(FCommandLine::Get(),TEXT("DYKM203Evidence="),Directory) && !Directory.IsEmpty())) return false;
	IFileManager::Get().MakeDirectory(*Directory,true);
	for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::Game)
	{
		auto* GI=Context.World()->GetGameInstance<UDYKGameInstance>();
		if (!TestNotNull(TEXT("Actual game instance"),GI)) return false;
		AddCommand(new FCityEditorRuntime(this,GI,Phase,Directory)); return true;
	}
	AddError(TEXT("Requires -game process")); return false;
}
#endif
