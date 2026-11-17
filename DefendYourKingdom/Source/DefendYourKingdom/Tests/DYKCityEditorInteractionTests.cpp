#include "../Meta/DYKCityEditorWidget.h"
#include "../Meta/DYKCityGameMode.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Saving/DYKSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

constexpr auto EditorInteractionFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKEditorPagingTest,"DYK.M203.EditorThemeAndPaging",EditorInteractionFlags)
bool FDYKEditorPagingTest::RunTest(const FString& Parameters)
{
	const auto* Skin=GetDefault<UDYKCityEditorWidget>();
	TestTrue(TEXT("Native editor inherits the existing parchment texture"),Skin->PanelTexture==LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/UI/panel_beige.panel_beige")) && Skin->PanelTexture!=nullptr);
	TestTrue(TEXT("Native buttons inherit the existing textured button"),Skin->ButtonTexture==LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/UI/buttonLong_beige.buttonLong_beige")) && Skin->ButtonTexture!=nullptr);
	FDYKCityBufferPage Page;
	TestEqual(TEXT("Ten entries fit without navigation"),FDYKCityBufferPage::Capacity,10);
	Page.Move(1,10); TestEqual(TEXT("No empty extra page for ten buildings"),Page.Start(10),0);
	Page.Move(1,11); TestEqual(TEXT("Eleventh building appears on next page"),Page.Start(11),10);
	Page.Move(1,11); TestEqual(TEXT("Cannot advance beyond last page"),Page.Start(11),10);
	TestEqual(TEXT("Removing the final page returns to populated page"),Page.Start(10),0);
	Page.Move(-1,10); TestEqual(TEXT("Cannot navigate before first page"),Page.Start(10),0);
	Page.Move(2,25); TestEqual(TEXT("Partial third page retains correct GUID slice"),Page.Start(25),20);
	TestEqual(TEXT("Empty storage resets navigation"),Page.Start(0),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKEditorReleaseTest,"DYK.M203.EditorReleasesCursor",EditorInteractionFlags)
bool FDYKEditorReleaseTest::RunTest(const FString& Parameters)
{
	// Test-only world: no level travel, play loop, viewport, rendering or screenshots.
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine));
	GI->Init();
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	World->SetGameInstance(GI.Get());
	struct FCleanup { UWorld* W; UGameInstance* G; ~FCleanup(){ W->DestroyWorld(false); G->Shutdown(); } } Cleanup{World,GI.Get()};
	auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();
	if (!TestNotNull(TEXT("Actual profile subsystem"),Save)) return false;
	if (!TestTrue(TEXT("Initialize city in isolated automation profile"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P)
	{
		return DYKCityEconomy::Initialize(P,1794700000) && DYKCityTerritory::Initialize(P,DYKCityTerritory::DEV_PLACEHOLDER());
	}))) return false;
	const auto Before=*Save->GetProfileSnapshot(); const auto Revision=Save->GetRevision();
	auto* City=World->SpawnActor<ADYKCityGameMode>();
	if (!TestNotNull(TEXT("Actual city mode in test world"),City)) return false;
	const auto Saw=Before.CityEconomy.Instances[1].InstanceId;
	const auto Quarry=Before.CityEconomy.Instances[2].InstanceId;
	TestTrue(TEXT("Select and explicitly begin moving"),City->SelectBuilding(Saw) && City->BeginLayoutEdit() && City->IsPlacingBuilding());
	TestTrue(TEXT("Store actual selected building"),City->StoreSelected());
	TestFalse(TEXT("Store frees selection"),City->GetSelectedBuilding().IsValid());
	TestFalse(TEXT("Store detaches cursor"),City->IsPlacingBuilding());
	TestTrue(TEXT("Select a different placed building"),City->SelectBuilding(Quarry));
	TestFalse(TEXT("Inspecting placed building does not attach it to cursor"),City->IsPlacingBuilding());
	TestFalse(TEXT("Inspection cannot accidentally move a building"),City->PlaceSelected({30,20}));
	City->ClearBuildingSelection();
	TestFalse(TEXT("Empty-space deselection leaves editor active"),City->GetSelectedBuilding().IsValid());
	TestTrue(TEXT("Editor remains active without selection"),City->GetLayoutEditor().IsActive());
	TestTrue(TEXT("Stored card starts explicit placement"),City->SelectBuilding(Saw) && City->IsPlacingBuilding());
	TestFalse(TEXT("Illegal placement cannot consume stored building"),City->PlaceSelected({0,0}));
	TestTrue(TEXT("Failed placement keeps selected stored building"),City->GetSelectedBuilding()==Saw && City->IsPlacingBuilding());
	const auto& B=Before.Buildings[Before.CityEconomy.Instances[1].BuildingIndex];
	TestTrue(TEXT("Place stored building back on its legal position"),City->PlaceSelected(B.CellOrigin));
	TestFalse(TEXT("Successful placement releases cursor"),City->IsPlacingBuilding() || City->GetSelectedBuilding().IsValid());
	TestEqual(TEXT("Selection and draft operations never save"),Save->GetRevision(),Revision);
	TestTrue(TEXT("Full saved profile unchanged"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Before,Save->GetProfileSnapshot(),0));
	return true;
}
#endif
