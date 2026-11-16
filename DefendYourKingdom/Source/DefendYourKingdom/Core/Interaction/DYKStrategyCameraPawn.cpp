// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKStrategyCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "../../Meta/DYKCityGameMode.h"
#include "../../Meta/DYKCityTerritory.h"
#include "Engine/World.h"

void UDYKCameraMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	if (!bBounded || !UpdatedComponent) return;
	const FVector Position = UpdatedComponent->GetComponentLocation();
	const FVector Limited(FMath::Clamp(Position.X,PanBounds.Min.X,PanBounds.Max.X),
		FMath::Clamp(Position.Y,PanBounds.Min.Y,PanBounds.Max.Y),Position.Z);
	if (!Position.Equals(Limited)) UpdatedComponent->SetWorldLocation(Limited,false,nullptr,ETeleportType::TeleportPhysics);
	if ((Limited.X <= PanBounds.Min.X && Velocity.X < 0) || (Limited.X >= PanBounds.Max.X && Velocity.X > 0)) Velocity.X=0;
	if ((Limited.Y <= PanBounds.Min.Y && Velocity.Y < 0) || (Limited.Y >= PanBounds.Max.Y && Velocity.Y > 0)) Velocity.Y=0;
}

void ADYKStrategyCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	if (Cast<ADYKCityGameMode>(GetWorld()->GetAuthGameMode()))
	{
		auto* Movement=CastChecked<UDYKCameraMovement>(MovementComponent);
		Movement->bBounded=true;
		const auto Min=DYKCityTerritory::CellCorner({-10,-10});
		const auto Max=DYKCityTerritory::CellCorner({60,60});
		Movement->PanBounds=FBox2D(FVector2D(Min),FVector2D(Max));
	}
}

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

	MovementComponent = CreateDefaultSubobject<UDYKCameraMovement>(TEXT("MovementComponent"));
	MovementComponent->SetUpdatedComponent(SceneRoot);
	MovementComponent->MaxSpeed = CameraPanSpeed;
	MovementComponent->Acceleration = CameraAcceleration;
	MovementComponent->Deceleration = CameraDeceleration;
	CameraBoom->AddTickPrerequisiteComponent(MovementComponent);
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
