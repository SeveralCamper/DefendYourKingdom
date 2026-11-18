#include "DYKCityHUDTestProfile.h"
#include "../Meta/DYKCityBuildingUI.h"
#include "../Meta/DYKCityWorkWidget.h"
#include "../Meta/DYKCityGameMode.h"
#include "../Meta/DYKCityBuildingActor.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../UI/DYKCityHUDArt.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/EngineBaseTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetComponent.h"
#include "Input/Events.h"
#include "Widgets/SWidget.h"
#include "UObject/StrongObjectPtr.h"

constexpr auto CityBuildingUITestFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityUIRoutesTest,"DYK.CityHUD.Routing.CategoriesPagesAndOwners",CityBuildingUITestFlags)
bool FDYKCityUIRoutesTest::RunTest(const FString& Parameters)
{
	using namespace DYKCityBuildingUI;
	TestTrue(TEXT("Resource tab includes sawmill"),ConstructionTypes(EDYKCityBuildTab::Resources).Contains(TEXT("CITY-SAWMILL")));
	TestTrue(TEXT("Special tab includes library"),ConstructionTypes(EDYKCityBuildTab::Special).Contains(TEXT("CITY-LIBRARY")));
	const auto City=ConstructionTypes(EDYKCityBuildTab::City);
	TestEqual(TEXT("City tab initially only houses"),City.Num(),1); TestEqual(TEXT("House category"),City[0],FName(TEXT("CITY-HOUSE")));
	TestFalse(TEXT("Library cannot leak into resource tab"),ConstructionTypes(EDYKCityBuildTab::Resources).Contains(TEXT("CITY-LIBRARY")));
	const auto Traps=Items(TEXT("CITY-WORKSHOP")); TestEqual(TEXT("Three complete pages"),Traps.Num(),18);
	for(int32 I=6;I<18;++I) TestTrue(TEXT("Future pages contain locked cells without fictitious research"),Traps[I].bFuture && Traps[I].ResearchRoot.IsNone());
	FDYKCityWindowState Window; Window.Select(FGuid::NewGuid(),TEXT("CITY-WORKSHOP")); Window.Visit();
	Window.TurnPage(1,18); TestEqual(TEXT("Second page"),Window.Page,1); Window.TurnPage(1,18); TestEqual(TEXT("Third page"),Window.Page,2);
	Window.TurnPage(1,18); TestEqual(TEXT("Cannot exceed third"),Window.Page,2); Window.TurnPage(-9,18); TestEqual(TEXT("Cannot go before first"),Window.Page,0);
	for(const auto& I:Items(TEXT("CITY-BARRACKS"))) TestEqual(TEXT("Barracks contains only its own research roots"),DYKCityResearch::Find(I.ResearchRoot)->Building,FName(TEXT("CITY-BARRACKS")));
	for(const auto& I:Items(TEXT("CITY-LIBRARY"),1)) TestEqual(TEXT("Library contains its common research"),DYKCityResearch::Find(I.ResearchRoot)->Building,FName(TEXT("CITY-LIBRARY")));
	TestTrue(TEXT("Separate hero route"),Interior(TEXT("CITY-TAVERN"))==EDYKCityInterior::Tavern);
	TestTrue(TEXT("Separate magic route"),Interior(TEXT("CITY-WIZARD-TOWER"))==EDYKCityInterior::Magic);
	for(const auto Asset:{TEXT("WorkshopHUD"),TEXT("AllTrapHUD"),TEXT("BarraksHUD"),TEXT("AllUnitHUD"),TEXT("LibraryHUD"),TEXT("SawmillHUD"),TEXT("MillHUD"),TEXT("QuarryHUD"),TEXT("ForgeHUD")})
		TestNotNull(TEXT("Real imported author texture exists"),DYKCityHUDArt::Brush(Asset).GetResourceObject());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityResearchOwnerTest,"DYK.CityHUD.Routing.ResearchRequiresCompletedBuilding",CityBuildingUITestFlags)
bool FDYKCityResearchOwnerTest::RunTest(const FString& Parameters)
{
	DYKCityHUDTest::Service S; DYKCityHUDTest::Seed(S.State,DYKCityHUDTest::Time,false);
	TestFalse(TEXT("Workshop required"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-TRAP-SPIKES-L1"),DYKCityHUDTest::Time));
	TestFalse(TEXT("Barracks required"),DYKCityResearch::CanEnqueue(S.State,TEXT("RESEARCH-INFANTRY-T2")));
	TestFalse(TEXT("Library required"),DYKCityResearch::CanEnqueue(S.State,TEXT("RESEARCH-INFANTRY-HP")));
	TestTrue(TEXT("Add completed workshop"),DYKCityHUDTest::AddOwned(S.State,TEXT("CITY-WORKSHOP"),DYKCityHUDTest::Time));
	TestTrue(TEXT("Correct owner unlocks its own command"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-TRAP-SPIKES-L1"),DYKCityHUDTest::Time));
	TestFalse(TEXT("Workshop never grants unit research access"),DYKCityResearch::CanEnqueue(S.State,TEXT("RESEARCH-INFANTRY-T2")));
	TestTrue(TEXT("Complete"),DYKCityWork::Update(S,DYKCityHUDTest::Time+20));
	TestTrue(TEXT("Trap type unlocked, not a fabricated inventory instance"),S.State.ResearchedContent.Contains(FDYKTrapId(TEXT("DYK-TRAP-SPIKES")).ToContentId()));
	TestTrue(TEXT("Next workshop level available"),DYKCityResearch::CanEnqueue(S.State,TEXT("RESEARCH-TRAP-SPIKES-L2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityNativeWindowsTest,"DYK.CityHUD.NativeWidgets.NavigationInputAndCollection",CityBuildingUITestFlags)
bool FDYKCityNativeWindowsTest::RunTest(const FString& Parameters)
{
	// Real native UMG/Slate in an isolated test world; no level, viewport, game loop or screenshots.
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine)); GI->Init();
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false); World->SetGameInstance(GI.Get());
	struct FCityUICleanup {UWorld* W; UGameInstance* G; ~FCityUICleanup(){W->DestroyWorld(false); G->Shutdown();}} Cleanup{World,GI.Get()};
	auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();
	if(!TestNotNull(TEXT("Actual profile service"),Save)) return false;
	const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
	TestTrue(TEXT("Seed completed owners and order migration"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Now](auto& P){return DYKCityHUDTest::Seed(P,Now);}));
	FURL URL; URL.AddOption(TEXT("game=/Script/DefendYourKingdom.DYKCityGameMode"));
	if(!TestTrue(TEXT("Native city mode in test world"),World->SetGameMode(URL))) return false;
	auto* UI=NewObject<UDYKCityWorkWidget>(World); UI->Initialize();
	const auto Slate=UI->TakeWidget();
	UI->ToggleClicked();
	TestTrue(TEXT("Central modal opened"),UI->GetWindowState().Window==EDYKCityWindow::Construction && UI->ModalRoot->IsVisible());
	const auto* ModalSlot=Cast<UCanvasPanelSlot>(UI->ModalRoot->Slot);
	if(!TestNotNull(TEXT("Anchored modal"),ModalSlot)) return false;
	TestTrue(TEXT("Large responsive bounds"),FMath::IsNearlyEqual(ModalSlot->GetAnchors().Maximum.X,.975,1.e-6));
	for(int32 I=0;I<4;++I)
	{
		auto* Tab=UI->FindAction(EDYKCityUIAction::BuildTab,I);
		if(!TestNotNull(TEXT("Four real tab buttons"),Tab)) return false;
		TestNotNull(TEXT("Tab content is an icon, not text"),Cast<UImage>(Tab->GetContent()));
		TestFalse(TEXT("Icon has an accessible tooltip"),Tab->GetToolTipText().IsEmpty());
	}
	TestTrue(TEXT("Modal blocks world input"),UI->BlocksWorldInput()); UI->CloseClicked();
	UI->OpenBuilding(FGuid::NewGuid(),TEXT("CITY-WORKSHOP")); TestFalse(TEXT("Unknown ownership cannot open a forged building"),UI->GetWindowState().IsModal());
	const auto Workshop=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-WORKSHOP"));
	UI->OpenBuilding(Workshop,TEXT("CITY-WORKSHOP"));
	UI->FindAction(EDYKCityUIAction::Visit)->OnClicked.Broadcast();
	TestTrue(TEXT("Visit opens owner's list"),UI->GetWindowState().Window==EDYKCityWindow::Interior);
	const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(310,225),FSlateLayoutTransform());
	const FKeyEvent Space(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
	for(int32 I=0;I<2;++I)
	{
		const auto Next=UI->FindAction(EDYKCityUIAction::Next)->TakeWidget();
		TestTrue(TEXT("Native Slate key down"),Next->OnKeyDown(Geometry,Space).IsEventHandled());
		TestTrue(TEXT("Native Slate key up changes page"),Next->OnKeyUp(Geometry,Space).IsEventHandled());
	}
	TestEqual(TEXT("Third page reachable through real buttons"),UI->GetWindowState().Page,2);
	TestFalse(TEXT("Next disabled at third page"),UI->FindAction(EDYKCityUIAction::Next)->GetIsEnabled());
	auto* Future=UI->FindAction(EDYKCityUIAction::Item,12);
	if(!TestNotNull(TEXT("Future cell is clickable"),Future)) return false;
	Future->OnClicked.Broadcast(); TestTrue(TEXT("Cell opens detail"),UI->GetWindowState().Window==EDYKCityWindow::Detail);
	TestFalse(TEXT("Closed item cannot start fictional research"),UI->FindAction(EDYKCityUIAction::Research)->GetIsEnabled());
	const auto Revision=Save->GetRevision(); UI->Dispatch(EDYKCityUIAction::Research,0,TEXT("RESEARCH-INFANTRY-HP"));
	TestEqual(TEXT("Cross-building payload cannot spend through wrong window"),Save->GetRevision(),Revision);
	UI->FindAction(EDYKCityUIAction::Back)->OnClicked.Broadcast(); TestEqual(TEXT("Back preserves list page"),UI->GetWindowState().Page,2);
	UI->CloseClicked();
	const auto Saw=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-SAWMILL"));
	TestTrue(TEXT("Prepare completed output only"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Saw,Now](auto& P)
	{
		auto* S=P.CityEconomy.Instances.FindByPredicate([Saw](const auto& Row){return Row.InstanceId==Saw;}); S->Stored=30; S->CollectionStartedAtUnix=Now-3600; return true;
	}));
	UI->OpenBuilding(Saw,TEXT("CITY-SAWMILL"));
	auto* Collect=UI->FindAction(EDYKCityUIAction::Collect);
	if(!TestNotNull(TEXT("Collection is in building card"),Collect)) return false;
	const auto CollectSlate=Collect->TakeWidget();
	CollectSlate->OnKeyDown(Geometry,Space); CollectSlate->OnKeyUp(Geometry,Space);
	TestEqual(TEXT("Real input collects only free room"),DYKCityEconomy::Balance(*Save->GetProfileSnapshot(),FDYKResourceId(TEXT("RES-WOOD"))),int64(100));
	TestFalse(TEXT("New card disables collection after emptying"),UI->FindAction(EDYKCityUIAction::Collect)->GetIsEnabled());
	CollectSlate->OnKeyDown(Geometry,Space); CollectSlate->OnKeyUp(Geometry,Space);
	TestEqual(TEXT("Repeated old Slate event cannot double collect"),DYKCityEconomy::Balance(*Save->GetProfileSnapshot(),FDYKResourceId(TEXT("RES-WOOD"))),int64(100));
	UI->FindAction(EDYKCityUIAction::Visit)->OnClicked.Broadcast();
	UI->FindAction(EDYKCityUIAction::Order,0)->OnClicked.Broadcast();
	const auto* Producer=Save->GetProfileSnapshot()->CityEconomy.Instances.FindByPredicate([Saw](const auto& Row){return Row.InstanceId==Saw;});
	TestEqual(TEXT("Visit order button starts actual persistent order"),Producer->Orders.Num(),1);
	const auto StaleOrder=UI->FindAction(EDYKCityUIAction::Order,0)->TakeWidget();
	UI->CloseClicked(); StaleOrder->OnKeyDown(Geometry,Space); StaleOrder->OnKeyUp(Geometry,Space);
	TestEqual(TEXT("Input from a closed screen cannot submit an order"),Save->GetProfileSnapshot()->CityEconomy.Instances.FindByPredicate([Saw](const auto& Row){return Row.InstanceId==Saw;})->Orders.Num(),1);
	UI->ReleaseSlateResources(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityHoverTest,"DYK.CityHUD.NativeWidgets.BuildingLabelsOnlyOnHover",CityBuildingUITestFlags)
bool FDYKCityHoverTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine)); GI->Init();
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false); World->SetGameInstance(GI.Get());
	struct FCityHoverCleanup {UWorld* W; UGameInstance* G; ~FCityHoverCleanup(){W->DestroyWorld(false); G->Shutdown();}} Cleanup{World,GI.Get()};
	auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();
	TestTrue(TEXT("Seed"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P){return DYKCityHUDTest::Seed(P,FDateTime::UtcNow().ToUnixTimestamp());}));
	auto* Actor=World->SpawnActor<ADYKCityBuildingActor>(); Actor->InstanceId=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-SAWMILL")); Actor->BuildingId=FDYKBuildingId(TEXT("CITY-SAWMILL")).ToContentId();
	TestFalse(TEXT("No persistent label even before BeginPlay"),Actor->CollectionWidget->IsVisible());
	Actor->DispatchBeginPlay();
	TestFalse(TEXT("Still hidden after initialization"),Actor->CollectionWidget->IsVisible());
	Actor->HoverBegin(Actor->Body); TestTrue(TEXT("Native hover event reveals text"),Actor->CollectionWidget->IsVisible());
	Actor->HoverEnd(Actor->Body); TestFalse(TEXT("Leaving building hides text"),Actor->CollectionWidget->IsVisible());
	return true;
}
#endif
