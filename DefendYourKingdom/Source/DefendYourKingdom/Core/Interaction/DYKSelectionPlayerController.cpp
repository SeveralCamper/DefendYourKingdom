// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKSelectionPlayerController.h"

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
