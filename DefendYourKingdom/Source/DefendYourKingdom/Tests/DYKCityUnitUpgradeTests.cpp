#include "DYKCityHUDTestProfile.h"
#include "../Meta/DYKCityWorkWidget.h"
#include "../Core/Content/DYKUnitCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKUnitUpgradeDetailTest,"DYK.CityHUD.UnitUpgrade.LiveStatsProgressAndHoverCosts",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDYKUnitUpgradeDetailTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Game(NewObject<UGameInstance>(GEngine));
	Game->Init();
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);
	World->SetGameInstance(Game.Get());
	struct FCleanup {UWorld* World; UGameInstance* Game; ~FCleanup(){World->DestroyWorld(false);Game->Shutdown();}} Cleanup{World,Game.Get()};
	FURL URL; URL.AddOption(TEXT("game=/Script/DefendYourKingdom.DYKCityGameMode"));
	if(!World->SetGameMode(URL)) return false;
	auto* Save=Game->GetSubsystem<UDYKSaveSubsystem>();
	const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
	const FName Guard(TEXT("RESEARCH-INFANTRY-T2"));
	if(!TestTrue(TEXT("Seed barracks and eligible warehouse"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Now](auto& Profile)
	{
		if(!DYKCityHUDTest::Seed(Profile,Now-60) || !DYKCityHUDTest::AddOwned(Profile,TEXT("CITY-WAREHOUSE"),Now-60)) return false;
		Profile.Buildings.Last().Level=4;
		for(auto& Resource:Profile.Resources)
			Resource.Amount=FMath::Min<int64>(1800,DYKCityEconomy::Capacity(Profile,FDYKResourceId(Resource.ResourceId.Name)));
		Profile.Resources.Add({FDYKResourceId(TEXT("RES-MEDALS")).ToContentId(),70});
		return true;
	}))) return false;
	if(!TestTrue(TEXT("Unlock the second infantry tier"),DYKCityResearch::Enqueue(*Save,FGuid::NewGuid(),Guard,Now-60)
		&& DYKCityWork::Update(*Save,Now))) return false;

	TStrongObjectPtr<UDYKCityWorkWidget> UI(NewObject<UDYKCityWorkWidget>(World));
	UI->Initialize();
	const auto Slate=UI->TakeWidget();
	UI->OpenBuilding(DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-BARRACKS")),TEXT("CITY-BARRACKS"));
	UI->Dispatch(EDYKCityUIAction::Visit);
	const auto SelectUnit=[&](int32 Index)
	{
		if(UI->GetWindowState().Window==EDYKCityWindow::Detail) UI->Dispatch(EDYKCityUIAction::Back);
		UI->Dispatch(EDYKCityUIAction::Item,Index);
	};
	const auto TextAt=[&](FVector2D Position)
	{
		for(const auto* Child:UI->DesignCanvas->GetAllChildren())
			if(const auto* CanvasSlot=Cast<UCanvasPanelSlot>(Child->Slot);CanvasSlot && CanvasSlot->GetPosition()==Position)
				if(const auto* Panel=Cast<UPanelWidget>(Child);Panel && Panel->GetChildrenCount()==1)
					if(const auto* Text=Cast<UTextBlock>(Panel->GetChildAt(0))) return Text->GetText().ToString();
		return FString();
	};
	const auto VisibleCost=[&]()
	{
		FString Text;
		for(auto* Child:UI->DesignCanvas->GetAllChildren())
			if(const auto* Panel=Cast<UCanvasPanel>(Child);Panel && Panel->GetVisibility()==ESlateVisibility::HitTestInvisible)
				for(auto* Cost:Panel->GetAllChildren())
					if(const auto* Fit=Cast<UPanelWidget>(Cost);Fit && Fit->GetChildrenCount()==1)
						if(const auto* Price=Cast<UTextBlock>(Fit->GetChildAt(0))) Text+=Price->GetText().ToString()+TEXT(" ");
		return Text;
	};
	const auto FooterArt=[&]()
	{
		int32 Pieces=0;
		for(const auto* Child:UI->DesignCanvas->GetAllChildren())
			if(const auto* Art=Cast<UImage>(Child);Art && Art->GetVisibility()==ESlateVisibility::HitTestInvisible)
				if(const auto* Texture=Art->GetBrush().GetResourceObject();Texture && Texture->GetFName()==TEXT("UnitUpgradeBase")) ++Pieces;
		return Pieces;
	};
	SelectUnit(0);
	const auto* Peasant=DYKUnitCatalog::Find(FDYKUnitTypeId(TEXT("DYK-PEASANT")).ToContentId());
	TestEqual(TEXT("Base attack comes from the peasant definition"),TextAt({565,538}),FString::FromInt(Peasant->Base.Attack.DamagePerHit.Value));
	TestEqual(TEXT("Base HP comes from the peasant definition"),TextAt({565,631}),FString::FromInt(Peasant->Base.MaxHP.Value));
	TestTrue(TEXT("Description and rank caption areas are empty"),TextAt({662,244}).IsEmpty() && TextAt({648,418}).IsEmpty());
	SelectUnit(1);
	const auto* Damage=UI->FindAction(EDYKCityUIAction::Research,0);
	const auto* Armor=UI->FindAction(EDYKCityUIAction::Research,2);
	if(!TestNotNull(TEXT("Independent attack research"),Damage) || !TestNotNull(TEXT("Independent armor research"),Armor))return false;
	TestTrue(TEXT("Armor uses the same button dimensions as attack and health"),
		CastChecked<UCanvasPanelSlot>(Damage->Slot)->GetSize()==CastChecked<UCanvasPanelSlot>(Armor->Slot)->GetSize()
		&& CastChecked<UCanvasPanelSlot>(Armor->Slot)->GetSize()==CastChecked<UCanvasPanelSlot>(UI->FindAction(EDYKCityUIAction::Research,1)->Slot)->GetSize());
	TestEqual(TEXT("Attack uses the selected unit's research"),Damage->Key,FName(TEXT("RESEARCH-INFANTRY-T2-DAMAGE-1")));
	TestEqual(TEXT("Armor uses a different research"),Armor->Key,FName(TEXT("RESEARCH-INFANTRY-T2-ARMOR-1")));
	TestTrue(TEXT("Prices start hidden"),VisibleCost().IsEmpty());
	TestEqual(TEXT("The original footer stays on the full board"),FooterArt(),1);
	const FPointerEvent Pointer(0,FVector2D(10),FVector2D(10),TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());
	const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(264,62),FSlateLayoutTransform());
	for(auto* Button:{UI->FindAction(EDYKCityUIAction::Research,0),UI->FindAction(EDYKCityUIAction::Research,2)})
	{
		const auto Native=Button->TakeWidget();
		Native->OnMouseEnter(Geometry,Pointer);
		const auto* Research=DYKCityResearch::Find(Button->Key);
		TestTrue(TEXT("Hovered button shows the exact research price"),
			VisibleCost().Contains(FString::Printf(TEXT("/ %lld"),Research->Costs[0].Amount)));
		TestEqual(TEXT("Hover preserves the same panel artwork"),FooterArt(),1);
		Native->OnMouseLeave(Pointer);
		TestTrue(TEXT("Leaving hides the price panel"),VisibleCost().IsEmpty());
	}
	const FName DamageKey=Damage->Key;
	const FString Preview=TextAt({656,538});
	const auto SetMedals=[&](int64 Balance)
	{
		return Save->ApplyProfileCommand(FGuid::NewGuid(),[Balance](auto& Profile)
		{
			auto* Medals=Profile.Resources.FindByPredicate([](const auto& Resource){return Resource.ResourceId.Name==TEXT("RES-MEDALS");});
			if(!Medals) return false;
			Medals->Amount=Balance;
			return true;
		});
	};
	if(!TestTrue(TEXT("Leave the player one medal short"),SetMedals(4))) return false;
	SelectUnit(1);
	auto* Unaffordable=UI->FindAction(EDYKCityUIAction::Research,0);
	const auto UnaffordableSlate=Unaffordable->TakeWidget();
	const FKeyEvent Accept(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
	const int64 BeforeRejectedAction=Save->GetRevision();
	TestFalse(TEXT("An unaffordable upgrade does not accept activation"),UnaffordableSlate->OnKeyDown(Geometry,Accept).IsEventHandled());
	Unaffordable->Invoke();
	TestEqual(TEXT("An unavailable HUD action does not write a save"),Save->GetRevision(),BeforeRejectedAction);
	UnaffordableSlate->OnMouseEnter(Geometry,Pointer);
	TestTrue(TEXT("An unavailable price still shows what is missing on hover"),VisibleCost().Contains(TEXT("4 / 5")));
	UnaffordableSlate->OnMouseLeave(Pointer);
	if(!TestTrue(TEXT("Restore the price of the next upgrade"),SetMedals(5))) return false;
	SelectUnit(1);
	UI->FindAction(EDYKCityUIAction::Research,0)->Invoke();
	TestTrue(TEXT("Research queues on its own row"),Save->GetProfileSnapshot()->CityEconomy.ResearchQueue.ContainsByPredicate(
		[DamageKey](const auto& Work){return Work.ResearchId==DamageKey;}));
	if(auto* Cancel=UI->FindAction(EDYKCityUIAction::CancelResearch,0);TestNotNull(TEXT("Queued attack can be cancelled independently"),Cancel))
	{
		const auto* CancelSlot=CastChecked<UCanvasPanelSlot>(Cancel->Slot);
		TestTrue(TEXT("Cancel is taller than upgrade controls and sits below the resources"),
			CancelSlot->GetPosition().Y>1000. && CancelSlot->GetSize().Y>62.);
		const auto Native=Cancel->TakeWidget();
		Native->OnMouseEnter(Geometry,Pointer);
		TestTrue(TEXT("Cancel does not reveal any prices"),VisibleCost().IsEmpty());
		TestEqual(TEXT("Cancel hover leaves the resource panel visible"),FooterArt(),1);
		Native->OnMouseLeave(Pointer);
	}
	TestEqual(TEXT("Queued stage has its own marker"),TextAt({1054,532}),FString(TEXT("…")));
	TestTrue(TEXT("An active improvement shows a running timer next to cancel"),TextAt({717,1027}).StartsWith(TEXT("Осталось:")));
	if(!TestTrue(TEXT("Move the paid research close to its deadline"),Save->ApplyProfileCommand(FGuid::NewGuid(),[DamageKey](auto& Profile)
	{
		auto* Job=Profile.CityEconomy.ResearchQueue.FindByPredicate([DamageKey](const auto& Work){return Work.ResearchId==DamageKey;});
		if(!Job || Job->StartedAtUnix<=0) return false;
		Job->CompletesAtUnix=FDateTime::UtcNow().ToUnixTimestamp()+5;
		return Job->CompletesAtUnix>Job->StartedAtUnix;
	}))) return false;
	UI->Refresh();
	const FString Timer=TextAt({717,1027});
	TestTrue(TEXT("The open detail HUD refreshes the remaining seconds"),Timer.StartsWith(TEXT("Осталось: ")) &&
		FCString::Atoi(*Timer.RightChop(FString(TEXT("Осталось: ")).Len()))<=5);
	if(!TestTrue(TEXT("Complete the queued attack"),DYKCityWork::Update(*Save,FDateTime::UtcNow().ToUnixTimestamp()+60)))return false;
	SelectUnit(1);
	TestFalse(TEXT("Next stat was actually shown"),Preview.IsEmpty());
	TestEqual(TEXT("Completed personal upgrade updates the current attack"),TextAt({565,538}),Preview);
	TestTrue(TEXT("The completed marker is distinct from a queued stage"),TextAt({1054,532}).IsEmpty());
	const auto* CompletedRow=UI->FindAction(EDYKCityUIAction::Research,0);
	if(TestNotNull(TEXT("The completed row has no research command"),CompletedRow))
		TestFalse(TEXT("A completed single-stage row cannot be queued again"),CompletedRow->GetIsEnabled());
	UI->ReleaseSlateResources(true);
	return true;
}
#endif
