#include "DYKCityWorkWidget.h"
#include "DYKCityOrders.h"
#include "DYKCityGameMode.h"
#include "DYKCityEditorWidget.h"
#include "DYKCityTerritoryWidget.h"
#include "DYKCityBuildingPreviewActor.h"
#include "DYKCityBuildingActor.h"
#include "EngineUtils.h"
#include "../UI/DYKCityHUDArt.h"
#include "../UI/DYKCityPresentation.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	FText CityText(const FString& Value) {return FText::FromString(Value);}
	const FDYKCityProductionSaveData* ScreenInstance(const FDYKPlayerProfileSaveData& P,FGuid Id)
	{return P.CityEconomy.Instances.FindByPredicate([Id](const auto& S){return S.InstanceId==Id;});}
	const FDYKCityResearchWorkSaveData* BuildingResearch(const FDYKPlayerProfileSaveData& P,FName Building)
	{
		const FDYKCityResearchWorkSaveData* Waiting=nullptr;
		for(const auto& Work:P.CityEconomy.ResearchQueue)
		{
			const auto* Definition=DYKCityResearch::Find(Work.ResearchId);
			if(!Definition || Definition->Building!=Building) continue;
			if(Work.StartedAtUnix>0) return &Work;
			if(!Waiting) Waiting=&Work;
		}
		return Waiting;
	}
	FString CityDuration(int64 Seconds)
	{
		Seconds=FMath::Max<int64>(0,Seconds);
		if(Seconds>=3600) return Seconds%3600==0 ? FString::Printf(TEXT("%lld ч"),Seconds/3600) : FString::Printf(TEXT("%lld ч %lld мин"),Seconds/3600,Seconds%3600/60);
		if(Seconds>=60) return Seconds%60==0 ? FString::Printf(TEXT("%lld мин"),Seconds/60) : FString::Printf(TEXT("%lld мин %lld с"),Seconds/60,Seconds%60);
		return FString::Printf(TEXT("%lld с"),Seconds);
	}
	void CityBrown(UWidgetTree* Tree,UCanvasPanel* Canvas,FVector2D Pos,FVector2D Size)
	{DYKCityHUDArt::Plate(Tree,Canvas,Pos,Size);}
	FString CityCosts(const FDYKResearchDefinition& D)
	{
		FString Result;
		for(const auto& C:D.Costs)
		{if(!Result.IsEmpty()) Result+=TEXT(" · "); Result+=FString::Printf(TEXT("%s: %lld"),DYKCityCatalog::ResourceName(FDYKResourceId(C.ResourceId.Name)),C.Amount);}
		return Result.IsEmpty() ? TEXT("Бесплатно") : Result;
	}
}

void UDYKCityActionButton::Invoke() {if(Screen && Screen->GetRenderGeneration()==Generation) Screen->Dispatch(Action,Index,Key);}

UDYKCityActionButton* UDYKCityWorkWidget::ActionButton(EDYKCityUIAction Action,int32 Index,const FText& Label,FVector2D Pos,FVector2D Size,FName Key,bool Enabled,bool Transparent)
{
	auto* Button=WidgetTree->ConstructWidget<UDYKCityActionButton>(); Button->Screen=this; Button->Action=Action; Button->Index=Index; Button->Key=Key;
	Button->Generation=RenderGeneration;
	const bool Left=Action==EDYKCityUIAction::Previous || Action==EDYKCityUIAction::ResearchPrevious;
	const bool Right=Action==EDYKCityUIAction::Next || Action==EDYKCityUIAction::ResearchNext;
	FSlateBrush B;
	if(Left || Right) B=DYKCityHUDArt::Brush(TEXT("WorkshopHUD"),Left ? FBox2D({578,814},{644,872}) : FBox2D({811,814},{877,872}));
	else if(!Transparent) B=DYKCityHUDArt::PlateBrush();
	else {B.DrawAs=ESlateBrushDrawType::Image; B.TintColor=FLinearColor(1,1,1,0);}
	FButtonStyle Style;
	Style.SetNormal(B); Style.SetHovered(B); Style.SetPressed(B); Style.SetDisabled(B);
	Style.Hovered.TintColor=Transparent ? FLinearColor(1,1,1,.06f) : FLinearColor(1.12f,1.09f,1,1);
	Style.Pressed.TintColor=Transparent ? FLinearColor(1,1,1,.12f) : FLinearColor(.72f,.68f,.55f,1);
	Style.Disabled.TintColor=Transparent ? FLinearColor(1,1,1,0) : FLinearColor(.46f,.46f,.46f,.75f);
	Style.SetNormalPadding(FMargin(0)); Style.SetPressedPadding(FMargin(0)); Button->SetStyle(Style); Button->SetIsEnabled(Enabled);
	Button->OnClicked.AddDynamic(Button,&UDYKCityActionButton::Invoke); Button->SetToolTipText(Label);
	DYKCityHUDArt::Place(DesignCanvas,Button,Pos,Size); Actions.Add(Button);
	if(!Label.IsEmpty() && !Left && !Right)
	{
		auto* Caption=DYKCityHUDArt::Label(WidgetTree,DesignCanvas,Label,Pos+FVector2D(14,5),Size-FVector2D(28,10),Size.Y>=65 ? 30 : 24,true);
		if(!Enabled) Caption->SetColorAndOpacity(FLinearColor(.62f,.58f,.48f,1));
	}
	return Button;
}

TSharedRef<SWidget> UDYKCityWorkWidget::RebuildWidget()
{
	if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if(!WidgetTree->RootWidget)
	{
		auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root; Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		DevelopmentButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCity","Development","Развитие города"));
		DevelopmentButton->OnClicked.AddDynamic(this,&ThisClass::ToggleClicked);
		if(auto* Caption=Cast<UTextBlock>(DevelopmentButton->GetContent())) Caption->SetFont(DYKCityHUDArt::FontInfo(22));
		auto* DevelopmentSlot=Root->AddChildToCanvas(DevelopmentButton); DevelopmentSlot->SetPosition({16,115}); DevelopmentSlot->SetAutoSize(true);
		ModalBlocker=WidgetTree->ConstructWidget<UButton>();
		FButtonStyle BlockingStyle=ModalBlocker->GetStyle();
		for(auto* Brush:{&BlockingStyle.Normal,&BlockingStyle.Hovered,&BlockingStyle.Pressed})
		{Brush->DrawAs=ESlateBrushDrawType::Image; Brush->SetResourceObject(nullptr); Brush->TintColor=FLinearColor(0,0,0,.35f);}
		ModalBlocker->SetStyle(BlockingStyle); ModalBlocker->OnClicked.AddDynamic(this,&ThisClass::CloseClicked);
		auto* BlockingSlot=Root->AddChildToCanvas(ModalBlocker); BlockingSlot->SetAnchors(FAnchors(0,0,1,1)); BlockingSlot->SetOffsets(FMargin(0));
		auto* Scale=WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit);
		auto* ModalSlot=Root->AddChildToCanvas(Scale); ModalSlot->SetAnchors(FAnchors(.025f,.025f,.975f,.975f)); ModalSlot->SetOffsets(FMargin(0));
		DesignSize=WidgetTree->ConstructWidget<USizeBox>(); DesignSize->SetWidthOverride(1448); DesignSize->SetHeightOverride(1086); Scale->AddChild(DesignSize);
		DesignCanvas=WidgetTree->ConstructWidget<UCanvasPanel>(); DesignSize->AddChild(DesignCanvas); ModalRoot=Scale;
	}
	RebuildModal(); return Super::RebuildWidget();
}
void UDYKCityWorkWidget::NativeConstruct()
{Super::NativeConstruct(); GetWorld()->GetTimerManager().SetTimer(WorkTimer,this,&ThisClass::Refresh,1.f,true); Refresh();}
void UDYKCityWorkWidget::NativeDestruct()
{
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(WorkTimer);
	if(SitePreview) SitePreview->Destroy(); SitePreview=nullptr; Super::NativeDestruct();
}
bool UDYKCityWorkWidget::BlocksWorldInput() const {return (State.IsModal() && !(ResourceScreen && !ResourceScreen->IsInterior())) || DYKCityPointer::HUDUnderPointer(this);}
UDYKCityActionButton* UDYKCityWorkWidget::FindAction(EDYKCityUIAction Action,int32 Index) const
{for(const auto& B:Actions) if(B->Action==Action && B->Index==Index) return B.Get(); return nullptr;}
void UDYKCityWorkWidget::ToggleClicked()
{
	auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>();
	if(City && City->GetLayoutEditor().IsActive()) return;
	if(City) City->CancelConstructionPlacement();
	if(State.Window==EDYKCityWindow::Construction) State.Close(); else State.OpenConstruction(); Message.Empty(); RebuildModal();
}
void UDYKCityWorkWidget::CloseClicked() {State.Close(); RebuildModal();}
void UDYKCityWorkWidget::OpenBuilding(FGuid Instance,FName Type)
{
	if(State.IsModal() && !(ResourceScreen && !ResourceScreen->IsInterior())) return;
	const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* Profile=Save ? Save->GetProfileSnapshot() : nullptr;
	const auto* Owned=Profile ? ScreenInstance(*Profile,Instance) : nullptr;
	if(!Owned || Profile->Buildings[Owned->BuildingIndex].BuildingId.Name!=Type) return;
	State.Select(Instance,Type); Message.Empty(); RebuildModal();
}
void UDYKCityWorkWidget::Refresh()
{
	auto* City=GetWorld() ? GetWorld()->GetAuthGameMode<ADYKCityGameMode>() : nullptr;
	auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	if(!City || !Save) return;
	const int64 Revision=Save->GetRevision();
	if(!DYKCityWork::Update(*Save,FDateTime::UtcNow().ToUnixTimestamp())) Message=TEXT("Не удалось сохранить прогресс. Повторите попытку.");
	if(Revision!=Save->GetRevision()) City->SyncLayoutVisuals();
	DevelopmentButton->SetIsEnabled(!City->GetLayoutEditor().IsActive());
	// Do not rebuild live buttons every second: retain pointer capture, focus and mouse-up.
	if(Revision!=Save->GetRevision() && State.IsModal() && !ResourceScreen) RebuildModal();
	if(ResourceScreen) if(const auto* Current=Save->GetProfileSnapshot()) ResourceScreen->UpdateView(ResourceView(*Current));
	if(LiveStatus)
	{
		const auto* Current=Save->GetProfileSnapshot();
		const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
		if(Current && !LiveResearch.IsNone())
		{
			const auto* W=Current->CityEconomy.ResearchQueue.FindByPredicate([this](const auto& Row){return Row.ResearchId==LiveResearch;});
			if(W) LiveStatus->SetText(CityText(W->StartedAtUnix>0 ? TEXT("Осталось: ")+CityDuration(W->CompletesAtUnix-Now) : TEXT("Ожидает ячейку или ресурсы")));
		}
		else if(Current) if(const auto* S=ScreenInstance(*Current,State.Instance); S && !S->Orders.IsEmpty())
		{
			const auto& O=S->Orders[0];
			LiveStatus->SetText(CityText(FString::Printf(TEXT("В здании: %lld · Заказов: %d / 3 · %s"),S->Stored,S->Orders.Num(),
				O.StartedAtUnix==0 ? TEXT("Ожидает места") : *CityDuration(O.CompletesAtUnix-Now))));
		}
	}
}
void UDYKCityWorkWidget::RebuildModal()
{
	if(!DesignCanvas) return;
	if(ResourceScreen) ResourceScreen->Deactivate();
	ResourceScreen=nullptr;
	++RenderGeneration;
	Actions.Reset(); LiveStatus=nullptr; LiveResearch=NAME_None; DesignCanvas->ClearChildren();
	ModalRoot->SetVisibility(State.IsModal() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ModalBlocker->SetVisibility(State.IsModal() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if(!State.IsModal()) return;
	const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P=Save ? Save->GetProfileSnapshot() : nullptr; if(!P) return;
	if(auto* Content=UDYKResourceBuildingWidget::FindDefinition(State.Building); Content && (State.Window==EDYKCityWindow::Building || (State.Window==EDYKCityWindow::Interior && !Content->bFirstOnly)))
	{
		const bool Inside=State.Window==EDYKCityWindow::Interior;
		const FVector2D Pixels=Inside ? Content->InteriorSize : Content->FirstSize;
		DesignSize->SetWidthOverride(Pixels.X); DesignSize->SetHeightOverride(Pixels.Y);
		ResourceScreen=WidgetTree->ConstructWidget<UDYKResourceBuildingWidget>(); ResourceScreen->Configure(Content,Inside);
		ResourceScreen->OnAction.AddDynamic(this,&ThisClass::ResourceAction);
		DYKCityHUDArt::Place(DesignCanvas,ResourceScreen,{0,0},Pixels);
		ResourceScreen->TakeWidget(); ResourceScreen->UpdateView(ResourceView(*P));
		ModalBlocker->SetVisibility(Inside ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PositionResourceWindow();
		return;
	}
	DesignSize->SetWidthOverride(1448); DesignSize->SetHeightOverride(1086);
	if(auto* Layout=Cast<UCanvasPanelSlot>(ModalRoot->Slot)) {Layout->SetAnchors(FAnchors(.025f,.025f,.975f,.975f)); Layout->SetAlignment({0,0}); Layout->SetOffsets(FMargin(0));}
	const auto* D=DYKCityCatalog::Find(FDYKBuildingId(State.Building).ToContentId());
	auto Title=State.Window==EDYKCityWindow::Construction ? NSLOCTEXT("DYKCity","Construction","Развитие города") : CityText(D ? D->DisplayName : TEXT("Город"));
	// Production windows have exactly one rendering path: ResourceScreen above.
	if(D && D->Category==EDYKCityBuildingCategory::Production && State.Window!=EDYKCityWindow::Construction)
	{ensureMsgf(false,TEXT("Shared production HUD asset is missing"));State.Close();ModalRoot->SetVisibility(ESlateVisibility::Collapsed);ModalBlocker->SetVisibility(ESlateVisibility::Collapsed);return;}
	const bool Detail=State.Window==EDYKCityWindow::Detail;
	FName Art=DYKCityBuildingUI::Art(State.Building,Detail);
	if(Detail)
	{
		const auto Items=DYKCityBuildingUI::Items(State.Building,State.LibraryTab);
		if(Items.IsValidIndex(State.SelectedItem)) Title=CityText(Items[State.SelectedItem].Name);
		if(DYKCityBuildingUI::Interior(State.Building)!=EDYKCityInterior::Barracks) Art=TEXT("AllTrapHUD");
	}
	DYKCityHUDArt::Frame(WidgetTree,DesignCanvas,Art,Title,CityText(Message.IsEmpty() ? TEXT("Выберите действие") : Message),!Detail,true);
	ActionButton(EDYKCityUIAction::Close,0,FText::GetEmpty(),{1240,76},{110,110},NAME_None,true,true)->SetToolTipText(NSLOCTEXT("DYKCity","Close","Закрыть"));
	if(State.Window!=EDYKCityWindow::Construction && State.Window!=EDYKCityWindow::Building)
		ActionButton(EDYKCityUIAction::Back,0,NSLOCTEXT("DYKCity","Back","Назад"),{235,215},{180,48});
	switch(State.Window)
	{
	case EDYKCityWindow::Construction: BuildConstruction(*P); break;
	case EDYKCityWindow::Building: BuildBuilding(*P); break;
	case EDYKCityWindow::Interior: BuildInterior(*P); break;
	case EDYKCityWindow::Detail: BuildDetail(*P); break;
	default: break;
	}
}
void UDYKCityWorkWidget::BuildConstruction(const FDYKPlayerProfileSaveData& P)
{
	const TCHAR* Labels[]={TEXT("Ресурсные здания"),TEXT("Специальные постройки"),TEXT("Городские постройки"),TEXT("Декор")};
	const FName Icons[]={TEXT("SawmillHUD"),TEXT("WorkshopHUD"),TEXT("LibraryHUD"),TEXT("WorkshopHUD")};
	const FBox2D Regions[]={FBox2D({266,512},{337,555}),FBox2D({689,25},{760,95}),FBox2D({963,247},{1000,285}),FBox2D({580,20},{645,83})};
	for(int32 I=0;I<4;++I)
	{
		auto* B=ActionButton(EDYKCityUIAction::BuildTab,I,FText::GetEmpty(),{240.+I*100.,242},{82,66});
		auto* Icon=WidgetTree->ConstructWidget<UImage>(); Icon->SetBrush(DYKCityHUDArt::Brush(Icons[I],Regions[I])); B->AddChild(Icon); B->SetToolTipText(CityText(Labels[I]));
		B->SetBackgroundColor(int32(State.BuildTab)==I ? FLinearColor(1.3f,1.15f,.5f,1) : FLinearColor::White);
	}
	const auto Types=DYKCityBuildingUI::ConstructionTypes(State.BuildTab);
	int32 Castle=1; for(const auto& B:P.Buildings) if(B.BuildingId.Name==TEXT("CITY-CASTLE")) Castle=B.Level;
	for(int32 I=0;I<Types.Num();++I)
	{
		const auto* D=DYKCityCatalog::Find(FDYKBuildingId(Types[I]).ToContentId());
		const auto& Rule=DYKCityBuildingRules::DEV_PLACEHOLDER().Buildings.FindChecked(Types[I]);
		const int32 Owned=P.Buildings.FilterByPredicate([&](const auto& B){return B.BuildingId.Name==Types[I];}).Num();
		const int32 Limit=Rule.LimitsByCastleLevel.IsValidIndex(Castle-1) ? Rule.LimitsByCastleLevel[Castle-1] : 1;
		const bool Available=Owned<Limit && Castle>=Rule.Levels[0].RequiredCastleLevel;
		const FVector2D Pos(242+(I%3)*332,335+(I/3)*240);
		// Separate image content, frame and live caption. No baked card text underneath.
		FBox2D IconRegion=Regions[int32(State.BuildTab)]; FName IconArt=Icons[int32(State.BuildTab)];
		if(D->Id.GetName()==TEXT("CITY-SAWMILL") || D->Id.GetName()==TEXT("CITY-QUARRY") || D->Id.GetName()==TEXT("CITY-MILL"))
		{IconArt=DYKCityBuildingUI::Art(D->Id.GetName()); IconRegion=FBox2D({245,270},{450,429});}
		DYKCityHUDArt::Image(WidgetTree,DesignCanvas,IconArt,IconRegion,Pos+FVector2D(65,12),{178,112});
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(FString::Printf(TEXT("Замок: %d · %d / %d"),Rule.Levels[0].RequiredCastleLevel,Owned,Limit)),Pos+FVector2D(10,128),{288,44},20);
		if(const auto* Skin=DYKCityHUDArt::Presentation()) DYKCityHUDArt::Texture(WidgetTree,DesignCanvas,Skin->CardFrame,Pos,{308,225});
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(D->DisplayName),Pos+FVector2D(22,181),{264,32},25,true);
		auto* B=ActionButton(EDYKCityUIAction::Build,I,FText::GetEmpty(),Pos,{308,225},Types[I],Available,true);
		B->SetToolTipText(CityText(DYKCityBuildingUI::Description(Types[I])+(Available ? TEXT("\nВыбрать место строительства") : TEXT("\nНе выполнены условия или достигнут лимит"))));
	}
}
void UDYKCityWorkWidget::BuildBuilding(const FDYKPlayerProfileSaveData& P)
{
	const auto* S=ScreenInstance(P,State.Instance); if(!S) {State.Close(); ModalRoot->SetVisibility(ESlateVisibility::Collapsed); return;}
	const auto& B=P.Buildings[S->BuildingIndex]; const auto* D=DYKCityCatalog::Find(B.BuildingId);
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(DYKCityBuildingUI::Description(State.Building)),{275,250},{900,95},28);
	FString Process=TEXT("Нет активных процессов.");
	if(const auto* W=P.CityEconomy.Work.FindByPredicate([this](const auto& Work){return Work.InstanceId==State.Instance;}))
		Process=W->StartedAtUnix>0 ? TEXT("Работы: ")+CityDuration(W->CompletesAtUnix-FDateTime::UtcNow().ToUnixTimestamp()) : TEXT("Строительная заявка ожидает строителя или ресурсов.");
	else if(D->Resource.IsValid())
	{
		const auto V=DYKCityEconomy::View(P,State.Instance,FDateTime::UtcNow().ToUnixTimestamp());
		Process=FString::Printf(TEXT("Накоплено: %lld / %lld\nДоступно к сбору: %lld"),V.Stored,V.Capacity,V.Collectable);
		if(S->OrdersVersion==1) Process+=FString::Printf(TEXT("\nОчередь заказов: %d / %d"),S->Orders.Num(),DYKCityOrders::QueueCapacity);
	}
	else for(const auto& ResearchWork:P.CityEconomy.ResearchQueue)
		if(const auto* R=DYKCityResearch::Find(ResearchWork.ResearchId); R && R->Building==State.Building)
		{Process=R->DisplayName+TEXT("\n")+(ResearchWork.StartedAtUnix>0 ? CityDuration(ResearchWork.CompletesAtUnix-FDateTime::UtcNow().ToUnixTimestamp()) : TEXT("Ожидает исследовательскую ячейку")); LiveResearch=R->Id; break;}
	LiveStatus=DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(FString::Printf(TEXT("Уровень %d\n%s"),B.Level,*Process)),{290,365},{870,155},30);
	if(D->Resource.IsValid()) ActionButton(EDYKCityUIAction::Collect,0,NSLOCTEXT("DYKCity","Collect","Собрать"),{430,522},{590,55},NAME_None,DYKCityEconomy::View(P,State.Instance,FDateTime::UtcNow().ToUnixTimestamp()).Collectable>0);
	FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=State.Instance; R.Kind=EDYKCityBuildingWorkKind::Upgrade; R.BuildingId=D->Id; R.ExpectedLevel=B.Level;
	FDYKCityBuildingWorkPlan Plan;
	const bool HasWork=P.CityEconomy.Work.ContainsByPredicate([this](const auto& W){return W.InstanceId==State.Instance;});
	ActionButton(EDYKCityUIAction::Upgrade,0,NSLOCTEXT("DYKCity","UpgradeBuilding","Улучшить здание"),{330,605},{365,60},NAME_None,!HasWork && DYKCityBuildingRules::Prepare(P,R,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan));
	ActionButton(EDYKCityUIAction::CancelWork,0,NSLOCTEXT("DYKCity","CancelWork","Отменить работу"),{745,605},{365,60},NAME_None,HasWork);
	ActionButton(EDYKCityUIAction::Visit,0,NSLOCTEXT("DYKCity","Visit","Посетить"),{330,690},{780,68},NAME_None,!B.bUnderConstruction && DYKCityBuildingUI::Interior(State.Building)!=EDYKCityInterior::None);
	ActionButton(EDYKCityUIAction::Rotate,0,NSLOCTEXT("DYKCity","Rotate","Повернуть"),{330,790},{365,65});
	ActionButton(EDYKCityUIAction::Move,0,NSLOCTEXT("DYKCity","Move","Переместить"),{745,790},{365,65});
}
void UDYKCityWorkWidget::BuildInterior(const FDYKPlayerProfileSaveData& P)
{
	const auto Kind=DYKCityBuildingUI::Interior(State.Building);
	if(State.Building==TEXT("CITY-HOUSE") || Kind==EDYKCityInterior::Warehouse)
	{
		if(Kind==EDYKCityInterior::Warehouse)
		{
			FString Storage;
			for(const auto Id:DYKCityCatalog::Resources())
				if(DYKCityCatalog::ResourceCapacity(Id)!=MAX_uint32) Storage+=FString::Printf(TEXT("%s: %lld / %lld\n"),DYKCityCatalog::ResourceName(Id),DYKCityEconomy::Balance(P,Id),DYKCityEconomy::Capacity(P,Id));
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,NSLOCTEXT("DYKCity","Capacity","Вместимость склада"),{300,310},{850,60},38);
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Storage),{300,420},{850,250},34);
		}
		else if(const auto* S=ScreenInstance(P,State.Instance))
		{
			const auto V=DYKCityEconomy::View(P,State.Instance,FDateTime::UtcNow().ToUnixTimestamp());
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(DYKCityBuildingUI::Description(State.Building)),{290,315},{870,110},30);
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(FString::Printf(TEXT("Золото: %lld / %lld\nОдин цикл дома"),V.Stored,V.Capacity)),{330,490},{790,130},36);
			ActionButton(EDYKCityUIAction::Collect,0,NSLOCTEXT("DYKCity","CollectGold","Собрать золото"),{420,715},{600,75},NAME_None,V.Collectable>0);
		}
		return;
	}
	if(Kind==EDYKCityInterior::Library)
	{
		const TCHAR* Tabs[]={TEXT("Экономика"),TEXT("Военное дело"),TEXT("Оборона")};
		for(int32 I=0;I<3;++I) ActionButton(EDYKCityUIAction::LibraryTab,I,CityText(Tabs[I]),{420.+I*270.,218},{255,48});
	}
	const auto* Skin=DYKCityHUDArt::Presentation();
	const auto Items=DYKCityBuildingUI::Items(State.Building,State.LibraryTab); State.TurnPage(0,Items.Num());
	for(int32 SlotIndex=0;SlotIndex<6;++SlotIndex)
	{
		const int32 ItemIndex=State.Page*6+SlotIndex; if(!Items.IsValidIndex(ItemIndex)) break;
		const auto& Item=Items[ItemIndex]; const FVector2D Pos(238+(SlotIndex%3)*335,275+(SlotIndex/3)*270);
		const bool Locked=DYKCityBuildingUI::IsLocked(P,Item);
		if(Locked)
			DYKCityHUDArt::Image(WidgetTree,DesignCanvas,TEXT("WorkshopHUD"),FBox2D({580,550},{878,780}),Pos+FVector2D(7,7),{296,226});
		else if(!DYKCityHUDArt::Illustration(WidgetTree,DesignCanvas,Item.Key,Pos+FVector2D(13,14),{284,184}))
			DYKCityHUDArt::Image(WidgetTree,DesignCanvas,Item.Art,Item.ImageRegion,Pos+FVector2D(13,14),{284,184});
		if(Skin) DYKCityHUDArt::Texture(WidgetTree,DesignCanvas,Skin->CardFrame,Pos,{310,245});
		if(!Locked)
		{
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Item.Name),Pos+FVector2D(23,198),{264,35},29,true);
			if(DYKCityBuildingUI::ShowNew(P,Item) && Skin)
				DYKCityHUDArt::Texture(WidgetTree,DesignCanvas,Skin->NewRibbon,Pos+FVector2D(-6,-3),{145,60},FBox2D({.07,.16},{.93,.89}));
		}
		else
			DYKCityHUDArt::Image(WidgetTree,DesignCanvas,TEXT("WorkshopHUD"),FBox2D({706,713},{751,761}),Pos+FVector2D(135,195),{40,43});
		auto* Click=ActionButton(EDYKCityUIAction::Item,ItemIndex,FText::GetEmpty(),Pos,{310,245},Item.Key,true,true);
		Click->SetToolTipText(CityText(Locked ? TEXT("Пока недоступно") : Item.Description));
	}
	const int32 Pages=FMath::Max(1,(Items.Num()+5)/6);
	ActionButton(EDYKCityUIAction::Previous,0,CityText(TEXT("Предыдущая страница")),{580,816},{66,58},NAME_None,State.Page>0);
	ActionButton(EDYKCityUIAction::Next,0,CityText(TEXT("Следующая страница")),{811,816},{66,58},NAME_None,State.Page+1<Pages);
	for(int32 I=0;I<Pages;++I)
		DYKCityHUDArt::Image(WidgetTree,DesignCanvas,TEXT("WorkshopHUD"),I==State.Page ? FBox2D({670,821},{703,857}) : FBox2D({711,823},{744,856}),{671.+I*41.,826},{30,30});
}

TArray<FName> UDYKCityWorkWidget::ResearchOptions(const FDYKPlayerProfileSaveData& P,const FDYKCityInteriorItem& Item) const
{
	TArray<FName> Result; if(Item.bFuture) return Result;
	const auto* Root=DYKCityResearch::Find(Item.ResearchRoot); if(!Root) return Result;
	if(!P.CityEconomy.CompletedResearch.Contains(Root->Id)) {Result.Add(Root->Id); return Result;}
	TSet<FName> Family; Family.Add(Root->Id);
	for(int32 Pass=0;Pass<4;++Pass) for(const auto& D:DYKCityResearch::DEV_PLACEHOLDER().Definitions)
		if(D.Building==State.Building) for(const auto Dependency:D.Prerequisites) if(Family.Contains(Dependency)) {Family.Add(D.Id); break;}
	for(const auto& D:DYKCityResearch::DEV_PLACEHOLDER().Definitions)
	{
		if(!Family.Contains(D.Id) || P.CityEconomy.CompletedResearch.Contains(D.Id)) continue;
		if(D.TargetType.IsValid() && Root->Unlock.Kind==EDYKContentKind::Warrior && D.TargetType.ToContentId()!=Root->Unlock) continue;
		bool Ready=true; for(const auto Dependency:D.Prerequisites) Ready&=P.CityEconomy.CompletedResearch.Contains(Dependency);
		if(Ready) Result.Add(D.Id);
	}
	return Result;
}
void UDYKCityWorkWidget::BuildDetail(const FDYKPlayerProfileSaveData& P)
{
	const auto Items=DYKCityBuildingUI::Items(State.Building,State.LibraryTab); if(!Items.IsValidIndex(State.SelectedItem)) {State.Visit(); return;}
	const auto& Item=Items[State.SelectedItem];
	const bool Locked=DYKCityBuildingUI::IsLocked(P,Item);
	const bool Known=P.CityEconomy.CompletedResearch.Contains(Item.ResearchRoot);
	if(!Locked)
	{
		if(!DYKCityHUDArt::Illustration(WidgetTree,DesignCanvas,Item.Key,{514,230},{420,230}))
			DYKCityHUDArt::Image(WidgetTree,DesignCanvas,Item.Art,Item.ImageRegion,{514,230},{420,230});
	}
	else DYKCityHUDArt::Image(WidgetTree,DesignCanvas,TEXT("WorkshopHUD"),FBox2D({589,554},{870,772}),{579,240},{290,225});
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Locked ? TEXT("Недоступно") : Known ? TEXT("Изучено") : TEXT("Не изучено")),{260,326},{220,70},28);
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(State.Building==TEXT("CITY-WORKSHOP") ? TEXT("Ловушка") : State.Building==TEXT("CITY-BARRACKS") ? TEXT("Боец") : TEXT("Исследование")),{975,326},{215,65},28);
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Locked ? TEXT("Условия открытия ещё не выполнены.") : Item.Description),{275,470},{900,68},25);
	CityBrown(WidgetTree,DesignCanvas,{255,530},{940,118});
	FString Stats=Locked ? TEXT("Характеристики пока недоступны") : TEXT("Постоянное исследование");
	if(!Locked && DYKCityBuildingUI::Interior(State.Building)==EDYKCityInterior::Workshop)
		Stats=TEXT("Автоматическое срабатывание\nНеуязвима · Ограниченный запас зарядов");
	else if(!Locked) if(const auto* Root=DYKCityResearch::Find(Item.ResearchRoot))
	{
		if(const auto* Unit=DYKUnitCatalog::Find(Root->Unlock))
		{
			const auto Bonus=DYKCityResearch::Bonus(P,*Unit);
			const auto Personal=DYKCityResearch::Bonus(P,*Unit,EDYKResearchScope::Type);
			Stats=FString::Printf(TEXT("Урон: %d    Здоровье: %d    Физическая броня: %d\nМагическая броня: %d    Размещение: %d припасов"),Unit->Base.Attack.DamagePerHit.Value+Bonus.Damage,Unit->Base.MaxHP.Value+Bonus.HP,Unit->Base.PhysicalArmor.Value+Bonus.PhysicalArmor,Unit->Base.MagicalArmor.Value,Unit->Base.PlacementSupplies.Value);
			if(Personal.HP || Personal.PhysicalArmor || Personal.Damage) Stats+=FString::Printf(TEXT("\nПерсональные бонусы: HP +%d · Броня +%d · Урон +%d"),Personal.HP,Personal.PhysicalArmor,Personal.Damage);
		}
		else if(Root->TargetLine!=EDYKUnitLine::None)
			Stats=FString::Printf(TEXT("Бонус линии\nЗдоровье: +%d · Броня: +%d · Урон: +%d"),Root->Bonus.HP,Root->Bonus.PhysicalArmor,Root->Bonus.Damage);
	}
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Stats),{285,542},{880,94},26,true);
	const auto Options=Locked ? TArray<FName>() : ResearchOptions(P,Item); ResearchChoice=FMath::Clamp(ResearchChoice,0,FMath::Max(0,Options.Num()-1));
	const auto* R=Options.IsEmpty() ? nullptr : DYKCityResearch::Find(Options[ResearchChoice]);
	FString Status=Locked ? TEXT("Пока недоступно") : TEXT("Доступно");
	const auto* Queued=R ? P.CityEconomy.ResearchQueue.FindByPredicate([R](const auto& W){return W.ResearchId==R->Id;}) : nullptr;
	if(Queued) Status=Queued->StartedAtUnix>0 ? TEXT("Осталось: ")+CityDuration(Queued->CompletesAtUnix-FDateTime::UtcNow().ToUnixTimestamp()) : TEXT("Ожидает ячейку или ресурсы");
	LiveStatus=DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Status),{330,664},{790,35},25);
	if(R)
	{
		LiveResearch=R->Id;
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(R->DisplayName),{345,717},{760,42},28);
		ActionButton(EDYKCityUIAction::ResearchPrevious,0,CityText(TEXT("Предыдущее улучшение")),{260,710},{60,53},NAME_None,ResearchChoice>0);
		ActionButton(EDYKCityUIAction::ResearchNext,0,CityText(TEXT("Следующее улучшение")),{1130,710},{60,53},NAME_None,ResearchChoice+1<Options.Num());
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(CityCosts(*R)+TEXT(" · ")+CityDuration(R->DurationSeconds)),{290,772},{870,34},24);
		ActionButton(EDYKCityUIAction::Research,0,R->Id==Item.ResearchRoot ? NSLOCTEXT("DYKCity","Learn","Изучить") : NSLOCTEXT("DYKCity","Improve","Улучшить"),{355,820},{355,64},R->Id,DYKCityResearch::CanEnqueue(P,R->Id));
	}
	else ActionButton(EDYKCityUIAction::Research,0,CityText(Locked ? TEXT("Недоступно") : TEXT("Изучено")),{355,820},{355,64},NAME_None,false);
	ActionButton(EDYKCityUIAction::CancelResearch,0,NSLOCTEXT("DYKCity","CancelResearch","Отменить"),{755,820},{355,64},Queued ? FName(*Queued->WorkId.ToString()) : NAME_None,Queued!=nullptr);
}

void UDYKCityWorkWidget::Dispatch(EDYKCityUIAction Action,int32 Index,FName Key)
{
	auto* City=GetWorld() ? GetWorld()->GetAuthGameMode<ADYKCityGameMode>() : nullptr;
	auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P=Save ? Save->GetProfileSnapshot() : nullptr; if(!City || !P) return;
	const int64 Now=FDateTime::UtcNow().ToUnixTimestamp(); Message.Empty();
	const auto* S=ScreenInstance(*P,State.Instance);
	switch(Action)
	{
	case EDYKCityUIAction::Close: State.Close(); break;
	case EDYKCityUIAction::Back: if(State.Window==EDYKCityWindow::Detail) State.Window=EDYKCityWindow::Interior; else State.Window=EDYKCityWindow::Building; break;
	case EDYKCityUIAction::BuildTab: State.BuildTab=static_cast<EDYKCityBuildTab>(FMath::Clamp(Index,0,3)); break;
	case EDYKCityUIAction::LibraryTab: State.LibraryTab=FMath::Clamp(Index,0,2); State.Page=0; break;
	case EDYKCityUIAction::Build: if(City->BeginConstruction(Key)) State.Close(); break;
	case EDYKCityUIAction::Visit: if(S) {const auto* D=DYKCityCatalog::Find(P->Buildings[S->BuildingIndex].BuildingId);if(D && (D->Category==EDYKCityBuildingCategory::Production || !P->Buildings[S->BuildingIndex].bUnderConstruction)) State.Visit();} break;
	case EDYKCityUIAction::Move: if(City->SelectBuilding(State.Instance) && City->BeginLayoutEdit()) State.Close(); break;
	case EDYKCityUIAction::Rotate:
		if(S) {const auto B=P->Buildings[S->BuildingIndex]; if(!DYKCityTerritory::Move(*Save,FGuid::NewGuid(),State.Instance,B.CellOrigin,(B.RotationQuarterTurns+1)%4,DYKCityTerritory::DEV_PLACEHOLDER())) Message=TEXT("Для поворота недостаточно места.");} break;
	case EDYKCityUIAction::Upgrade:
		if(S) {const auto B=P->Buildings[S->BuildingIndex]; FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=State.Instance; R.BuildingId=FDYKBuildingId(State.Building); R.Kind=EDYKCityBuildingWorkKind::Upgrade; R.ExpectedLevel=B.Level; if(!DYKCityWork::Enqueue(*Save,R,Now)) Message=TEXT("Улучшение недоступно или не удалось сохранить.");} break;
	case EDYKCityUIAction::CancelWork:
		if(const auto* W=P->CityEconomy.Work.FindByPredicate([this](const auto& Work){return Work.InstanceId==State.Instance;})) {const auto WorkId=W->WorkId; if(!DYKCityWork::Cancel(*Save,FGuid::NewGuid(),WorkId,Now)) Message=TEXT("Не удалось отменить работу.");}
        else if(ResourceScreen && ResourceScreen->Definition && ResourceScreen->Definition->bFirstOnly)
        {
         if(const auto* Research=BuildingResearch(*P,State.Building))
         {
          const FGuid Id=Research->WorkId;
          if(!DYKCityResearch::Cancel(*Save,FGuid::NewGuid(),Id,Now)) Message=TEXT("Не удалось отменить исследование.");
         }
        }
        else if(ResourceScreen && S && !S->Orders.IsEmpty()) {const auto Order=S->Orders[0].OrderId; if(!DYKCityOrders::CancelCurrent(*Save,FGuid::NewGuid(),State.Instance,Order,Now)) Message=TEXT("Заказ уже завершён или не удалось сохранить отмену.");} break;
	case EDYKCityUIAction::Collect:
		if(!PendingCollect.IsValid()) PendingCollect=FGuid::NewGuid();
		if(DYKCityEconomy::Collect(*Save,PendingCollect,State.Instance,Now)) PendingCollect.Invalidate(); else Message=TEXT("Сбор недоступен или не удалось сохранить."); break;
	case EDYKCityUIAction::Order: if(!DYKCityOrders::Enqueue(*Save,FGuid::NewGuid(),State.Instance,Index,Now)) Message=TEXT("Очередь заполнена или не удалось сохранить заказ."); break;
	case EDYKCityUIAction::Item:
		if(State.Window==EDYKCityWindow::Interior && DYKCityBuildingUI::Items(State.Building,State.LibraryTab).IsValidIndex(Index))
		{
			const auto Item=DYKCityBuildingUI::Items(State.Building,State.LibraryTab)[Index];
			if(!DYKCityBuildingUI::IsLocked(*P,Item) && !P->CityEconomy.VisitedCards.Contains(Item.Key)
				&& !DYKCityBuildingUI::VisitCard(*Save,FGuid::NewGuid(),State.Building,State.LibraryTab,Index))
			{Message=TEXT("Не удалось сохранить просмотр. Повторите попытку."); break;}
			State.SelectedItem=Index; State.Window=EDYKCityWindow::Detail; ResearchChoice=0;
		} break;
	case EDYKCityUIAction::Previous: State.TurnPage(-1,DYKCityBuildingUI::Items(State.Building,State.LibraryTab).Num()); break;
	case EDYKCityUIAction::Next: State.TurnPage(1,DYKCityBuildingUI::Items(State.Building,State.LibraryTab).Num()); break;
	case EDYKCityUIAction::ResearchPrevious: ResearchChoice=FMath::Max(0,ResearchChoice-1); break;
	case EDYKCityUIAction::ResearchNext: ++ResearchChoice; break;
	case EDYKCityUIAction::Research:
	{
		const auto* R=DYKCityResearch::Find(Key);
		const auto Items=DYKCityBuildingUI::Items(State.Building,State.LibraryTab);
		if(State.Window!=EDYKCityWindow::Detail || !R || R->Building!=State.Building
			|| !Items.IsValidIndex(State.SelectedItem) || !ResearchOptions(*P,Items[State.SelectedItem]).Contains(Key))
		{Message=TEXT("Это исследование недоступно в данном здании."); break;}
		if(!DYKCityResearch::Enqueue(*Save,FGuid::NewGuid(),Key,Now)) Message=TEXT("Исследование недоступно или не удалось сохранить."); break;
	}
	case EDYKCityUIAction::CancelResearch: {FGuid WorkId; if(!FGuid::Parse(Key.ToString(),WorkId) || !DYKCityResearch::Cancel(*Save,FGuid::NewGuid(),WorkId,Now)) Message=TEXT("Исследование уже завершено или не удалось сохранить отмену.");} break;
	}
	City->SyncLayoutVisuals(); RebuildModal();
}
void UDYKCityWorkWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	if(ResourceScreen) PositionResourceWindow();
	const auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>(); FIntPoint Cell;
	if(State.IsModal() || !City || City->GetConstructionType().IsNone() || DYKCityPointer::HUDUnderPointer(this) || !DYKCityEditorInput::CursorCell(GetOwningPlayer(),Cell))
	{if(SitePreview) SitePreview->SetActorHiddenInGame(true); return;}
	if(!SitePreview || SitePreviewType!=City->GetConstructionType())
	{
		if(SitePreview) SitePreview->Destroy(); SitePreviewType=City->GetConstructionType(); SitePreview=GetWorld()->SpawnActor<ADYKCityBuildingPreviewActor>();
		if(SitePreview && !SitePreview->Configure(SitePreviewType,true)) {SitePreview->Destroy(); SitePreview=nullptr;}
	}
	if(SitePreview) {FDYKBuildingSaveData B; B.BuildingId=FDYKBuildingId(SitePreviewType).ToContentId(); B.CellOrigin=Cell; SitePreview->SetActorHiddenInGame(false); SitePreview->SetActorLocation(DYKCityTerritory::BuildingLocation(B,DYKCityTerritory::DEV_PLACEHOLDER()));}
}

FDYKResourceBuildingView UDYKCityWorkWidget::ResourceView(const FDYKPlayerProfileSaveData& P) const
{
	FDYKResourceBuildingView V; V.Instance=State.Instance;
	const auto* S=ScreenInstance(P,State.Instance); if(!S) return V;
	const auto& OwnedBuilding=P.Buildings[S->BuildingIndex];
    const auto* BuildingData=DYKCityCatalog::Find(OwnedBuilding.BuildingId);
    if(BuildingData)
    {
     V.BuildingName=CityText(BuildingData->DisplayName);
     V.ResourceName=CityText(BuildingData->ProductName ? BuildingData->ProductName : DYKCityCatalog::ResourceName(BuildingData->Resource));
     if(BuildingData->Category==EDYKCityBuildingCategory::Production)
     {
      V.Description=CityText(TEXT("Производство: ")+V.ResourceName.ToString()+TEXT(".\nЗаказы выполняются по очереди."));
      const auto* Art=ResourceScreen && ResourceScreen->Definition && ResourceScreen->IsInterior()
       ? ResourceScreen->Definition->InteriorIllustrations.Find(OwnedBuilding.BuildingId.Name)
       : nullptr;
      for(const auto& Recipe:DYKCityOrders::DEV_PLACEHOLDER())
      {
       const int32 Index=V.Recipes.Num();
       auto& Entry=V.Recipes.Add_GetRef({Recipe.Amount,Recipe.Seconds});
       if(Art && Art->Images.IsValidIndex(Index))
        Entry.Illustration=Art->Images[Index].Get();
      }
     }
    }
    V.QueueCapacity=DYKCityOrders::QueueCapacity;
    V.bCanOrder=!OwnedBuilding.bUnderConstruction;
    V.Level=P.Buildings[S->BuildingIndex].Level;
 V.Process=TEXT("Нет активных исследований");
	V.bCanVisit=BuildingData && BuildingData->Category==EDYKCityBuildingCategory::Production ? true : !OwnedBuilding.bUnderConstruction;
	const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
	const auto Store=DYKCityEconomy::View(P,State.Instance,Now);
	V.Stored=Store.Stored; V.Capacity=Store.Capacity; V.Orders=S->Orders.Num(); V.bCanCollect=Store.Collectable>0;
	if(!S->Orders.IsEmpty())
	{
        V.bCanCancel=true;
		const auto& O=S->Orders[0]; V.bWorking=O.StartedAtUnix>0; V.Seconds=FMath::Max<int64>(0,O.CompletesAtUnix-Now);
	}
	const auto* BuildingWork=P.CityEconomy.Work.FindByPredicate([this](const auto& Row){return Row.InstanceId==State.Instance;});
	if(BuildingWork)
	{
		V.bCanCancel=true;
		V.bWorking=BuildingWork->StartedAtUnix>0;
		V.Seconds=FMath::Max<int64>(0,BuildingWork->CompletesAtUnix-Now);
		V.WorkName=CityText(BuildingWork->bUpgrade ? TEXT("Улучшение здания") : TEXT("Строительство"));
		V.Process=V.WorkName.ToString()+TEXT("\n")+(V.bWorking ? CityDuration(V.Seconds) : TEXT("Ожидает строителя или ресурсов"));
	}
	if(ResourceScreen && ResourceScreen->Definition && ResourceScreen->Definition->bFirstOnly)
	{
		if(const auto* Research=BuildingResearch(P,State.Building); Research && !BuildingWork)
		{
			V.bCanCancel=true;
			V.bWorking=Research->StartedAtUnix>0;
			V.Seconds=FMath::Max<int64>(0,Research->CompletesAtUnix-Now);
			V.WorkName=CityText(DYKCityResearch::Find(Research->ResearchId)->DisplayName);
			V.Process=V.WorkName.ToString()+TEXT("\n")+(V.bWorking ? CityDuration(V.Seconds) : TEXT("Ожидает ячейку"));
		}
		if(V.WorkName.IsEmpty()) V.WorkName=CityText(TEXT("Нет активного улучшения"));
		V.WorkTime=CityText(V.bWorking ? CityDuration(V.Seconds) : V.bCanCancel ? TEXT("Ожидает") : TEXT("—"));
	}
    FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=State.Instance; R.Kind=EDYKCityBuildingWorkKind::Upgrade;
	R.BuildingId=FDYKBuildingId(State.Building); R.ExpectedLevel=P.Buildings[S->BuildingIndex].Level;
	FDYKCityBuildingWorkPlan Plan;
	V.bCanUpgrade= DYKCityBuildingRules::Prepare(P,R,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan);
	return V;
}
void UDYKCityWorkWidget::ResourceAction(EDYKResourceAction Action,int32 Index)
{
	switch(Action)
	{
	case EDYKResourceAction::Close: Dispatch(EDYKCityUIAction::Close); break;
	case EDYKResourceAction::Back: Dispatch(EDYKCityUIAction::Back); break;
	case EDYKResourceAction::Collect: Dispatch(EDYKCityUIAction::Collect); break;
	case EDYKResourceAction::Upgrade: Dispatch(EDYKCityUIAction::Upgrade); break;
	case EDYKResourceAction::CancelWork: Dispatch(EDYKCityUIAction::CancelWork); break;
	case EDYKResourceAction::Visit: Dispatch(EDYKCityUIAction::Visit); break;
	case EDYKResourceAction::Rotate: Dispatch(EDYKCityUIAction::Rotate); break;
	case EDYKResourceAction::Move: Dispatch(EDYKCityUIAction::Move); break;
	case EDYKResourceAction::Order: Dispatch(EDYKCityUIAction::Order,Index); break;
	default: break;
	}
 if(ResourceScreen && !Message.IsEmpty()) ResourceScreen->SetFeedback(Message);
}
void UDYKCityWorkWidget::PositionResourceWindow()
{
	auto* Layout=ModalRoot ? Cast<UCanvasPanelSlot>(ModalRoot->Slot) : nullptr;
	if(!Layout || !ResourceScreen) return;
	if(ResourceScreen->IsInterior())
	{
		// The old .025..975 rectangle reduced about its center by exactly 1.5.
		Layout->SetAnchors(FAnchors(.18333333f,.18333333f,.81666667f,.81666667f)); Layout->SetAlignment({0,0}); Layout->SetOffsets(FMargin(0)); return;
	}
	FVector2D Viewport=UWidgetLayoutLibrary::GetViewportSize(this)/FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
	if(Viewport.X<=0 || Viewport.Y<=0) Viewport={1920,1080};
	const FVector2D Popup=UDYKResourceBuildingWidget::PopupSize(Viewport,ResourceScreen->Definition->FirstSize);
	FVector2D Anchor=Viewport*.5;
	for(TActorIterator<ADYKCityBuildingActor> It(GetWorld());It;++It) if(It->InstanceId==State.Instance)
	{
		const FVector Center=It->Body ? It->Body->Bounds.Origin : It->GetActorLocation();
		if(GetOwningPlayer()) UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Center,Anchor,false);
		break;
	}
	Layout->SetAnchors(FAnchors(0)); Layout->SetAlignment({0,0});
	Layout->SetPosition(UDYKResourceBuildingWidget::PopupPosition(Anchor,Viewport,Popup)); Layout->SetSize(Popup);
}
