#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DYKGameInstance.generated.h"

// Session lifetime only. Profile data and time remain owned by their subsystems.
UCLASS()
class DEFENDYOURKINGDOM_API UDYKGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	bool IsLocalSessionReady(FString& OutError) const;
};
