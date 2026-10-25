// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKStrategyCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"

ADYKStrategyCameraPawn::ADYKStrategyCameraPawn()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = InitialArmLength;
	CameraBoom->SetRelativeRotation(FRotator(FixedPitch, FixedYaw, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
	MovementComponent->SetUpdatedComponent(SceneRoot);
	MovementComponent->MaxSpeed = CameraPanSpeed;
	MovementComponent->Acceleration = CameraAcceleration;
	MovementComponent->Deceleration = CameraDeceleration;
}

void ADYKStrategyCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("DYK_CameraForward"), this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("DYK_CameraRight"), this, &ThisClass::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("DYK_CameraZoom"), this, &ThisClass::Zoom);
}

void ADYKStrategyCameraPawn::MoveForward(const float AxisValue)
{
	const FRotator YawRotation(0.0f, FixedYaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), AxisValue);
}

void ADYKStrategyCameraPawn::MoveRight(const float AxisValue)
{
	const FRotator YawRotation(0.0f, FixedYaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), AxisValue);
}

void ADYKStrategyCameraPawn::Zoom(const float AxisValue)
{
	if (!CameraBoom || FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	CameraBoom->TargetArmLength = FMath::Clamp(
		CameraBoom->TargetArmLength - AxisValue * ZoomStep,
		MinimumArmLength,
		MaximumArmLength);
}
