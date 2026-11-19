#include "DYKCityHUDTestProfile.h"
#include "../UI/DYKResourceBuildingWidget.h"
#include "../Meta/DYKCityWorkWidget.h"
#include "../Meta/DYKCityGameMode.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Components/CanvasPanelSlot.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
constexpr auto BakeryTestFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBakeryBindingTest,"DYK.BakeryHUD.BindingAndCommands",BakeryTestFlags)
bool FDYKBakeryBindingTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine)); GI->Init();
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false); World->SetGameInstance(GI.Get());
	struct FCleanup {UWorld* W; UGameInstance* G; ~FCleanup(){W->DestroyWorld(false); G->Shutdown();}} Cleanup{World,GI.Get()};
	auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>(); const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
	TestTrue(TEXT("Prepare owned bakery using stable legacy GUID"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Now](auto& P){return DYKCityHUDTest::Seed(P,Now);}));
	FURL URL; URL.AddOption(TEXT("game=/Script/DefendYourKingdom.DYKCityGameMode")); if(!World->SetGameMode(URL)) return false;
	auto* UI=NewObject<UDYKCityWorkWidget>(World); UI->Initialize(); const auto Slate=UI->TakeWidget();
	const auto Instance=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-MILL")); UI->OpenBuilding(Instance,TEXT("CITY-MILL"));
	if(!TestNotNull(TEXT("Reusable resource widget, not BuildProduction layout"),UI->ResourceScreen.Get())) return false;
	TestTrue(TEXT("First card is nonmodal"),UI->ModalBlocker->GetVisibility()==ESlateVisibility::Collapsed);
	TestTrue(TEXT("Binds clicked instance"),UI->ResourceScreen->GetView().Instance==Instance);
	UI->ResourceScreen->FindAction(EDYKResourceAction::Visit)->OnClicked.Broadcast();
	TestTrue(TEXT("Visit opens interior"),UI->ResourceScreen->IsInterior());
	const auto* Slot=Cast<UCanvasPanelSlot>(UI->ModalRoot->Slot);
	TestTrue(TEXT("Old window reduced exactly 1.5 about screen center"),FMath::IsNearlyEqual(Slot->GetAnchors().Maximum.X,.81666667f));
	const auto Stale=UI->ResourceScreen->FindAction(EDYKResourceAction::Order,0)->TakeWidget();
	UI->ResourceScreen->FindAction(EDYKResourceAction::Back)->OnClicked.Broadcast();
	const auto Before=Save->GetRevision();
	const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(225,77),FSlateLayoutTransform());
	const FKeyEvent Space(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
	Stale->OnKeyDown(Geometry,Space); Stale->OnKeyUp(Geometry,Space);
	TestEqual(TEXT("Late real Slate input from previous screen cannot order"),Save->GetRevision(),Before);
	UI->ResourceScreen->FindAction(EDYKResourceAction::Visit)->OnClicked.Broadcast();
	for(int32 I=0;I<3;++I) UI->ResourceScreen->FindAction(EDYKResourceAction::Order,I)->OnClicked.Broadcast();
	TestEqual(TEXT("Orders affect only bakery instance"),UI->ResourceScreen->GetView().Orders,3);
	TestFalse(TEXT("Fourth order disabled"),UI->ResourceScreen->FindAction(EDYKResourceAction::Order,0)->bActionAvailable);
    const auto FullRevision=Save->GetRevision();
    UI->ResourceScreen->FindAction(EDYKResourceAction::Order,0)->OnClicked.Broadcast();
    TestEqual(TEXT("Full queue click gives feedback without extra order"),Save->GetRevision(),FullRevision);
	UI->ResourceScreen->FindAction(EDYKResourceAction::Back)->OnClicked.Broadcast();
	TestFalse(TEXT("Circle back returns to building card"),UI->ResourceScreen->IsInterior());
	UI->ResourceScreen->FindAction(EDYKResourceAction::CancelWork)->OnClicked.Broadcast();
    TestEqual(TEXT("Cancel active production starts next order"),UI->ResourceScreen->GetView().Orders,2);
    UI->ResourceScreen->FindAction(EDYKResourceAction::Upgrade)->OnClicked.Broadcast();
    TestTrue(TEXT("Upgrade button enqueues building work"),Save->GetProfileSnapshot()->CityEconomy.Work.ContainsByPredicate([Instance](const auto& W){return W.InstanceId==Instance;}));
    UI->ResourceScreen->FindAction(EDYKResourceAction::CancelWork)->OnClicked.Broadcast();
    TestFalse(TEXT("Cancel button cancels upgrade before production"),Save->GetProfileSnapshot()->CityEconomy.Work.ContainsByPredicate([Instance](const auto& W){return W.InstanceId==Instance;}));
    TestTrue(TEXT("Prepare ready bread with free warehouse space"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Instance,Now](auto& P){for(auto& R:P.Resources) R.Amount=0; for(auto& Row:P.CityEconomy.Instances) if(Row.InstanceId==Instance) {Row.Stored=15; Row.CollectionStartedAtUnix=Now-61;} return true;}));
    UI->CloseClicked(); UI->OpenBuilding(Instance,TEXT("CITY-MILL"));
    UI->ResourceScreen->FindAction(EDYKResourceAction::Collect)->OnClicked.Broadcast();
    TestEqual(TEXT("Collect button really transfers ready bread"),UI->ResourceScreen->GetView().Stored,int64(0));
    UI->CloseClicked(); TestFalse(TEXT("Closed"),UI->GetWindowState().IsModal());
	UI->ReleaseSlateResources(true); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBakeryGeometryTest,"DYK.BakeryHUD.ContentAndAnchoring",BakeryTestFlags)
bool FDYKBakeryGeometryTest::RunTest(const FString& Parameters)
{
	const auto* Def=UDYKResourceBuildingWidget::FindDefinition(TEXT("CITY-MILL")); if(!TestNotNull(TEXT("Bakery content asset"),Def)) return false;
	TestTrue(TEXT("Other resource content not silently converted"),UDYKResourceBuildingWidget::FindDefinition(TEXT("CITY-SAWMILL"))==nullptr);
	TSet<FName> Keys;
	for(const auto& Part:Def->Parts)
	{
		TestNotNull(TEXT("Every part is a real extracted texture"),Part.Texture.Get());
		TestFalse(TEXT("No duplicate layers or duplicate clickable buttons"),Keys.Contains(Part.Key)); Keys.Add(Part.Key);
	}
	const FVector2D Viewport(1920,1080),Size=UDYKResourceBuildingWidget::PopupSize(Viewport),Anchor(700,400);
	TestTrue(TEXT("Popup is 15 percent larger"),Size.Equals(FVector2D(506,632.5)));
	TestTrue(TEXT("Small viewport keeps popup inside"),UDYKResourceBuildingWidget::PopupSize({320,240}).Y<=224);
	TestTrue(TEXT("First window centered on building"),UDYKResourceBuildingWidget::PopupPosition(Anchor,Viewport,Size)+Size*.5==Anchor);
	TestTrue(TEXT("Screen edge clamped"),UDYKResourceBuildingWidget::PopupPosition({0,0},Viewport,Size)==FVector2D(8,8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBakeryCancelTest,"DYK.BakeryHUD.CancelProductionSafety",BakeryTestFlags)
bool FDYKBakeryCancelTest::RunTest(const FString& Parameters)
{
 DYKCityHUDTest::Service Service;
 const auto Instance=DYKCityHUDTest::Instance(Service.State,TEXT("CITY-MILL"));
 const int64 Now=DYKCityHUDTest::Time;
 const FGuid First=FGuid::NewGuid(),Next=FGuid::NewGuid(),Cancel=FGuid::NewGuid();
 TestTrue(TEXT("Enqueue first"),DYKCityOrders::Enqueue(Service,First,Instance,0,Now));
 TestTrue(TEXT("Enqueue successor"),DYKCityOrders::Enqueue(Service,Next,Instance,1,Now));
 auto Find=[&]() -> const FDYKCityProductionSaveData& {return *Service.State.CityEconomy.Instances.FindByPredicate([&](const auto& S){return S.InstanceId==Instance;});};
 const int64 Stored=Find().Stored;
 Service.Fail=true;
 TestFalse(TEXT("Failed save leaves active order intact"),DYKCityOrders::CancelCurrent(Service,Cancel,Instance,First,Now+1));
 TestEqual(TEXT("No lost order on save failure"),Find().Orders.Num(),2);
 Service.Fail=false;
 TestTrue(TEXT("Cancel running order"),DYKCityOrders::CancelCurrent(Service,Cancel,Instance,First,Now+1));
 TestTrue(TEXT("Successor starts, stored bread retained"),Find().Orders.Num()==1 && Find().Orders[0].OrderId==Next && Find().Orders[0].StartedAtUnix>0 && Find().Stored==Stored);
 TestTrue(TEXT("Repeated command is idempotent"),DYKCityOrders::CancelCurrent(Service,Cancel,Instance,First,Now+2));
 TestFalse(TEXT("Stale order id cannot cancel successor"),DYKCityOrders::CancelCurrent(Service,FGuid::NewGuid(),Instance,First,Now+2));
 return true;
}

#if WITH_EDITOR
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "RenderingThread.h"
#include "RHI.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBakeryRenderTest,"DYK.BakeryHUD.RenderReferenceWindows",BakeryTestFlags)
bool FDYKBakeryRenderTest::RunTest(const FString& Parameters)
{
	if(GUsingNullRHI) {AddInfo(TEXT("NOT_RENDERED: run this test with -d3d11 for the author-requested images.")); return true;}
	auto* Definition=UDYKResourceBuildingWidget::FindDefinition(TEXT("CITY-MILL")); if(!TestNotNull(TEXT("Content"),Definition)) return false;
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BakeryHUDReview")); IFileManager::Get().MakeDirectory(*Folder,true);
	FWidgetRenderer* Renderer=new FWidgetRenderer(false);
	TArray<FColor> NormalFirst,NormalInterior;
	for(int32 Pass=0;Pass<10;++Pass)
	{
		const bool Interior=(Pass%2)==1, Hover=Pass>=2;
		TStrongObjectPtr<UDYKResourceBuildingWidget> UI(NewObject<UDYKResourceBuildingWidget>()); UI->Initialize(); UI->Configure(Definition,Interior);
		const auto Slate=UI->TakeWidget(); FDYKResourceBuildingView View; View.Stored=2150; View.Capacity=2160; View.Orders=1; View.Seconds=3204; View.bWorking=true; View.bCanCollect=Pass!=4; View.bCanUpgrade=Pass!=6; View.bCanCancel=Pass!=8; if(Pass==3) View.Orders=3; UI->UpdateView(View);
		const FIntPoint Pixels=Interior ? FIntPoint(960,720) : FIntPoint(506,633);
		if(Hover)
		{
		 const EDYKResourceAction A=Pass==4 ? EDYKResourceAction::Collect : Pass==6 ? EDYKResourceAction::Upgrade : Pass==8 ? EDYKResourceAction::CancelWork : Pass==5 || Pass==7 || Pass==9 ? EDYKResourceAction::Back : Interior ? EDYKResourceAction::Order : EDYKResourceAction::Visit;
         const auto Button=UI->FindAction(A)->TakeWidget();
		 const FPointerEvent Pointer(0,FVector2D(10,10),FVector2D(10,10),TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());
		 Button->OnMouseEnter(FGeometry::MakeRoot(FVector2D(722,127),FSlateLayoutTransform()),Pointer);
		 TestTrue(TEXT("Real Slate mouse enter reaches button"),Button->IsHovered());
		}
		auto* Target=Renderer->DrawWidget(Slate,FVector2D(Pixels)); FlushRenderingCommands();
		TArray<FColor> Buffer; FReadSurfaceDataFlags ReadFlags; ReadFlags.SetLinearToGamma(true);
		if(!TestTrue(TEXT("Actual UMG render readback"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Buffer,ReadFlags))) {BeginCleanup(Renderer); return false;}
		// D3D11 8-bit readback returns this linear render target verbatim. PNG stores sRGB.
		// Encode once here; ReadPixels' conversion flag does not convert this surface format.
		for(auto& Pixel:Buffer) Pixel=FLinearColor(Pixel.R/255.f,Pixel.G/255.f,Pixel.B/255.f,Pixel.A/255.f).ToFColorSRGB();
		TestEqual(TEXT("Transparent exterior, no opaque matte"),Buffer[0].A,uint8(0));
		if(!Interior)
		{
			// Unmodified parchment in the author reference is sRGB (243,192,139).
			// Catch both missing and duplicated output gamma, with room for bilinear filtering.
			const auto Paper=Buffer[FMath::RoundToInt(471.*Pixels.Y/550)*Pixels.X+FMath::RoundToInt(373.*Pixels.X/440)];
			TestTrue(TEXT("Rendered source colors preserved"),FMath::Abs(int32(Paper.R)-243)<16 && FMath::Abs(int32(Paper.G)-192)<16 && FMath::Abs(int32(Paper.B)-139)<16);
		}
		auto& Normal=Interior ? NormalInterior : NormalFirst;
        if(!Hover) Normal=Buffer;
        else {int32 Changed=0; for(int32 I=0;I<Buffer.Num();++I) if(Buffer[I]!=Normal[I]) ++Changed; TestTrue(TEXT("Hover visibly changes rendered contour"),Changed>30);}
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Pixels.X,Pixels.Y,Buffer,PNG);
		TestTrue(TEXT("Saved actual UE widget PNG"),FFileHelper::SaveArrayToFile(PNG,*FPaths::Combine(Folder,Pass>=4 ? *FString::Printf(TEXT("hover-%d.png"),Pass) : Hover ? (Interior ? TEXT("interior-hover.png") : TEXT("first-hover.png")) : (Interior ? TEXT("interior.png") : TEXT("first.png")))));
		UI->ReleaseSlateResources(true);
	}
	BeginCleanup(Renderer); return true;
}
#endif
#endif
