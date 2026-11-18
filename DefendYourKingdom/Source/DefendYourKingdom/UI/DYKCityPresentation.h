#pragma once
#include "Engine/DataAsset.h"
#include "DYKCityPresentation.generated.h"
class UTexture2D;
class UFontFace;

// Presentation only: changing an illustration never changes unlock rules or balance.
USTRUCT(BlueprintType)
struct FDYKCityCardPicture
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Key;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Illustration;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D UVMin = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D UVMax = FVector2D(1,1);
};

UCLASS(BlueprintType)
class DEFENDYOURKINGDOM_API UDYKCityPresentation : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Window;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> CardFrame;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> NewRibbon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> ButtonPlate;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UFontFace> Typeface;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FDYKCityCardPicture> Pictures;
	const FDYKCityCardPicture* Find(FName Key) const
	{return Pictures.FindByPredicate([Key](const auto& Row){return Row.Key==Key;});}
};
