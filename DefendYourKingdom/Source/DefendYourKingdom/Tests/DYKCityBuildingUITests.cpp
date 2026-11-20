#include "DYKCityHUDTestProfile.h"
#include "../Meta/DYKCityBuildingUI.h"
#include "../Meta/DYKCityWorkWidget.h"
#include "../Meta/DYKCityGameMode.h"
#include "../Meta/DYKCityBuildingActor.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../UI/DYKCityHUDArt.h"
#include "../UI/DYKCityPresentation.h"

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
#include "Components/CanvasPanel.h"
#include "Components/WidgetComponent.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Font.h"
#include "Misc/Paths.h"
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
	const auto Units=Items(TEXT("CITY-BARRACKS")); TestEqual(TEXT("Barracks has three pages"),Units.Num(),18);
	for(const auto& I:Units) if(!I.bFuture) TestEqual(TEXT("Barracks contains only its own research roots"),DYKCityResearch::Find(I.ResearchRoot)->Building,FName(TEXT("CITY-BARRACKS")));
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
	if(!TestNotNull(TEXT("Workshop primary popup"),UI->ResourceScreen.Get())) return false;
	auto* Visit=UI->ResourceScreen->FindAction(EDYKResourceAction::Visit);
	if(!TestNotNull(TEXT("Primary popup visit action"),Visit)) return false;
	Visit->OnClicked.Broadcast();
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
	UI->Dispatch(EDYKCityUIAction::Previous); UI->Dispatch(EDYKCityUIAction::Previous);
	UI->FindAction(EDYKCityUIAction::Item,0)->OnClicked.Broadcast();
	TestTrue(TEXT("Opening available card records view in actual service"),Save->GetProfileSnapshot()->CityEconomy.VisitedCards.Contains(TEXT("TRAP-CARD-0")));
	UI->CloseClicked();
	const auto Barracks=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-BARRACKS")); UI->OpenBuilding(Barracks,TEXT("CITY-BARRACKS")); UI->Dispatch(EDYKCityUIAction::Visit);
	for(int32 I=0;I<2;++I)
	{
		auto* NextButton=UI->FindAction(EDYKCityUIAction::Next); const auto Next=NextButton->TakeWidget();
		TestTrue(TEXT("Arrow uses artwork at authored aspect, not a text glyph"),NextButton->GetContent()==nullptr && NextButton->GetStyle().Normal.GetResourceObject()!=nullptr);
		Next->OnKeyDown(Geometry,Space); Next->OnKeyUp(Geometry,Space);
	}
	TestEqual(TEXT("Barracks third page through native inputs"),UI->GetWindowState().Page,2);
	UI->CloseClicked();
	const auto Saw=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-SAWMILL"));
	TestTrue(TEXT("Prepare completed output only"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Saw,Now](auto& P)
	{
		auto* S=P.CityEconomy.Instances.FindByPredicate([Saw](const auto& Row){return Row.InstanceId==Saw;}); S->Stored=30; S->CollectionStartedAtUnix=Now-3600; return true;
	}));
	UI->OpenBuilding(Saw,TEXT("CITY-SAWMILL"));
	auto* Collect=UI->ResourceScreen->FindAction(EDYKResourceAction::Collect);
	if(!TestNotNull(TEXT("Collection is in building card"),Collect)) return false;
	const auto CollectSlate=Collect->TakeWidget();
	CollectSlate->OnKeyDown(Geometry,Space); CollectSlate->OnKeyUp(Geometry,Space);
	TestEqual(TEXT("Real input collects only free room"),DYKCityEconomy::Balance(*Save->GetProfileSnapshot(),FDYKResourceId(TEXT("RES-WOOD"))),int64(100));
	TestFalse(TEXT("New card disables collection after emptying"),UI->ResourceScreen->FindAction(EDYKResourceAction::Collect)->bActionAvailable);
	CollectSlate->OnKeyDown(Geometry,Space); CollectSlate->OnKeyUp(Geometry,Space);
	TestEqual(TEXT("Repeated old Slate event cannot double collect"),DYKCityEconomy::Balance(*Save->GetProfileSnapshot(),FDYKResourceId(TEXT("RES-WOOD"))),int64(100));
	UI->ResourceScreen->FindAction(EDYKResourceAction::Visit)->OnClicked.Broadcast();
	UI->ResourceScreen->FindAction(EDYKResourceAction::Order,0)->OnClicked.Broadcast();
	const auto* Producer=Save->GetProfileSnapshot()->CityEconomy.Instances.FindByPredicate([Saw](const auto& Row){return Row.InstanceId==Saw;});
	TestEqual(TEXT("Visit order button starts actual persistent order"),Producer->Orders.Num(),1);
	const auto StaleOrder=UI->ResourceScreen->FindAction(EDYKResourceAction::Order,0)->TakeWidget();
	UI->CloseClicked(); StaleOrder->OnKeyDown(Geometry,Space); StaleOrder->OnKeyUp(Geometry,Space);
	TestEqual(TEXT("Input from a closed screen cannot submit an order"),Save->GetProfileSnapshot()->CityEconomy.Instances.FindByPredicate([Saw](const auto& Row){return Row.InstanceId==Saw;})->Orders.Num(),1);
	UI->ReleaseSlateResources(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityHoverTest,"DYK.CityHUD.NativeWidgets.BuildingDoesNotCreateHoverPopup",CityBuildingUITestFlags)
bool FDYKCityHoverTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine)); GI->Init();
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false); World->SetGameInstance(GI.Get());
	struct FCityHoverCleanup {UWorld* W; UGameInstance* G; ~FCityHoverCleanup(){W->DestroyWorld(false); G->Shutdown();}} Cleanup{World,GI.Get()};
	auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();
	TestTrue(TEXT("Seed"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P){return DYKCityHUDTest::Seed(P,FDateTime::UtcNow().ToUnixTimestamp());}));
	auto* Actor=World->SpawnActor<ADYKCityBuildingActor>(); Actor->InstanceId=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-SAWMILL")); Actor->BuildingId=FDYKBuildingId(TEXT("CITY-SAWMILL")).ToContentId();
	Actor->DispatchBeginPlay();
	Actor->Body->OnBeginCursorOver.Broadcast(Actor->Body);
	TestNull(TEXT("A city building has no screen-space hover widget"),Actor->FindComponentByClass<UWidgetComponent>());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityNewBadgeTest,"DYK.CityHUD.Cards.NewBadgeDurableAndLocked",CityBuildingUITestFlags)
bool FDYKCityNewBadgeTest::RunTest(const FString& Parameters)
{
	using namespace DYKCityBuildingUI;
	DYKCityHUDTest::Service S;
	const auto Traps=Items(TEXT("CITY-WORKSHOP"));
	TestTrue(TEXT("Legacy profile treats available unseen card as new"),ShowNew(S.State,Traps[0]));
	TestFalse(TEXT("Closed cell has no new badge"),ShowNew(S.State,Traps[6]));
	TestFalse(TEXT("Blocked unit has no new badge"),ShowNew(S.State,Items(TEXT("CITY-BARRACKS"))[2]));
	const auto Command=FGuid::NewGuid(); S.Fail=true;
	TestFalse(TEXT("Failed save does not dismiss new badge"),VisitCard(S,Command,TEXT("CITY-WORKSHOP"),0,0));
	TestTrue(TEXT("Unseen after failed command"),ShowNew(S.State,Traps[0]));
	S.Fail=false; const auto Wallet=S.State.Resources;
	TestTrue(TEXT("Opening available card"),VisitCard(S,Command,TEXT("CITY-WORKSHOP"),0,0));
	TestFalse(TEXT("Dismissed immediately after first successful open"),ShowNew(S.State,Traps[0]));
	TestTrue(TEXT("Duplicate command remains harmless"),VisitCard(S,Command,TEXT("CITY-WORKSHOP"),0,0));
	TestEqual(TEXT("Only one view marker"),S.State.CityEconomy.VisitedCards.Num(),1);
	for(int32 I=0;I<Wallet.Num();++I) TestEqual(TEXT("Viewing costs no resources"),Wallet[I].Amount,S.State.Resources[I].Amount);
	TestFalse(TEXT("Future card cannot acquire viewed state"),VisitCard(S,FGuid::NewGuid(),TEXT("CITY-WORKSHOP"),0,6));
	TestFalse(TEXT("Invalid index rejected"),VisitCard(S,FGuid::NewGuid(),TEXT("CITY-WORKSHOP"),0,-1));
	TStrongObjectPtr<UGameInstance> Owner(NewObject<UGameInstance>());
	const FString Prefix=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("CityHUD/seen-files"),FGuid::NewGuid().ToString(EGuidFormats::Digits),TEXT("profile")));
	auto* Save=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityHUDTestAccess::Open(*Save,Prefix);
	TestTrue(TEXT("Prepare durable profile"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P){return DYKCityHUDTest::Seed(P);}));
	TestTrue(TEXT("Persist card visit"),VisitCard(*Save,FGuid::NewGuid(),TEXT("CITY-WORKSHOP"),0,0));
	auto* Reload=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityHUDTestAccess::Open(*Reload,Prefix);
	if(!TestNotNull(TEXT("Actual file reload"),Reload->GetProfileSnapshot())) return false;
	TestFalse(TEXT("New badge never reappears after reload"),ShowNew(*Reload->GetProfileSnapshot(),Traps[0]));
	TestTrue(TEXT("Other item stays new"),ShowNew(*Reload->GetProfileSnapshot(),Traps[1]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityModularLayoutTest,"DYK.CityHUD.Cards.ModularAssetsAndBoundedLayout",CityBuildingUITestFlags)
bool FDYKCityModularLayoutTest::RunTest(const FString& Parameters)
{
	const auto* Skin=DYKCityHUDArt::Presentation(); if(!TestNotNull(TEXT("Editable presentation asset"),Skin)) return false;
	TestNotNull(TEXT("Standalone blank window"),Skin->Window.Get()); TestNotNull(TEXT("Standalone alpha card rim"),Skin->CardFrame.Get());
	TestNotNull(TEXT("Independent new overlay"),Skin->NewRibbon.Get()); TestNotNull(TEXT("Licensed serif font face"),Skin->Typeface.Get());
	TestTrue(TEXT("All defined traps and units have individually editable picture rows"),Skin->Pictures.Num()>=10);
	for(const auto& Row:Skin->Pictures)
	{
		TestNotNull(TEXT("Real configurable illustration"),Row.Illustration.LoadSynchronous());
		TestTrue(TEXT("Valid atlas region"),Row.UVMin.X>=0 && Row.UVMin.Y>=0 && Row.UVMax.X<=1 && Row.UVMax.Y<=1 && Row.UVMax.X>Row.UVMin.X && Row.UVMax.Y>Row.UVMin.Y);
	}
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine)); GI->Init();
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false); World->SetGameInstance(GI.Get());
	struct FCityModularCleanup {UWorld* W; UGameInstance* G; ~FCityModularCleanup(){W->DestroyWorld(false); G->Shutdown();}} Cleanup{World,GI.Get()};
	auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();
	TestTrue(TEXT("Seed test owners"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P){return DYKCityHUDTest::Seed(P,FDateTime::UtcNow().ToUnixTimestamp());}));
	FURL URL; URL.AddOption(TEXT("game=/Script/DefendYourKingdom.DYKCityGameMode")); if(!World->SetGameMode(URL)) return false;
	auto* UI=NewObject<UDYKCityWorkWidget>(World); UI->Initialize(); const auto Slate=UI->TakeWidget();
	const auto CheckLayout=[this,UI,Skin]()
	{
		for(const auto& Button:UI->Actions)
		{
			const auto* S=Cast<UCanvasPanelSlot>(Button->Slot); if(!S) continue;
			if(Button->Action!=EDYKCityUIAction::Close) TestTrue(TEXT("Actions stay above footer"),S->GetPosition().Y+S->GetSize().Y<=890);
			for(const auto& Other:UI->Actions)
			{
				if(Button==Other) continue;
				const auto* O=Cast<UCanvasPanelSlot>(Other->Slot); if(!O) continue;
				const FBox2D A(S->GetPosition()+FVector2D(1,1),S->GetPosition()+S->GetSize()-FVector2D(1,1));
				const FBox2D B(O->GetPosition(),O->GetPosition()+O->GetSize());
				TestFalse(TEXT("Clickable controls never overlap"),A.Intersect(B));
			}
		}
		UI->WidgetTree->ForEachWidget([this,Skin,UI](UWidget* Widget)
		{
			if(const auto* Label=Cast<UTextBlock>(Widget))
			{
				const UWidget* Parent=Label;
				while(Parent && Parent!=UI->DesignCanvas.Get()) Parent=Parent->GetParent();
				if(!Parent) return;
				const auto* Fit=Cast<UScaleBox>(Label->GetParent());
				TestNotNull(TEXT("Text has a fitting container"),Fit);
				if(Fit) TestTrue(TEXT("Text cannot spill beyond its assigned rectangle"),Fit->GetClipping()==EWidgetClipping::ClipToBounds);
				TestTrue(TEXT("Text uses configured serif face"),Label->GetFont().FontObject.Get()==Skin->Typeface.Get());
			}
		});
	};
	UI->OpenBuilding(DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-BARRACKS")),TEXT("CITY-BARRACKS")); UI->Dispatch(EDYKCityUIAction::Visit); CheckLayout();
	int32 FrameCount=0,NewCount=0;
	UI->WidgetTree->ForEachWidget([&](UWidget* W){if(const auto* I=Cast<UImage>(W)) {FrameCount+=I->GetBrush().GetResourceObject()==Skin->CardFrame; NewCount+=I->GetBrush().GetResourceObject()==Skin->NewRibbon;}});
	TestEqual(TEXT("Six separate card rims over content"),FrameCount,6);
	TestEqual(TEXT("Only four available unviewed cards get new overlays"),NewCount,4);
	UI->Dispatch(EDYKCityUIAction::Item,0); CheckLayout(); UI->CloseClicked();
	UI->OpenBuilding(DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-SAWMILL")),TEXT("CITY-SAWMILL")); UI->Dispatch(EDYKCityUIAction::Visit); CheckLayout();
	UI->ReleaseSlateResources(true); return true;
}
#endif
