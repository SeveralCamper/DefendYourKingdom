#include "DYKHeroAbilityWidget.h"
#include "DYKScreenWidget.h"
#include "../Battle/DYKDefenderActor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

TSharedRef<SWidget> UDYKHeroAbilityWidget::RebuildWidget()
{
	if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
	if(!WidgetTree->RootWidget)
	{
		Button=WidgetTree->ConstructWidget<UButton>();
		Button->AddChild(DYKUI::Text(WidgetTree,NSLOCTEXT("DYKHeroes","Hammer","Удар молотом"),18));
		WidgetTree->RootWidget=Button; Button->OnClicked.AddDynamic(this,&ThisClass::Activate);
	}
	return Super::RebuildWidget();
}
void UDYKHeroAbilityWidget::Configure(ADYKDefenderActor* Hero) {Unit=Hero; Refresh();}
void UDYKHeroAbilityWidget::Refresh()
{
	if(!Button) return;
	const bool Ready=Unit.IsValid() && Unit->CanActivateAbility();
	Button->SetVisibility(Ready ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button->SetIsEnabled(Ready);
}
void UDYKHeroAbilityWidget::Activate() {if(Unit.IsValid()) Unit->ActivateAbility(); Refresh();}
void UDYKHeroAbilityWidget::NativeConstruct()
{
	Super::NativeConstruct(); GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Refresh,.1f,true); Refresh();
}
void UDYKHeroAbilityWidget::NativeDestruct()
{
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Timer); Super::NativeDestruct();
}
