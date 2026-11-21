#include "DYKCityDecorationActor.h"
#include "DYKHeroes.h"
#include "../UI/DYKVisuals.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ADYKCityDecorationActor::ADYKCityDecorationActor()
{
	PrimaryActorTick.bCanEverTick=false;
	Roads=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PerimeterRoads"));
	SetRootComponent(Roads);
	Residents=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DecorativeResidents"));
	Residents->SetupAttachment(Roads);
	HeroResidents=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OwnedHeroResidents"));
	HeroResidents->SetupAttachment(Roads);
	for (auto* Component:{Roads.Get(),Residents.Get(),HeroResidents.Get()})
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
	}
	// Already licensed Kenney CC0 meshes from the project's visual library.
	Roads->SetStaticMesh(DYKVisuals::Mesh(TEXT("Town"),TEXT("road")));
	Residents->SetStaticMesh(DYKVisuals::Mesh(TEXT("Dungeon"),TEXT("character_human")));
	HeroResidents->SetStaticMesh(Residents->GetStaticMesh());
	Roads->SetCastShadow(false);
	Roads->SetRemoveSwap();
}

void ADYKCityDecorationActor::BeginPlay()
{
	Super::BeginPlay();
	// The M1 map contains fixed cross-shaped road props. The derived perimeters replace
	// that layout; keep the authored assets intact, but never show stale roads under buildings.
	const auto* OldBridge=DYKVisuals::Mesh(TEXT("Town"),TEXT("planks"));
	for (TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
		if (It->ActorHasTag(TEXT("M110_VISUAL")) && (It->GetStaticMeshComponent()->GetStaticMesh()==Roads->GetStaticMesh()
			|| It->GetStaticMeshComponent()->GetStaticMesh()==OldBridge))
		{
			It->SetActorHiddenInGame(true); It->SetActorEnableCollision(false);
		}
	LastUpdate=GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(Timer,this,&ADYKCityDecorationActor::UpdateResidents,Pedestrians.DEV_PLACEHOLDER.UpdateSeconds,true);
}

void ADYKCityDecorationActor::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(Timer);
	Super::EndPlay(Reason);
}

void ADYKCityDecorationActor::Refresh(const FDYKPlayerProfileSaveData& Profile,const FDYKCityLayoutEditor* Editor)
{
	if (!bRandomInitialized) { Random.Initialize(FMath::Rand()); bRandomInitialized=true; }
	if (!Network.Rebuild(Profile,DYKCityTerritory::DEV_PLACEHOLDER(),Editor))
	{
		Network.Cells.Reset(); Network.OrderedCells.Reset();
	}
	// Preserve existing mesh instances; remove obsolete ones and add only missing segments.
	for (int32 I=RenderedCells.Num()-1;I>=0;--I) if (!Network.Cells.Contains(RenderedCells[I]))
	{
		Roads->RemoveInstance(I); RenderedCells.RemoveAtSwap(I);
	}
	const UStaticMesh* Mesh=Roads->GetStaticMesh();
	if (Mesh)
	{
		const auto Bounds=Mesh->GetBoundingBox();
		const auto Size=Bounds.GetSize();
		const FVector Scale(DYKCityTerritory::CellSize/FMath::Max(1.,Size.X),DYKCityTerritory::CellSize/FMath::Max(1.,Size.Y),.3);
		for (const auto Cell:Network.OrderedCells) if (!RenderedCells.Contains(Cell))
		{
			const FVector Center=DYKCityTerritory::CellCorner(Cell)+FVector(100,100,4);
			Roads->AddInstance(FTransform(FRotator::ZeroRotator,Center-Bounds.GetCenter()*Scale+FVector(0,0,Size.Z*Scale.Z*.5),Scale));
			RenderedCells.Add(Cell);
		}
	}
	Pedestrians.Reconcile(Network,Random);
	HeroPedestrians.DEV_PLACEHOLDER.Population=DYKHeroes::Owns(Profile) ? 1 : 0;
	HeroPedestrians.Reconcile(Network,Random);
	HeroResidents->ClearInstances();
	for(int32 I=0;I<HeroPedestrians.Walkers.Num();++I) HeroResidents->AddInstance(FTransform::Identity);
	Residents->ClearInstances();
	for (int32 I=0;I<Pedestrians.Walkers.Num();++I) Residents->AddInstance(FTransform::Identity);
	UpdateResidents();
}

void ADYKCityDecorationActor::UpdateResidents()
{
	const double Now=GetWorld()->GetTimeSeconds();
	HeroPedestrians.Advance(Network,static_cast<float>(Now-LastUpdate),Random);
	Pedestrians.Advance(Network,static_cast<float>(Now-LastUpdate),Random); LastUpdate=Now;
	const UStaticMesh* Mesh=Residents->GetStaticMesh();
	const double Bottom=Mesh ? Mesh->GetBoundingBox().Min.Z : 0;
	for (int32 I=0;I<Pedestrians.Walkers.Num();++I)
	{
		const auto& W=Pedestrians.Walkers[I]; const auto P=W.Position();
		const bool Talking=W.ConversationLeft>0;
		const FVector Facing(W.Next.X-W.Cell.X,W.Next.Y-W.Cell.Y,0);
		const double Yaw=Facing.Rotation().Yaw+90+(Talking ? 180+FMath::Sin(Now*5)*12 : 0);
		const double Bob=Talking ? FMath::Sin(Now*7)*3 : FMath::Abs(FMath::Sin(Now*8))*4;
		const FVector Location((P.X-15)*200+100,(P.Y-15)*200+100,12-Bottom*1.8+Bob);
		Residents->UpdateInstanceTransform(I,FTransform(FRotator(0,Yaw,0),Location,FVector(1.8)),false,false,true);
	}
	Residents->MarkRenderStateDirty();
	for(int32 I=0;I<HeroPedestrians.Walkers.Num();++I)
	{
		const auto& W=HeroPedestrians.Walkers[I]; const auto P=W.Position();
		const FVector Facing(W.Next.X-W.Cell.X,W.Next.Y-W.Cell.Y,0);
		HeroResidents->UpdateInstanceTransform(I,FTransform(FRotator(0,Facing.Rotation().Yaw+90,0),
			FVector((P.X-15)*200+100,(P.Y-15)*200+100,12-Bottom*2.2),FVector(2.2)),false,false,true);
	}
	HeroResidents->MarkRenderStateDirty();
}
