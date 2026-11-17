#include "../Meta/DYKCityTerritory.h"
#include "../Meta/DYKCityTerritoryWidget.h"
#include "../Meta/DYKCityTerritoryActor.h"
#include "../Meta/DYKCityBuildingActor.h"
#include "../Meta/DYKCityDecorationActor.h"
#include "../Meta/DYKCityWidgets.h"
#include "../UI/DYKStartWidget.h"
#include "../Core/DYKGameInstance.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../Core/Interaction/DYKStrategyCameraPawn.h"

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
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/Button.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
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
				if (auto* Title=TerritoryRuntimeWidget<UDYKStartWidget>(W))
				{
					HoverHUD(W,Title->EnterButton); Stage=30; WaitHover(); return false;
				}
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
				CheckCamera(W);
				Test->TestTrue(TEXT("Seed dedicated test profile"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P)
					{ for (auto& R:P.Resources) if (R.ResourceId.Name==TEXT("RES-WOOD")) R.Amount=100; return true; }));
				UI->Select({0,1});
				HoverHUD(W,TerritoryRuntimeWidget<UDYKCityResourceWidget>(W)->MissionButton);
				Stage=20; WaitHover(); return false;
			}
			if (Stage==30)
			{
				Test->TestFalse(TEXT("Title HUD suppresses parcel popup"),UI->IsPopupVisible());
				auto* Title=TerritoryRuntimeWidget<UDYKStartWidget>(W);
				if (!Title || !TerritoryPress(Test,Title->EnterButton)) return true;
				Stage=0; WaitHover(); return false;
			}
			if (Stage==20)
			{
				Test->TestFalse(TEXT("Mission HUD hover clears existing parcel popup immediately"),UI->IsPopupVisible());
				auto* City=TerritoryRuntimeWidget<UDYKCityResourceWidget>(W);
				Test->TestTrue(TEXT("Mission button remains enabled"),City->MissionButton->GetIsEnabled());
				HoverHUD(W,City->ResourceText); Stage=21; WaitHover(); return false;
			}
			if (Stage==21)
			{
				Test->TestFalse(TEXT("Resource HUD does not hover through to land"),UI->IsPopupVisible());
				UDYKCityCollectWidget* Collect=nullptr;
				for (TActorIterator<ADYKCityBuildingActor> It(W); It; ++It)
				{
					auto* Candidate=Cast<UDYKCityCollectWidget>(It->CollectionWidget->GetUserWidgetObject());
					if (Candidate && Candidate->CollectButton && Candidate->CollectButton->GetVisibility()==ESlateVisibility::Visible)
					{ Collect=Candidate; break; }
				}
				if (!Test->TestNotNull(TEXT("Actual building collection HUD exists"),Collect)) return true;
				HoverHUD(W,Collect->CollectButton); Stage=22; WaitHover(); return false;
			}
			if (Stage==22)
			{
				Test->TestFalse(TEXT("Screen-space building collection HUD suppresses land popup even disabled"),UI->IsPopupVisible());
				Hover(W,{0,1}); Stage=23; WaitHover(); return false;
			}
			if (Stage==23)
			{
				Test->TestTrue(TEXT("Land hover resumes after leaving HUD"),UI->IsPopupVisible());
				auto* Mission=TerritoryRuntimeWidget<UDYKCityResourceWidget>(W)->MissionButton.Get();
				const auto MissionGeometry=Mission->GetCachedGeometry();
				const auto PopupGeometry=UI->Popup->GetCachedGeometry();
				const auto Offset=MissionGeometry.GetAbsolutePosition()+MissionGeometry.GetAbsoluteSize()*.5
					-PopupGeometry.GetAbsolutePosition()-PopupGeometry.GetAbsoluteSize()*.5;
				UI->Popup->SetRenderTranslation(Offset/PopupGeometry.GetAccumulatedLayoutTransform().GetScale());
				HoverHUD(W,Mission); Stage=25; WaitHover(); return false;
			}
			if (Stage==25)
			{
				Test->TestFalse(TEXT("Foreign HUD wins even where purchase popup overlaps its button"),UI->IsPopupVisible());
				UI->Popup->SetRenderTranslation(FVector2D::ZeroVector);
				Hover(W,{0,0}); Stage=10; WaitHover(); return false;
			}
			if (Stage==10)
			{
				Test->TestTrue(TEXT("Real world hover selects nonadjacent parcel"),UI->GetSelectedParcel()==FIntPoint(0,0));
				Test->TestTrue(TEXT("Nonadjacent popup visible"),UI->Popup->GetVisibility()==ESlateVisibility::Visible);
				Test->TestFalse(TEXT("Nonadjacent purchase disabled"),UI->UnlockButton->GetIsEnabled());
				Test->TestTrue(TEXT("Specific adjacency blocker displayed"),UI->Details->GetText().ToString().Contains(TEXT("нет смежного")));
				Hover(W,{1,1}); Stage=11; WaitHover(); return false;
			}
			if (Stage==11)
			{
				Test->TestTrue(TEXT("Open parcel has no purchase popup"),!UI->IsPopupVisible());
				Hover(W,{-1,1}); Stage=12; WaitHover(); return false;
			}
			if (Stage==12)
			{
				Test->TestTrue(TEXT("Decorative exterior has no purchase popup"),!UI->IsPopupVisible());
				Hover(W,{0,1}); Stage=13; WaitHover(); return false;
			}
			if (Stage==13)
			{
				Test->TestTrue(TEXT("Real cursor selects eligible parcel"),UI->GetSelectedParcel()==FIntPoint(0,1));
				Test->TestTrue(TEXT("Eligible popup visible"),UI->Popup->GetVisibility()==ESlateVisibility::Visible);
				Test->TestTrue(TEXT("Price and current balance displayed"),UI->Details->GetText().ToString().Contains(TEXT("100 / 7")));
				Stage=14; Capture(TEXT("01-city-grid.png")); return false;
			}
			if (Stage==14)
			{
				const auto Geometry=UI->UnlockButton->GetCachedGeometry();
				const auto Absolute=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5);
				FVector2D Pixel,Viewport; USlateBlueprintLibrary::AbsoluteToViewport(W,Absolute,Pixel,Viewport);
				float X=0,Y=0; W->GetFirstPlayerController()->GetMousePosition(X,Y);
				CursorStart={X,Y}; CursorEnd=Pixel; CursorStep=0; Stage=15; return false;
			}
			if (Stage==15)
			{
				Test->TestTrue(TEXT("Selected parcel stable along pointer path to popup"),UI->GetSelectedParcel()==FIntPoint(0,1));
				if (++CursorStep>6)
				{
					if (!bPanChecked)
					{
						CameraBeforePan=W->GetFirstPlayerController()->GetPawn()->GetActorLocation();
						W->GetFirstPlayerController()->GetPawn()->SetActorLocation(FVector(9000.,9000.,CameraBeforePan.Z));
						Stage=16; WaitHover(); return false;
					}
					Stage=1; WaitHover(); return false;
				}
				const auto Pixel=FMath::Lerp(CursorStart,CursorEnd,double(CursorStep)/6.);
				W->GetFirstPlayerController()->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));
				AfterTime=FPlatformTime::Seconds()+.1; AfterFrame=GFrameCounter+2; return false;
			}
			if (Stage==16)
			{
				Test->TestTrue(TEXT("Pan cannot retain stale popup for offscreen original parcel"),!UI->IsPopupVisible() || UI->GetSelectedParcel()!=FIntPoint(0,1));
				W->GetFirstPlayerController()->GetPawn()->SetActorLocation(CameraBeforePan);
				bPanChecked=true; Stage=17; WaitHover(); return false;
			}
			if (Stage==17) { Hover(W,{0,1}); Stage=18; WaitHover(); return false; }
			if (Stage==18) { Stage=14; return false; }
			if (Stage==1)
			{
				if (!Test->TestNotNull(TEXT("City plot UI"),UI)) return true;
				Test->TestTrue(TEXT("Popup stays visible while cursor moves onto purchase button"),UI->Popup->GetVisibility()==ESlateVisibility::Visible);
				ADYKCityDecorationActor* Decoration=nullptr;
				for (TActorIterator<ADYKCityDecorationActor> It(W);It;++It) { Decoration=*It; break; }
				if (!Test->TestNotNull(TEXT("Actual city decoration before purchase"),Decoration)) return true;
				const int32 RoadsBefore=Decoration->Roads->GetInstanceCount();
				Test->TestTrue(TEXT("Castle has its complete west perimeter before any land purchase"),Decoration->GetRoads().Cells.Contains({10,12}));
				FDYKCityRoadNetwork BeforeRoads;
				Test->TestTrue(TEXT("Complete road geometry derives before purchase"),BeforeRoads.Rebuild(*Save->GetProfileSnapshot(),DYKCityTerritory::DEV_PLACEHOLDER()));
				Test->TestTrue(TEXT("Actual roads already complete before purchase"),BeforeRoads.OrderedCells==Decoration->GetRoads().OrderedCells);
				if (!TerritoryPress(Test,UI->UnlockButton)) return true;
				Test->TestTrue(TEXT("Native plot button unlocks western neighbour"),DYKCityTerritory::IsUnlocked(*Save->GetProfileSnapshot(),{0,1}));
				FDYKCityRoadNetwork ExpectedRoads;
				Test->TestTrue(TEXT("Expected purchased-land network derives"),ExpectedRoads.Rebuild(*Save->GetProfileSnapshot(),DYKCityTerritory::DEV_PLACEHOLDER()));
				Test->TestTrue(TEXT("Full castle west perimeter remains visible after purchase"),Decoration->GetRoads().Cells.Contains({10,12}));
				Test->TestEqual(TEXT("Buying land cannot reveal a previously clipped required road"),Decoration->Roads->GetInstanceCount(),RoadsBefore);
				Test->TestTrue(TEXT("Actual network refreshed exactly after unlock"),Decoration->GetRoads().OrderedCells==ExpectedRoads.OrderedCells);
				Test->TestEqual(TEXT("Actual purchased-land mesh count matches network"),Decoration->Roads->GetInstanceCount(),ExpectedRoads.Cells.Num());
				Test->TestFalse(TEXT("Repeat UI unlock disabled"),UI->UnlockButton->GetIsEnabled());
				Test->TestTrue(TEXT("Exact wood debit persisted"),Save->GetProfileSnapshot()->Resources.ContainsByPredicate([](const auto& R) { return R.ResourceId.Name==TEXT("RES-WOOD") && R.Amount==93; }));
				const auto& P=*Save->GetProfileSnapshot();
				for (const auto& S:P.CityEconomy.Instances) if (P.Buildings[S.BuildingIndex].BuildingId.Name==TEXT("CITY-SAWMILL")) Instance=S.InstanceId;
				Test->TestTrue(TEXT("Saved placement rotated on newly opened land"),DYKCityTerritory::Move(*Save,FGuid::NewGuid(),Instance,{2,12},1,DYKCityTerritory::DEV_PLACEHOLDER()));
				HoverHUD(W,TerritoryRuntimeWidget<UDYKCityResourceWidget>(W)->MissionButton);
				Stage=24; WaitHover(); return false;
			}
			if (Stage==24)
			{
				Test->TestFalse(TEXT("Mission navigation hover suppresses land overlay before clicking"),UI->IsPopupVisible());
				if (!TerritoryPress(Test,TerritoryRuntimeWidget<UDYKCityResourceWidget>(W)->MissionButton)) return true;
				Stage=2; return false;
			}
			if (Stage==2)
			{
				if (UI) return false;
				if (auto* Movement=W->GetFirstPlayerController()->GetPawn()->FindComponentByClass<UDYKCameraMovement>())
					Test->TestFalse(TEXT("Battle camera does not inherit city bounds"),Movement->bBounded);
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
		void HoverHUD(UWorld* W,UWidget* Widget)
		{
			if (!Test->TestNotNull(TEXT("Requested HUD widget exists"),Widget)) return;
			const auto Geometry=Widget->GetCachedGeometry();
			const auto Absolute=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5);
			FVector2D Pixel,Viewport;
			USlateBlueprintLibrary::AbsoluteToViewport(W,Absolute,Pixel,Viewport);
			W->GetFirstPlayerController()->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));
		}
		void WaitHover() { AfterTime=FPlatformTime::Seconds()+.6; AfterFrame=GFrameCounter+5; }
		void Hover(UWorld* W,FIntPoint Parcel)
		{
			const FVector Center=DYKCityTerritory::CellCorner(Parcel*10+FIntPoint(5,5));
			FVector2D Screen; auto* PC=W->GetFirstPlayerController();
			Test->TestTrue(TEXT("Parcel center projects into viewport"),PC->ProjectWorldLocationToScreen(Center,Screen));
			PC->SetMouseLocation(FMath::RoundToInt(Screen.X),FMath::RoundToInt(Screen.Y));
		}
		void CheckCamera(UWorld* W)
		{
			auto* Pawn=W->GetFirstPlayerController()->GetPawn();
			auto* Movement=Pawn ? Pawn->FindComponentByClass<UDYKCameraMovement>() : nullptr;
			if (!Test->TestNotNull(TEXT("Real bounded camera movement exists"),Movement)) return;
			Test->TestTrue(TEXT("City camera bounds enabled"),Movement->bBounded);
			Test->TestTrue(TEXT("Seven by seven bounds minimum"),Movement->PanBounds.Min.Equals(FVector2D(-5000,-5000)));
			Test->TestTrue(TEXT("Seven by seven bounds maximum"),Movement->PanBounds.Max.Equals(FVector2D(9000,9000)));
			const auto Original=Pawn->GetActorLocation();
			for (int32 X : {-1,1}) for (int32 Y : {-1,1})
			{
				Pawn->SetActorLocation(FVector(X<0 ? -4999. : 8999.,Y<0 ? -4999. : 8999.,Original.Z));
				Movement->Velocity=FVector(X*2400.,Y*2400.,0.);
				Movement->TickComponent(.05f,LEVELTICK_All,&Movement->PrimaryComponentTick);
				const auto Location=Pawn->GetActorLocation();
				Test->TestTrue(TEXT("Inertial diagonal pan remains in 7x7"),Location.X>=-5000 && Location.X<=9000 && Location.Y>=-5000 && Location.Y<=9000);
				Test->TestTrue(TEXT("Outward velocity removed at each corner"),Movement->Velocity.IsNearlyZero());
				Movement->Velocity=FVector(-X*2400.,-Y*2400.,0.);
				Movement->TickComponent(.05f,LEVELTICK_All,&Movement->PrimaryComponentTick);
				const auto Inward=Pawn->GetActorLocation();
				Test->TestTrue(TEXT("Camera can move inward immediately after touching boundary"),Inward.X> -5000 && Inward.X<9000 && Inward.Y> -5000 && Inward.Y<9000);
			}
			for (int32 Axis=0; Axis<2; ++Axis) for (int32 Sign : {-1,1})
			{
				FVector Start(2000.,2000.,Original.Z),Velocity(1000.,1000.,0.);
				Start[Axis]=Sign<0 ? -4999. : 8999.; Velocity[Axis]=Sign*2400.;
				Pawn->SetActorLocation(Start); Movement->Velocity=Velocity;
				Movement->TickComponent(.05f,LEVELTICK_All,&Movement->PrimaryComponentTick);
				const auto Edge=Pawn->GetActorLocation();
				Test->TestTrue(TEXT("Each individual edge clamps outward movement"),FMath::IsNearlyEqual(Edge[Axis],Sign<0 ? -5000. : 9000.));
				Test->TestTrue(TEXT("Tangential camera movement continues along boundary"),Edge[1-Axis]>Start[1-Axis]);
			}
			Movement->StopMovementImmediately(); Pawn->SetActorLocation(Original);
		}
		void Capture(const TCHAR* File) { if (FParse::Param(FCommandLine::Get(),TEXT("DYKM201Capture"))) { PendingCapture=File; AfterTime=FPlatformTime::Seconds()+2.; } }
		FAutomationTestBase* Test;
		TWeakObjectPtr<UDYKGameInstance> GI;
		FString Mode,Evidence,PendingCapture;
		double Deadline,AfterTime=0;
		uint64 AfterFrame=0;
		int32 Stage=0;
		FGuid Instance;
		FVector2D CursorStart,CursorEnd;
		int32 CursorStep=0;
		FVector CameraBeforePan;
		bool bPanChecked=false;
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
