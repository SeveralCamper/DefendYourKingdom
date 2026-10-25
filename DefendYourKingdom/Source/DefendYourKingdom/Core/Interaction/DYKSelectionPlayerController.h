// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DYKSelectionTarget.h"
#include "DYKSelectionPlayerController.generated.h"

struct FDYKWorldSelection
{
	EDYKSelectionTargetKind TargetKind = EDYKSelectionTargetKind::None;
	TWeakObjectPtr<AActor> TargetActor;
	FHitResult HitResult;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDYKWorldSelectionChanged, const FDYKWorldSelection&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDYKUICommandRequested, FName);

UCLASS()
class DEFENDYOURKINGDOM_API ADYKSelectionPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	FDYKWorldSelectionChanged& OnWorldSelectionChanged();
	FDYKUICommandRequested& OnUICommandRequested();

	// UI controls use this route so button commands never masquerade as world selections.
	void RequestUICommand(FName CommandId);

	AActor* GetSelectedActor() const;

private:
	void HandleWorldSelect();
	void ClearWorldSelection();

	UPROPERTY(Transient)
	TObjectPtr<AActor> SelectedActor;

	FDYKWorldSelectionChanged WorldSelectionChanged;
	FDYKUICommandRequested UICommandRequested;
};
