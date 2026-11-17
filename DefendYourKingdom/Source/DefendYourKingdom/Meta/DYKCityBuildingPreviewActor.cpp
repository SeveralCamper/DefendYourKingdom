#include "DYKCityBuildingPreviewActor.h"
#include "../UI/DYKVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

ADYKCityBuildingPreviewActor::ADYKCityBuildingPreviewActor()
{
	PrimaryActorTick.bCanEverTick=false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PreviewPosition")));
	Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ThumbnailCapture")); Capture->SetupAttachment(GetRootComponent());
	Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
	Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->ProjectionType=ECameraProjectionMode::Orthographic;
	Capture->ShowFlags.SetLighting(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetPostProcessing(false);
	Capture->ShowFlags.SetMotionBlur(false);
	SetActorEnableCollision(false);
}

bool ADYKCityBuildingPreviewActor::Configure(FName BuildingType,bool Ghost)
{
	if (bConfigured) return false;
	bConfigured=true; bGhost=Ghost;
	DYKVisuals::Building(this,Body,BuildingType);
	auto* Master=Ghost ? LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/DefendYourKingdom/Materials/M_CityPlacementPreview.M_CityPlacementPreview")) : nullptr;
	if (Ghost && !Master) { SetActorHiddenInGame(true); return false; }
	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (auto* Part:Parts)
	{
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		Part->SetVisibleInSceneCaptureOnly(!Ghost);
		if (!Ghost) continue;
		for (int32 Slot=0;Slot<Part->GetNumMaterials();++Slot)
		{
			TArray<UTexture*> Textures;
			if (const auto* Original=Part->GetMaterial(Slot))
				Original->GetUsedTextures(Textures,EMaterialQualityLevel::High,false,ERHIFeatureLevel::SM5,false);
			auto* Material=UMaterialInstanceDynamic::Create(Master,this);
			for (auto* Texture:Textures) if (Cast<UTexture2D>(Texture))
			{ Material->SetTextureParameterValue(TEXT("PaletteTexture"),Texture); break; }
			Material->SetScalarParameterValue(TEXT("Opacity"),.7f);
			Part->SetMaterial(Slot,Material);
		}
	}
	return Body->GetStaticMesh()!=nullptr;
}

FBox ADYKCityBuildingPreviewActor::GetModelBounds() const
{
	FBox Box(ForceInit);
	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (const auto* Part:Parts) if (Part->GetStaticMesh()) Box+=Part->Bounds.GetBox();
	return Box;
}

UTextureRenderTarget2D* ADYKCityBuildingPreviewActor::CaptureThumbnail(UObject* TextureOwner)
{
	if (!bConfigured || bGhost || Capture->TextureTarget) return Capture->TextureTarget;
	auto* Target=NewObject<UTextureRenderTarget2D>(TextureOwner);
	Target->ClearColor=FLinearColor(.15f,.18f,.19f,1.f);
	Target->RenderTargetFormat=ETextureRenderTargetFormat::RTF_RGBA8;
	Target->InitAutoFormat(192,192); Target->UpdateResourceImmediate(true);
	Capture->TextureTarget=Target;
	const auto Box=GetModelBounds();
	const FVector Center=Box.GetCenter();
	const double Diameter=FMath::Max(200.,Box.GetSize().GetMax());
	const FVector Offset=FVector(1.,-1.4,1.15).GetSafeNormal()*Diameter*3.;
	Capture->SetWorldLocationAndRotation(Center+Offset,(-Offset).Rotation());
	Capture->OrthoWidth=Diameter*1.6;
	Capture->ShowOnlyActorComponents(this);
	Capture->CaptureScene();
	return Target;
}
