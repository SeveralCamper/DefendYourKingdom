// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKSelectionPlayerController.h"
#include "../../Meta/DYKCityGameMode.h"
#include "../../Meta/DYKCityBuildingActor.h"
#include "../../Meta/DYKCityEditorWidget.h"
#include "Engine/World.h"

void ADYKSelectionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ADYKSelectionPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindAction(TEXT("DYK_SelectWorld"), IE_Pressed, this, &ThisClass::HandleWorldSelect);
	}
}

FDYKWorldSelectionChanged& ADYKSelectionPlayerController::OnWorldSelectionChanged()
{
	return WorldSelectionChanged;
}

FDYKUICommandRequested& ADYKSelectionPlayerController::OnUICommandRequested()
{
	return UICommandRequested;
}

void ADYKSelectionPlayerController::RequestUICommand(const FName CommandId)
{
	if (!CommandId.IsNone())
	{
		UICommandRequested.Broadcast(CommandId);
	}
}

AActor* ADYKSelectionPlayerController::GetSelectedActor() const
{
	return SelectedActor.Get();
}

void ADYKSelectionPlayerController::HandleWorldSelect()
{
	// GameAndUI can forward unhandled clicks on panel/text backgrounds to the game.
	for (TObjectIterator<UDYKCityEditorWidget> It; It; ++It)
		if (It->GetWorld()==GetWorld() && It->IsInViewport() && It->BlocksWorldInput()) return;
	if (auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>(); City && City->GetLayoutEditor().IsActive())
	{
		FHitResult BuildingHit;
		if (GetHitResultUnderCursor(ECC_Visibility,false,BuildingHit))
			if (const auto* Building=Cast<ADYKCityBuildingActor>(BuildingHit.GetActor()); Building && Building->InstanceId!=City->GetSelectedBuilding())
			{
				if (City->SelectBuilding(Building->InstanceId))
				{
					SelectedActor=const_cast<ADYKCityBuildingActor*>(Building);
					FDYKWorldSelection Selection;
					Selection.TargetKind=EDYKSelectionTargetKind::CityBuilding;
					Selection.TargetActor=SelectedActor;
					Selection.HitResult=BuildingHit;
					WorldSelectionChanged.Broadcast(Selection);
					City->SyncLayoutVisuals();
				}
				return;
			}
		FIntPoint Cell;
		if (DYKCityEditorInput::CursorCell(this,Cell)) City->PlaceSelected(Cell);
		City->SyncLayoutVisuals();
		return;
	}
	FHitResult HitResult;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, HitResult) || !HitResult.bBlockingHit)
	{
		ClearWorldSelection();
		return;
	}

	AActor* HitActor = HitResult.GetActor();
	const IDYKSelectionTarget* SelectionTarget = HitActor ? Cast<IDYKSelectionTarget>(HitActor) : nullptr;
	if (!SelectionTarget || SelectionTarget->GetSelectionTargetKind() == EDYKSelectionTargetKind::None)
	{
		ClearWorldSelection();
		return;
	}

	SelectedActor = HitActor;
	if (const auto* Building=Cast<ADYKCityBuildingActor>(HitActor))
		if (auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>())
		{
			City->SelectBuilding(Building->InstanceId); City->SyncLayoutVisuals();
		}

	FDYKWorldSelection Selection;
	Selection.TargetKind = SelectionTarget->GetSelectionTargetKind();
	Selection.TargetActor = HitActor;
	Selection.HitResult = HitResult;
	WorldSelectionChanged.Broadcast(Selection);
}

void ADYKSelectionPlayerController::ClearWorldSelection()
{
	if (!SelectedActor)
	{
		return;
	}

	SelectedActor = nullptr;
	WorldSelectionChanged.Broadcast(FDYKWorldSelection());
}
