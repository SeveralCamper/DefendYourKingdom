#pragma once
#include "CoreMinimal.h"
class UStaticMesh;
class UStaticMeshComponent;
class AActor;
namespace DYKVisuals
{
	UStaticMesh* Mesh(const TCHAR* Group, const TCHAR* Name);
	void Unit(UStaticMeshComponent* Component, const TCHAR* Name, float Scale);
	float Building(AActor* Owner, UStaticMeshComponent* Body, FName Type);
}
