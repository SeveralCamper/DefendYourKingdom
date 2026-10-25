// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKSelectionTargetActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADYKSelectionTargetActor::ADYKSelectionTargetActor()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GetStaticMeshComponent()->SetStaticMesh(CubeMesh.Object);
	}

	GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

EDYKSelectionTargetKind ADYKSelectionTargetActor::GetSelectionTargetKind() const
{
	return SelectionTargetKind;
}
