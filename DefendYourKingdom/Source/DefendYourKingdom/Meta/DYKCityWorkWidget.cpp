#include "DYKCityWorkWidget.h"
#include "DYKCityOrders.h"
#include "DYKCityGameMode.h"
#include "DYKCityEditorWidget.h"
#include "DYKCityTerritoryWidget.h"
#include "DYKCityBuildingPreviewActor.h"
#include "DYKCityBuildingActor.h"
#include "EngineUtils.h"
#include "../UI/DYKCityHUDArt.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	FText CityText(const FString& Value) {return FText::FromString(Value);}
	const FDYKCityProductionSaveData* ScreenInstance(const FDYKPlayerProfileSaveData& P,FGuid Id)
	{return P.CityEconomy.Instances.FindByPredicate([Id](const auto& S){return S.InstanceId==Id;});}
	FString CityDuration(int64 Seconds)
	{
		Seconds=FMath::Max<int64>(0,Seconds);
		if(Seconds>=3600) return FString::Printf(TEXT("%lld ч %lld мин"),Seconds/3600,Seconds%3600/60);
		if(Seconds>=60) return FString::Printf(TEXT("%lld мин %lld с"),Seconds/60,Seconds%60);
		return FString::Printf(TEXT("%lld с"),Seconds);
	}
	void CityBrown(UWidgetTree* Tree,UCanvasPanel* Canvas,FVector2D Pos,FVector2D Size)
	{DYKCityHUDArt::Image(Tree,Canvas,TEXT("WorkshopHUD"),FBox2D({258,469},{288,497}),Pos,Size);}
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
	auto Brush=DYKCityHUDArt::Brush(TEXT("SawmillHUD"),FBox2D({271,646},{439,653}));
	FButtonStyle Style=Button->GetStyle(); Style.SetNormal(Brush); Style.SetHovered(Brush); Style.SetPressed(Brush); Style.SetDisabled(Brush);
	if(Transparent)
	{
		Style.Normal.TintColor=FLinearColor(1,1,1,0); Style.Hovered.TintColor=FLinearColor(1,1,1,.10f);
		Style.Pressed.TintColor=FLinearColor(1,1,1,.18f); Style.Disabled.SetResourceObject(nullptr); Style.Disabled.TintColor=FLinearColor(0,0,0,.55f);
	}
	Style.SetNormalPadding(FMargin(8)); Style.SetPressedPadding(FMargin(8)); Button->SetStyle(Style); Button->SetIsEnabled(Enabled);
	if(!Label.IsEmpty()) {auto* Text=DYKUI::Text(WidgetTree,Label,26); Text->SetAutoWrapText(true); Text->SetColorAndOpacity(FLinearColor(1,.93f,.75f,1)); Button->AddChild(Text);}
	Button->OnClicked.AddDynamic(Button,&UDYKCityActionButton::Invoke); Button->SetToolTipText(Label);
	DYKCityHUDArt::Place(DesignCanvas,Button,Pos,Size); Actions.Add(Button); return Button;
}

TSharedRef<SWidget> UDYKCityWorkWidget::RebuildWidget()
{
	if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if(!WidgetTree->RootWidget)
	{
		auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root; Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		DevelopmentButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCity","Development","Развитие города"));
		DevelopmentButton->OnClicked.AddDynamic(this,&ThisClass::ToggleClicked);
		auto* DevelopmentSlot=Root->AddChildToCanvas(DevelopmentButton); DevelopmentSlot->SetPosition({16,115}); DevelopmentSlot->SetAutoSize(true);
		ModalBlocker=WidgetTree->ConstructWidget<UButton>();
		FButtonStyle BlockingStyle=ModalBlocker->GetStyle();
		for(auto* Brush:{&BlockingStyle.Normal,&BlockingStyle.Hovered,&BlockingStyle.Pressed})
		{Brush->DrawAs=ESlateBrushDrawType::Image; Brush->SetResourceObject(nullptr); Brush->TintColor=FLinearColor(0,0,0,.35f);}
		ModalBlocker->SetStyle(BlockingStyle); ModalBlocker->OnClicked.AddDynamic(this,&ThisClass::CloseClicked);
		auto* BlockingSlot=Root->AddChildToCanvas(ModalBlocker); BlockingSlot->SetAnchors(FAnchors(0,0,1,1)); BlockingSlot->SetOffsets(FMargin(0));
		auto* Scale=WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit);
		auto* ModalSlot=Root->AddChildToCanvas(Scale); ModalSlot->SetAnchors(FAnchors(.025f,.025f,.975f,.975f)); ModalSlot->SetOffsets(FMargin(0));
		auto* DesignSize=WidgetTree->ConstructWidget<USizeBox>(); DesignSize->SetWidthOverride(1448); DesignSize->SetHeightOverride(1086); Scale->AddChild(DesignSize);
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
bool UDYKCityWorkWidget::BlocksWorldInput() const {return State.IsModal() || DYKCityPointer::HUDUnderPointer(this);}
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
	if(State.IsModal()) return;
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
	if(Revision!=Save->GetRevision() && State.IsModal()) RebuildModal();
	if(LiveStatus)
	{
		const auto* Current=Save->GetProfileSnapshot();
		const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
		if(Current && !LiveResearch.IsNone())
		{
			const auto* W=Current->CityEconomy.ResearchQueue.FindByPredicate([this](const auto& Row){return Row.ResearchId==LiveResearch;});
			if(W) LiveStatus->SetText(CityText(W->StartedAtUnix>0 ? TEXT("Осталось: ")+CityDuration(W->CompletesAtUnix-Now) : TEXT("Ожидает исследовательскую ячейку")));
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
	++RenderGeneration;
	Actions.Reset(); LiveStatus=nullptr; LiveResearch=NAME_None; DesignCanvas->ClearChildren();
	ModalRoot->SetVisibility(State.IsModal() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ModalBlocker->SetVisibility(State.IsModal() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if(!State.IsModal()) return;
	const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P=Save ? Save->GetProfileSnapshot() : nullptr; if(!P) return;
	const auto* D=DYKCityCatalog::Find(FDYKBuildingId(State.Building).ToContentId());
	auto Title=State.Window==EDYKCityWindow::Construction ? NSLOCTEXT("DYKCity","Construction","Развитие города") : CityText(D ? D->DisplayName : TEXT("Город"));
	const bool Production=State.Window==EDYKCityWindow::Interior && D && D->Category==EDYKCityBuildingCategory::Production;
	const bool Detail=State.Window==EDYKCityWindow::Detail;
	FName Art=DYKCityBuildingUI::Art(State.Building,Detail);
	if(Detail)
	{
		const auto Items=DYKCityBuildingUI::Items(State.Building,State.LibraryTab);
		if(Items.IsValidIndex(State.SelectedItem)) Title=CityText(Items[State.SelectedItem].Name);
		if(DYKCityBuildingUI::Interior(State.Building)!=EDYKCityInterior::Barracks) Art=TEXT("AllTrapHUD");
	}
	DYKCityHUDArt::Frame(WidgetTree,DesignCanvas,Art,Title,CityText(Message.IsEmpty() ? TEXT("Выберите действие") : Message),!Production && !Detail,!Detail);
	for(TActorIterator<ADYKCityBuildingActor> It(GetWorld());It;++It) It->SetHovered(false);
	ActionButton(EDYKCityUIAction::Close,0,FText::GetEmpty(),{1240,76},{110,110},NAME_None,true,true)->SetToolTipText(NSLOCTEXT("DYKCity","Close","Закрыть"));
	if(State.Window!=EDYKCityWindow::Construction && State.Window!=EDYKCityWindow::Building)
		ActionButton(EDYKCityUIAction::Back,0,NSLOCTEXT("DYKCity","Back","Назад"),{235,215},{180,48});
	switch(State.Window)
	{
	case EDYKCityWindow::Construction: BuildConstruction(*P); break;
	case EDYKCityWindow::Building: BuildBuilding(*P); break;
	case EDYKCityWindow::Interior: if(Production) BuildProduction(*P); else BuildInterior(*P); break;
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
		DYKCityHUDArt::Image(WidgetTree,DesignCanvas,TEXT("WorkshopHUD"),FBox2D({576,545},{883,785}),Pos,{308,225});
		// Reuse the author's resource/building emblems; captions and availability are live data.
		DYKCityHUDArt::Paper(WidgetTree,DesignCanvas,Pos+FVector2D(10,10),{288,170});
		FBox2D IconRegion=Regions[int32(State.BuildTab)]; FName IconArt=Icons[int32(State.BuildTab)];
		if(D->Id.GetName()==TEXT("CITY-SAWMILL") || D->Id.GetName()==TEXT("CITY-QUARRY") || D->Id.GetName()==TEXT("CITY-MILL"))
		{IconArt=DYKCityBuildingUI::Art(D->Id.GetName()); IconRegion=FBox2D({245,270},{450,429});}
		DYKCityHUDArt::Image(WidgetTree,DesignCanvas,IconArt,IconRegion,Pos+FVector2D(65,12),{178,112});
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(FString::Printf(TEXT("Замок: %d · %d / %d"),Rule.Levels[0].RequiredCastleLevel,Owned,Limit)),Pos+FVector2D(10,128),{288,44},20);
		CityBrown(WidgetTree,DesignCanvas,Pos+FVector2D(8,176),{292,43});
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(D->DisplayName),Pos+FVector2D(12,177),{284,40},25,true);
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
	if(Kind==EDYKCityInterior::Production || Kind==EDYKCityInterior::Warehouse)
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
		for(int32 I=0;I<3;++I) ActionButton(EDYKCityUIAction::LibraryTab,I,CityText(Tabs[I]),{285.+I*310.,265},{290,55});
	}
	const auto Items=DYKCityBuildingUI::Items(State.Building,State.LibraryTab); State.TurnPage(0,Items.Num());
	const int32 StartY=Kind==EDYKCityInterior::Library ? 335 : 275;
	for(int32 SlotIndex=0;SlotIndex<6;++SlotIndex)
	{
		const int32 ItemIndex=State.Page*6+SlotIndex; if(!Items.IsValidIndex(ItemIndex)) break;
		const auto& Item=Items[ItemIndex]; const FVector2D Pos(240+(SlotIndex%3)*335,StartY+(SlotIndex/3)*240);
		DYKCityHUDArt::Image(WidgetTree,DesignCanvas,TEXT("WorkshopHUD"),FBox2D({576,545},{883,785}),Pos,{310,225});
		DYKCityHUDArt::Image(WidgetTree,DesignCanvas,Item.Art,Item.ImageRegion,Pos+FVector2D(9,10),{292,170});
		CityBrown(WidgetTree,DesignCanvas,Pos+FVector2D(8,180),{294,38});
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Item.Name),Pos+FVector2D(10,179),{290,42},Item.Name.Len()>23 ? 18 : 25,true);
		auto* Click=ActionButton(EDYKCityUIAction::Item,ItemIndex,FText::GetEmpty(),Pos,{310,225},Item.Key,true,true); Click->SetToolTipText(CityText(Item.Description));
	}
	const int32 Pages=FMath::Max(1,(Items.Num()+5)/6);
	ActionButton(EDYKCityUIAction::Previous,0,CityText(TEXT("◀")),{580,820},{68,55},NAME_None,State.Page>0);
	ActionButton(EDYKCityUIAction::Next,0,CityText(TEXT("▶")),{813,820},{68,55},NAME_None,State.Page+1<Pages);
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
	DYKCityHUDArt::Paper(WidgetTree,DesignCanvas,{480,194},{490,244});
	DYKCityHUDArt::Image(WidgetTree,DesignCanvas,Item.Art,Item.ImageRegion,{530,210},{380,225});
	DYKCityHUDArt::Paper(WidgetTree,DesignCanvas,{248,277},{231,83});
	const bool Known=P.CityEconomy.CompletedResearch.Contains(Item.ResearchRoot);
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Known ? TEXT("Изучено") : TEXT("Не изучено")),{250,292},{225,65},27);
	CityBrown(WidgetTree,DesignCanvas,{1028,293},{174,43});
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Item.Name),{1025,295},{180,55},22,true);
	DYKCityHUDArt::Paper(WidgetTree,DesignCanvas,{270,448},{905,61});
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Item.Description),{275,449},{895,60},24);
	CityBrown(WidgetTree,DesignCanvas,{252,538},{942,152});
	FString Stats=Item.bFuture ? TEXT("Пока недоступно") : TEXT("Характеристики пока недоступны");
	if(DYKCityBuildingUI::Interior(State.Building)==EDYKCityInterior::Workshop)
		Stats=TEXT("Автоматическое срабатывание\nНеуязвима · Ограниченный запас зарядов");
	else if(const auto* Root=DYKCityResearch::Find(Item.ResearchRoot))
	{
		if(const auto* Unit=DYKUnitCatalog::Find(Root->Unlock))
		{
			const auto Bonus=DYKCityResearch::Bonus(P,*Unit);
			const auto Personal=DYKCityResearch::Bonus(P,*Unit,EDYKResearchScope::Type);
			Stats=FString::Printf(TEXT("Урон: %d · Здоровье: %d\nФизическая броня: %d · Магическая броня: %d\nРазмещение: %d припасов"),Unit->Base.Attack.DamagePerHit.Value+Bonus.Damage,Unit->Base.MaxHP.Value+Bonus.HP,Unit->Base.PhysicalArmor.Value+Bonus.PhysicalArmor,Unit->Base.MagicalArmor.Value,Unit->Base.PlacementSupplies.Value);
			if(Personal.HP || Personal.PhysicalArmor || Personal.Damage) Stats+=FString::Printf(TEXT("\nПерсональные бонусы: HP +%d · Броня +%d · Урон +%d"),Personal.HP,Personal.PhysicalArmor,Personal.Damage);
		}
		else if(Root->TargetLine!=EDYKUnitLine::None)
			Stats=FString::Printf(TEXT("Бонус линии\nЗдоровье: +%d · Броня: +%d · Урон: +%d"),Root->Bonus.HP,Root->Bonus.PhysicalArmor,Root->Bonus.Damage);
	}
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Stats),{280,543},{885,140},27,true);
	const auto Options=ResearchOptions(P,Item); ResearchChoice=FMath::Clamp(ResearchChoice,0,FMath::Max(0,Options.Num()-1));
	const auto* R=Options.IsEmpty() ? nullptr : DYKCityResearch::Find(Options[ResearchChoice]);
	FString Status=Item.bFuture ? TEXT("Пока недоступно") : TEXT("Доступно");
	const auto* Queued=P.CityEconomy.ResearchQueue.FindByPredicate([&Item](const auto& W){return W.ResearchId==Item.ResearchRoot;});
	if(R) Queued=P.CityEconomy.ResearchQueue.FindByPredicate([R](const auto& W){return W.ResearchId==R->Id;});
	if(Queued) Status=Queued->StartedAtUnix>0 ? TEXT("Осталось: ")+CityDuration(Queued->CompletesAtUnix-FDateTime::UtcNow().ToUnixTimestamp()) : TEXT("Ожидает исследовательскую ячейку");
	DYKCityHUDArt::Paper(WidgetTree,DesignCanvas,{270,697},{925,140});
	LiveStatus=DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Status),{335,695},{780,34},24);
	if(R)
	{
		LiveResearch=R->Id;
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(R->DisplayName),{335,735},{790,40},26);
		ActionButton(EDYKCityUIAction::ResearchPrevious,0,CityText(TEXT("◀")),{250,731},{65,45},NAME_None,ResearchChoice>0);
		ActionButton(EDYKCityUIAction::ResearchNext,0,CityText(TEXT("▶")),{1140,731},{65,45},NAME_None,ResearchChoice+1<Options.Num());
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(CityCosts(*R)+TEXT(" · ")+CityDuration(R->DurationSeconds)),{275,784},{930,47},23);
		const bool Initial=R->Id==Item.ResearchRoot;
		ActionButton(EDYKCityUIAction::Research,0,Initial ? NSLOCTEXT("DYKCity","Learn","Изучить") : NSLOCTEXT("DYKCity","Improve","Улучшить"),{350,881},{360,70},R->Id,DYKCityResearch::CanEnqueue(P,R->Id));
	}
	else ActionButton(EDYKCityUIAction::Research,0,CityText(Item.bFuture ? TEXT("Недоступно") : TEXT("Изучено")),{350,881},{360,70},NAME_None,false);
	ActionButton(EDYKCityUIAction::CancelResearch,0,NSLOCTEXT("DYKCity","CancelResearch","Отменить"),{760,881},{350,70},Queued ? FName(*Queued->WorkId.ToString()) : NAME_None,Queued!=nullptr);
}
void UDYKCityWorkWidget::BuildProduction(const FDYKPlayerProfileSaveData& P)
{
	const auto* S=ScreenInstance(P,State.Instance); if(!S) return;
	for(int32 I=0;I<4;++I)
	{
		const auto R=DYKCityOrders::DEV_PLACEHOLDER()[I];
		CityBrown(WidgetTree,DesignCanvas,{346.+I*253.,510},{108,45});
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,FText::AsNumber(R.Amount),{345.+I*253.,509},{111,46},34,true);
		CityBrown(WidgetTree,DesignCanvas,{310.+I*253.,577},{145,44});
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(CityDuration(R.Seconds)),{311.+I*253.,579},{142,43},24,true);
		auto* Button=ActionButton(EDYKCityUIAction::Order,I,FText::GetEmpty(),{246.+I*253.,638},{220,78},NAME_None,S->Orders.Num()<DYKCityOrders::QueueCapacity,true);
		Button->SetToolTipText(CityText(FString::Printf(TEXT("Заказать %lld · %s"),R.Amount,*CityDuration(R.Seconds))));
		if(State.Building==TEXT("CITY-MILL"))
		{
			CityBrown(WidgetTree,DesignCanvas,{246.+I*253.,443},{215,42});
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,NSLOCTEXT("DYKCity","Food","Еда"),{251.+I*253.,447},{205,37},28,true);
		}
	}
	CityBrown(WidgetTree,DesignCanvas,{700,785},{530,72});
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(FString::Printf(TEXT("Очередь заказов: %d / %d"),S->Orders.Num(),DYKCityOrders::QueueCapacity)),{710,797},{510,50},28,true);
	const auto V=DYKCityEconomy::View(P,State.Instance,FDateTime::UtcNow().ToUnixTimestamp());
	FString Progress=FString::Printf(TEXT("В здании: %lld / %lld"),V.Stored,V.Capacity);
	if(!S->Orders.IsEmpty())
	{
		const auto& O=S->Orders[0];
		Progress+=O.StartedAtUnix==0 ? TEXT(" · Ожидает места в накопителе") : O.CompletesAtUnix>FDateTime::UtcNow().ToUnixTimestamp()
			? TEXT(" · Готово через ")+CityDuration(O.CompletesAtUnix-FDateTime::UtcNow().ToUnixTimestamp()) : TEXT(" · Результат ожидает места в накопителе");
	}
	DYKCityHUDArt::Paper(WidgetTree,DesignCanvas,{235,737},{995,37});
	LiveStatus=DYKCityHUDArt::Label(WidgetTree,DesignCanvas,CityText(Progress),{245,739},{975,38},23);
	ActionButton(EDYKCityUIAction::Collect,0,CityText(FString::Printf(TEXT("Собрать: %lld"),V.Collectable)),{245,862},{400,42},NAME_None,V.Collectable>0);
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
	case EDYKCityUIAction::Visit: if(S && !P->Buildings[S->BuildingIndex].bUnderConstruction) State.Visit(); break;
	case EDYKCityUIAction::Move: if(City->SelectBuilding(State.Instance) && City->BeginLayoutEdit()) State.Close(); break;
	case EDYKCityUIAction::Rotate:
		if(S) {const auto B=P->Buildings[S->BuildingIndex]; if(!DYKCityTerritory::Move(*Save,FGuid::NewGuid(),State.Instance,B.CellOrigin,(B.RotationQuarterTurns+1)%4,DYKCityTerritory::DEV_PLACEHOLDER())) Message=TEXT("Для поворота недостаточно места.");} break;
	case EDYKCityUIAction::Upgrade:
		if(S) {const auto B=P->Buildings[S->BuildingIndex]; FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=State.Instance; R.BuildingId=FDYKBuildingId(State.Building); R.Kind=EDYKCityBuildingWorkKind::Upgrade; R.ExpectedLevel=B.Level; if(!DYKCityWork::Enqueue(*Save,R,Now)) Message=TEXT("Улучшение недоступно или не удалось сохранить.");} break;
	case EDYKCityUIAction::CancelWork:
		if(const auto* W=P->CityEconomy.Work.FindByPredicate([this](const auto& Work){return Work.InstanceId==State.Instance;})) {const auto WorkId=W->WorkId; if(!DYKCityWork::Cancel(*Save,FGuid::NewGuid(),WorkId,Now)) Message=TEXT("Не удалось отменить работу.");} break;
	case EDYKCityUIAction::Collect:
		if(!PendingCollect.IsValid()) PendingCollect=FGuid::NewGuid();
		if(DYKCityEconomy::Collect(*Save,PendingCollect,State.Instance,Now)) PendingCollect.Invalidate(); else Message=TEXT("Сбор недоступен или не удалось сохранить."); break;
	case EDYKCityUIAction::Order: if(!DYKCityOrders::Enqueue(*Save,FGuid::NewGuid(),State.Instance,Index,Now)) Message=TEXT("Очередь заполнена или не удалось сохранить заказ."); break;
	case EDYKCityUIAction::Item:
		if(State.Window==EDYKCityWindow::Interior && DYKCityBuildingUI::Items(State.Building,State.LibraryTab).IsValidIndex(Index))
		{State.SelectedItem=Index; State.Window=EDYKCityWindow::Detail; ResearchChoice=0;} break;
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
