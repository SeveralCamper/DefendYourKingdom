#include "../Core/DYKGameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTravelRejectionTest, "DYK.M104.TravelRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDYKTravelRejectionTest::RunTest(const FString& Parameters)
{
	// No initialized world/subsystems: every request must fail without starting a load.
	TStrongObjectPtr<UDYKGameInstance> Session(NewObject<UDYKGameInstance>());
	FString Error = TEXT("old error");
	TestFalse(TEXT("Invalid enum is rejected"), Session->RequestLevelTravel(static_cast<EDYKLocalLevel>(255), Error));
	TestTrue(TEXT("Invalid destination reports the reason"), Error.Contains(TEXT("Unknown local destination")));
	TestFalse(TEXT("Invalid destination leaves no pending load"), Session->IsLevelTravelPending());
	TestFalse(TEXT("Valid destination without a game world is rejected"), Session->RequestLevelTravel(EDYKLocalLevel::MissionPolygon, Error));
	TestTrue(TEXT("World rejection replaces the error"), Error.Contains(TEXT("active local game world")));
	TestFalse(TEXT("Rejected request leaves no pending load"), Session->IsLevelTravelPending());
	return true;
}

#endif
