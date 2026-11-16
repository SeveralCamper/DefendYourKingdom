#include "DYKUnitHealthWidget.h"
#include "../Battle/DYKCombatUnit.h"
#include "../Core/Content/DYKUnitDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "TimerManager.h"

TSharedRef<SWidget> UDYKUnitHealthWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		Bar = WidgetTree->ConstructWidget<UProgressBar>(); WidgetTree->RootWidget = Bar;
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}
void UDYKUnitHealthWidget::Configure(ADYKCombatUnit* InUnit) { Unit = InUnit; RefreshHealth(); }
void UDYKUnitHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::RefreshHealth,.2f,true);
	RefreshHealth();
}
void UDYKUnitHealthWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}
void UDYKUnitHealthWidget::RefreshHealth()
{
	if (!Bar || !Unit.IsValid() || !Unit->GetUnitDefinition()) return;
	Bar->SetPercent(float(Unit->GetHP()) / Unit->GetUnitDefinition()->Base.MaxHP.Value);
	Bar->SetFillColorAndOpacity(Unit->GetUnitDefinition()->Side == EDYKUnitSide::Defender ? FLinearColor(.12f,.65f,1.f) : FLinearColor(.85f,.12f,.07f));
}
