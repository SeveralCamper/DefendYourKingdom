#include "DYKVisuals.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
UStaticMesh* DYKVisuals::Mesh(const TCHAR* Group, const TCHAR* Name)
{
	return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/DefendYourKingdom/Art/%s/SM_%s.SM_%s"),Group,Name,Name));
}

float DYKVisuals::Building(AActor* Owner, UStaticMeshComponent* Body, FName Type)
{
	const bool bCastle = Type == FName(TEXT("CITY-CASTLE"));
	const bool bQuarry = Type == FName(TEXT("CITY-QUARRY"));
	const bool bMill = Type == FName(TEXT("CITY-MILL"));
	const bool bSawmill = Type == FName(TEXT("CITY-SAWMILL"));
	const auto Piece = [Owner](const TCHAR* Group, const TCHAR* Name, FVector Position, FVector Scale)
	{
		auto* Part = NewObject<UStaticMeshComponent>(Owner); Owner->AddInstanceComponent(Part);
		Part->SetupAttachment(Owner->GetRootComponent());
		Part->SetStaticMesh(Mesh(Group,Name));
		Part->SetRelativeLocation(Position); Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->RegisterComponent();
	};
	Body->SetStaticMesh(Mesh(bSawmill ? TEXT("Dungeon") : TEXT("Town"), bSawmill ? TEXT("wood_structure") : bCastle || bQuarry ? TEXT("wall_block") : TEXT("wall_wood_block")));
	Body->SetRelativeScale3D(bCastle ? FVector(4,4,3) : bQuarry ? FVector(2,2,1) : bMill ? FVector(2.8,2.8,4) : FVector(3,3,2.5));
	if (bCastle)
	{
		for (int32 X : {-170,170}) for (int32 Y : {-170,170})
		{
			Piece(TEXT("Town"),TEXT("wall_block"),FVector(X,Y,0),FVector(1.4,1.4,4.5));
			Piece(TEXT("Town"),TEXT("roof_high_point"),FVector(X,Y,450),FVector(1.5,1.5,1.5));
		}
		return 600;
	}
	if (bQuarry)
	{
		Piece(TEXT("Dungeon"),TEXT("rocks"),FVector(-130,20,0),FVector(3,2,3));
		Piece(TEXT("Dungeon"),TEXT("rocks"),FVector(100,-40,0),FVector(2,3,2));
	}
	else Piece(TEXT("Town"),TEXT("roof_high_point"),FVector(0,0,bMill ? 400 : 250),FVector(3,3,1.2));
	if (bMill) Piece(TEXT("Town"),TEXT("windmill"),FVector(160,0,340),FVector(1.1));
	if (bSawmill || bQuarry) Piece(TEXT("Town"),TEXT("cart"),FVector(160,100,0),FVector(1.8));
	return bMill ? 520 : bQuarry ? 150 : 370;
}
void DYKVisuals::Unit(UStaticMeshComponent* Component, const TCHAR* Name, float Scale)
{
	Component->SetStaticMesh(Mesh(TEXT("Dungeon"),Name));
	Component->SetRelativeScale3D(FVector(Scale));
	Component->SetRelativeLocation(FVector(0,0,-60));
	// Kenney's character front is -Y; logical actor forward stays +X.
	Component->SetRelativeRotation(FRotator(0,90,0));
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
