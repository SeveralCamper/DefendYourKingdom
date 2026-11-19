#include "DYKCityHUDTestProfile.h"
#include "../UI/DYKResourceBuildingWidget.h"
#include "../UI/DYKHUDButtonSlate.h"
#include "../Meta/DYKCityWorkWidget.h"
#include "../Meta/DYKCityGameMode.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
constexpr auto ProductionFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProductionMigrationTest,"DYK.ProductionHUD.IronMigrationAndOrders",ProductionFlags)
bool FDYKProductionMigrationTest::RunTest(const FString& Parameters)
{
 DYKCityHUDTest::Service Service;
 const FDYKResourceId Iron(TEXT("RES-IRON"));
 Service.State.Resources.RemoveAll([Iron](const auto& R){return R.ResourceId==Iron.ToContentId();});
 auto Expected=Service.State;
 Expected.Resources.Add({Iron.ToContentId(),0});
 TestTrue(TEXT("Existing city gains only an empty iron balance"),DYKCityEconomy::Initialize(Service.State,DYKCityHUDTest::Time));
 TestTrue(TEXT("Buildings, orders, other balances and progress preserved"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Expected,&Service.State,0));
 TestTrue(TEXT("Migration can be repeated"),DYKCityEconomy::Initialize(Service.State,DYKCityHUDTest::Time+60));
 TestTrue(TEXT("No duplicate or changed balance"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Expected,&Service.State,0));
 TestFalse(TEXT("Forge is not silently granted to existing profiles"),DYKCityHUDTest::Instance(Service.State,TEXT("CITY-FORGE")).IsValid());
 TestTrue(TEXT("Own forge"),DYKCityHUDTest::AddOwned(Service.State,TEXT("CITY-FORGE"),DYKCityHUDTest::Time));
 TestTrue(TEXT("Forge uses normal order migration"),DYKCityOrders::Migrate(Service.State,DYKCityHUDTest::Time));
 const auto Instance=DYKCityHUDTest::Instance(Service.State,TEXT("CITY-FORGE"));
 const auto Other=DYKCityHUDTest::Instance(Service.State,TEXT("CITY-QUARRY"));
 const auto OtherBefore=DYKCityEconomy::View(Service.State,Other,DYKCityHUDTest::Time).Stored;
 TestTrue(TEXT("Queue iron order"),DYKCityOrders::Enqueue(Service,FGuid::NewGuid(),Instance,0,DYKCityHUDTest::Time));
 TestTrue(TEXT("Complete iron order"),DYKCityOrders::Advance(Service.State,DYKCityHUDTest::Time+601));
 const auto Ready=DYKCityEconomy::View(Service.State,Instance,DYKCityHUDTest::Time+601);
 TestEqual(TEXT("Iron order produces recipe quantity"),Ready.Stored,int64(15));
 Service.Fail=true;
 TestFalse(TEXT("Failed persistence cannot lose iron"),DYKCityEconomy::Collect(Service,FGuid::NewGuid(),Instance,DYKCityHUDTest::Time+601));
 TestEqual(TEXT("Stored iron retained after failed save"),DYKCityEconomy::View(Service.State,Instance,DYKCityHUDTest::Time+601).Stored,int64(15));
 Service.Fail=false;
 const auto Command=FGuid::NewGuid();
 TestTrue(TEXT("Collect iron"),DYKCityEconomy::Collect(Service,Command,Instance,DYKCityHUDTest::Time+601));
 TestTrue(TEXT("Retry collect is idempotent"),DYKCityEconomy::Collect(Service,Command,Instance,DYKCityHUDTest::Time+601));
 TestEqual(TEXT("Iron credited once"),DYKCityEconomy::Balance(Service.State,Iron),int64(15));
 TestEqual(TEXT("Other producer unaffected"),DYKCityEconomy::View(Service.State,Other,DYKCityHUDTest::Time).Stored,OtherBefore);
 TestTrue(TEXT("Final city validates"),DYKCityEconomy::Validate(Service.State));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProductionSharedTest,"DYK.ProductionHUD.AllProducersShareLayoutAndCommands",ProductionFlags)
bool FDYKProductionSharedTest::RunTest(const FString& Parameters)
{
 TestEqual(TEXT("Subminute remaining rounds up"),UDYKResourceBuildingWidget::FormatWorkTime(59),FString(TEXT("0 ч 01 мин")));
 TestEqual(TEXT("Hour rollover"),UDYKResourceBuildingWidget::FormatWorkTime(3599),FString(TEXT("1 ч 00 мин")));
 TestEqual(TEXT("Negative remaining bounded"),UDYKResourceBuildingWidget::FormatWorkTime(-1),FString(TEXT("0 ч 00 мин")));
 TestFalse(TEXT("Huge timer never wraps negative"),UDYKResourceBuildingWidget::FormatWorkTime(MAX_int64).StartsWith(TEXT("-")));
 TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine));GI->Init();
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);World->SetGameInstance(GI.Get());
 struct FCleanup{UWorld* W;UGameInstance* G;~FCleanup(){W->DestroyWorld(false);G->Shutdown();}}Cleanup{World,GI.Get()};
 auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
 TestTrue(TEXT("Seed all producers"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Now](auto& P){return DYKCityHUDTest::Seed(P,Now) && DYKCityHUDTest::AddOwned(P,TEXT("CITY-FORGE"),Now) && DYKCityOrders::Migrate(P,Now);}));
 FURL URL;URL.AddOption(TEXT("game=/Script/DefendYourKingdom.DYKCityGameMode"));if(!World->SetGameMode(URL))return false;
 TStrongObjectPtr<UDYKCityWorkWidget> UI(NewObject<UDYKCityWorkWidget>(World));UI->Initialize();const auto Root=UI->TakeWidget();
 const auto* Shared=UDYKResourceBuildingWidget::FindDefinition(TEXT("CITY-MILL"));
 if(!TestNotNull(TEXT("Single production definition"),Shared))return false;
 TestTrue(TEXT("Production template flag"),Shared->bProductionTemplate);
 int32 Count=0;
 auto Press=[&](UDYKResourceHUDButton* Button)
 {
  if(!TestNotNull(TEXT("Action exists"),Button))return;
  const auto Native=Button->TakeWidget();const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(300,90),FSlateLayoutTransform());
  const FKeyEvent Space(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
  Native->OnKeyDown(Geometry,Space);Native->OnKeyUp(Geometry,Space);
 };
 for(const auto& Def:DYKCityCatalog::Buildings())
 {
  if(Def.Category!=EDYKCityBuildingCategory::Production)continue;
  ++Count;
  TestTrue(TEXT("Every producer loads the identical asset object"),Shared==UDYKResourceBuildingWidget::FindDefinition(Def.Id.GetName()));
  const auto Instance=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),*Def.Id.GetName().ToString());
  UI->CloseClicked();UI->OpenBuilding(Instance,Def.Id.GetName());
  if(!TestNotNull(TEXT("Shared widget"),UI->ResourceScreen.Get()))continue;
  const auto& View=UI->ResourceScreen->GetView();
  const auto* Picture=Cast<UImage>(UI->ResourceScreen->WidgetTree->FindWidget(TEXT("PrimaryIllustration")));
  const auto& Illustration=Shared->PrimaryIllustrations.FindRef(Def.Id.GetName());
  if(TestNotNull(TEXT("Producer has a configured illustration"),Illustration.Get()) &&
     TestNotNull(TEXT("Primary window shows its illustration"),Picture))
  {
   TestTrue(TEXT("Primary window uses the selected producer's artwork"),Picture->GetBrush().GetResourceObject()==Illustration.Get());
  }
  TestEqual(TEXT("Title is building data"),View.BuildingName.ToString(),FString(Def.DisplayName));
  TestEqual(TEXT("Product is building data"),View.ResourceName.ToString(),FString(Def.ProductName));
  const auto* Cancel=UI->ResourceScreen->FindAction(EDYKResourceAction::CancelWork);
  TestFalse(TEXT("Empty cancel does not accept input"),Cancel->GetIsEnabled());
  TestNotNull(TEXT("Empty cancel has authored gray artwork"),Cancel->GetStyle().Disabled.GetResourceObject());
  TestTrue(TEXT("Visit remains actionable"),UI->ResourceScreen->FindAction(EDYKResourceAction::Visit)->GetIsEnabled());
  Press(UI->ResourceScreen->FindAction(EDYKResourceAction::Visit));
  TestTrue(TEXT("Visit opens shared interior"),UI->ResourceScreen && UI->ResourceScreen->IsInterior());
  TestNull(TEXT("Interior has no primary illustration"),UI->ResourceScreen->WidgetTree->FindWidget(TEXT("PrimaryIllustration")));
  TestEqual(TEXT("Live product label"),UI->ResourceScreen->FindField(EDYKProductionField::ProductName)->GetText().ToString(),FString(Def.ProductName));
  for(int32 I=0;I<3;++I)
  {
   const auto* Repeat=UI->ResourceScreen->FindAction(EDYKResourceAction::Order,0);
   TestTrue(TEXT("The same recipe can be queued again while there is room"),Repeat->GetIsEnabled());
   Press(UI->ResourceScreen->FindAction(EDYKResourceAction::Order,0));
  }
  for(int32 I=0;I<4;++I)
  {
   const auto* Order=UI->ResourceScreen->FindAction(EDYKResourceAction::Order,I);
   TestFalse(TEXT("Full queue disables every recipe"),Order->GetIsEnabled());
   TestNotNull(TEXT("Full queue selects the authored gray order brush"),Order->GetStyle().Disabled.GetResourceObject());
  }
  const auto Revision=Save->GetRevision();
  Press(UI->ResourceScreen->FindAction(EDYKResourceAction::Order,3));
  TestEqual(TEXT("Full queue click cannot mutate profile"),Save->GetRevision(),Revision);
  TestEqual(TEXT("Exactly three orders on clicked instance"),UI->ResourceScreen->GetView().Orders,3);
  Press(UI->ResourceScreen->FindAction(EDYKResourceAction::Back));
  Press(UI->ResourceScreen->FindAction(EDYKResourceAction::CancelWork));
  TestEqual(TEXT("Cancel works for every producer"),UI->ResourceScreen->GetView().Orders,2);
  Press(UI->ResourceScreen->FindAction(EDYKResourceAction::Visit));
  TestTrue(TEXT("Freeing an order slot restores interactivity"),UI->ResourceScreen->FindAction(EDYKResourceAction::Order,3)->GetIsEnabled());
 }
 TestEqual(TEXT("Four production types including forge"),Count,4);
 UI->ReleaseSlateResources(true);return true;
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProductionRenderTest,"DYK.ProductionHUD.RenderSharedWindowsAndHover",ProductionFlags)
bool FDYKProductionRenderTest::RunTest(const FString& Parameters)
{
 if(GUsingNullRHI){AddInfo(TEXT("NOT_RENDERED: requires the separate D3D11 run."));return true;}
 auto* Definition=UDYKResourceBuildingWidget::FindDefinition(TEXT("CITY-MILL"));
 if(!TestNotNull(TEXT("Shared production asset"),Definition))return false;
 FAssetCompilingManager::Get().FinishAllCompilation();
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("ProductionTemplate/review");IFileManager::Get().MakeDirectory(*Folder,true);
 FWidgetRenderer* Renderer=new FWidgetRenderer(false);
 auto Draw=[&](TSharedRef<SWidget> Slate,FIntPoint Pixels,const FString& Name)
 {
  UTextureRenderTarget2D* Target=nullptr;
  for(int32 Frame=0;Frame<3;++Frame){Target=Renderer->DrawWidget(Slate,FVector2D(Pixels));FlushRenderingCommands();}
  TArray<FColor> Buffer;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
  if(!TestTrue(TEXT("Real UMG render readback"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Buffer,Flags)))return Buffer;
  for(auto& C:Buffer)C=FLinearColor(C.R/255.f,C.G/255.f,C.B/255.f,C.A/255.f).ToFColorSRGB();
  TestEqual(TEXT("Transparent exterior"),Buffer[0].A,uint8(0));
  if(!Name.IsEmpty())
  {
   TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Pixels.X,Pixels.Y,Buffer,PNG);
   TestTrue(TEXT("Save actual rendered widget"),FFileHelper::SaveArrayToFile(PNG,*(Folder/(Name+TEXT(".png")))));
  }
  return Buffer;
 };
 for(const auto& Def:DYKCityCatalog::Buildings())
 {
  if(Def.Category!=EDYKCityBuildingCategory::Production)continue;
  for(bool Interior:{false,true})
  {
   TStrongObjectPtr<UDYKResourceBuildingWidget> UI(NewObject<UDYKResourceBuildingWidget>());UI->Initialize();UI->Configure(Definition,Interior);
   const auto Slate=UI->TakeWidget();FDYKResourceBuildingView View;
   View.Illustration=Definition->PrimaryIllustrations.FindRef(Def.Id.GetName()).Get();
   View.BuildingName=FText::FromString(Def.DisplayName);View.ResourceName=FText::FromString(Def.ProductName);
   View.Description=FText::FromString(TEXT("Производство: ")+View.ResourceName.ToString()+TEXT(".\nЗаказы выполняются по очереди."));
   for(const auto& Recipe:DYKCityOrders::DEV_PLACEHOLDER())View.Recipes.Add({Recipe.Amount,Recipe.Seconds});
   View.Stored=15;View.Orders=1;View.Seconds=3204;View.bWorking=true;
   View.bCanCollect=View.bCanCancel=View.bCanUpgrade=true;
   UI->UpdateView(View);
   const FIntPoint Pixels=Interior?FIntPoint(960,720):FIntPoint(506,633);
   const FString Prefix=Def.Id.GetName().ToString()+(Interior?TEXT("-interior"):TEXT("-first"));
   const auto Normal=Draw(Slate,Pixels,Prefix);
   if(Interior)
   {
    View.Orders=View.QueueCapacity;
    UI->UpdateView(View);
    const auto FullQueue=Draw(Slate,Pixels,Prefix+TEXT("-full-queue"));
    TestTrue(TEXT("Full queue changes all four order buttons"),FullQueue!=Normal);
    for(int32 Index=0;Index<4;++Index)
     TestFalse(TEXT("Full queue disables every order in the rendered window"),UI->FindAction(EDYKResourceAction::Order,Index)->GetIsEnabled());
    View.Orders=1;
    UI->UpdateView(View);
    TestTrue(TEXT("Free space restores the original order visuals"),Normal==Draw(Slate,Pixels,FString()));
   }
   if(Interior || Def.Id.GetName()==TEXT("CITY-MILL"))
   {
    int32 Pass=0;
    for(const auto& Part:Definition->Parts)
    {
     const auto Action=Part.Action;
     if(Part.bInterior!=Interior || Action==EDYKResourceAction::None)continue;
     auto* Button=UI->FindAction(Action,Part.Index);if(!TestNotNull(TEXT("Hover action"),Button))continue;
     const auto Native=Button->TakeWidget();
     const auto Highlight=StaticCastSharedRef<SDYKHUDButton>(Native)->HighlightColor;
     TestTrue(TEXT("Enabled button retains a gold contour after view update"),Highlight.R>Highlight.B+.5f);
     const FPointerEvent Pointer(0,FVector2D(10),FVector2D(10),TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());
     Native->OnMouseEnter(FGeometry::MakeRoot(FVector2D(300,90),FSlateLayoutTransform()),Pointer);
     const auto Hover=Draw(Slate,Pixels,Prefix+FString::Printf(TEXT("-hover-%d"),Pass++));
     int32 Changed=0;for(int32 I=0;I<FMath::Min(Normal.Num(),Hover.Num());++I)if(Normal[I]!=Hover[I])++Changed;
     TestTrue(TEXT("Hover contour is actually painted"),Changed>20);
     Native->OnMouseLeave(Pointer);
     const auto Left=Draw(Slate,Pixels,FString());
     TestTrue(TEXT("Mouse leave restores source pixels"),Normal==Left);
     if(Action==EDYKResourceAction::Collect || Action==EDYKResourceAction::CancelWork || Action==EDYKResourceAction::Upgrade)
     {
      Button->SetAvailability(false,FString());
      const auto Gray=Draw(Slate,Pixels,Prefix+FString::Printf(TEXT("-disabled-%d"),Pass-1));
      TestTrue(TEXT("Unavailable button changes its artwork before hover"),Gray!=Normal);
      UI->SetFeedback(TEXT("This must not appear in the shared production template."));
      Native->OnMouseEnter(FGeometry::MakeRoot(FVector2D(300,90),FSlateLayoutTransform()),Pointer);
      TestTrue(TEXT("Disabled hover and feedback leave idle pixels unchanged"),Gray==Draw(Slate,Pixels,FString()));
      Native->OnMouseLeave(Pointer);Button->SetAvailability(true,FString());
     }
    }
   }
   UI->ReleaseSlateResources(true);
  }
 }
 BeginCleanup(Renderer);return true;
}
#endif
#endif
