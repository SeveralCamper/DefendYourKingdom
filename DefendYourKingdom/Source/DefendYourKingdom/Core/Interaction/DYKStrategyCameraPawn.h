// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DYKStrategyCameraPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USpringArmComponent;

UCLASS()
class DEFENDYOURKINGDOM_API ADYKStrategyCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ADYKStrategyCameraPawn();
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void MoveForward(float AxisValue);
	void MoveRight(float AxisValue);
	void Zoom(float AxisValue);

	UPROPERTY(VisibleAnywhere, Category="Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category="Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category="Camera")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;

	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="1000.0"))
	float InitialArmLength = 6000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="1000.0"))
	float MinimumArmLength = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="2000.0"))
	float MaximumArmLength = 10000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="0.0"))
	float ZoomStep = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera|Movement", meta=(ClampMin="0.0"))
	float CameraPanSpeed = 2400.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera|Movement", meta=(ClampMin="0.0"))
	float CameraAcceleration = 8000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera|Movement", meta=(ClampMin="0.0"))
	float CameraDeceleration = 10000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float FixedPitch = -55.0f;

	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float FixedYaw = -45.0f;
};
