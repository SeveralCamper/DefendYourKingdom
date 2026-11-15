#pragma once

#include "../Core/DYKLocalGameMode.h"
#include "DYKBattleSession.h"
#include "DYKPolygonGameMode.generated.h"

// Technical first-mission blockout; a fresh, transient attempt per loaded world.
UCLASS()
class DEFENDYOURKINGDOM_API ADYKPolygonGameMode : public ADYKLocalGameMode
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FDYKBattleSession& GetBattleSession() { return BattleSession; }
	const FDYKBattleSession& GetBattleSession() const { return BattleSession; }

private:
	void QueueWaveResolution(const FGuid& WaveToken);
	FDYKBattleSession BattleSession;
	FTimerHandle ResolutionTimer;
};
