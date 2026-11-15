#pragma once

#include "../Saving/DYKSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
// Narrow storage boundary used to force real IO failures and isolated reloads.
struct FDYKMissionRewardTestAccess
{
	static void Open(UDYKSaveSubsystem& Save, const FString& Prefix) { Save.SlotPrefix = Prefix; Save.LoadOrCreateProfile(); }
	static void Prefix(UDYKSaveSubsystem& Save, const FString& Prefix) { Save.SlotPrefix = Prefix; }
};
#endif
