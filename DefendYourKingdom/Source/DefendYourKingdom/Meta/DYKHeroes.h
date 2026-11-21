#pragma once
#include "DYKCityResearch.h"

struct FDYKHeroAbilityDefinition
{
	TDYKDefinitionValue<int32> Damage{20};
	TDYKDefinitionValue<float> Radius{10.f};
	TDYKDefinitionValue<float> ForwardDistance{5.f,EDYKDefinitionSource::DEV_PLACEHOLDER};
	TDYKDefinitionValue<float> Cooldown{25.f,EDYKDefinitionSource::DEV_PLACEHOLDER};
	TDYKDefinitionValue<float> ImpactDelay{.3f,EDYKDefinitionSource::DEV_PLACEHOLDER};
	TDYKDefinitionValue<EDYKDamageType> DamageType{EDYKDamageType::Physical,EDYKDefinitionSource::DEV_PLACEHOLDER};
};

namespace DYKHeroes
{
	DEFENDYOURKINGDOM_API bool Owns(const FDYKPlayerProfileSaveData& Profile);
	DEFENDYOURKINGDOM_API FString UnlockBlockReason(const FDYKPlayerProfileSaveData& Profile);
	DEFENDYOURKINGDOM_API FString HireBlockReason(const FDYKPlayerProfileSaveData& Profile);
	DEFENDYOURKINGDOM_API int64 HireGoldDEV_PLACEHOLDER();
	DEFENDYOURKINGDOM_API int32 RequiredLevelDEV_PLACEHOLDER(int32 Tier);
	DEFENDYOURKINGDOM_API bool Hire(IDYKProfileService& Service,FGuid Command,int64 Now);
	DEFENDYOURKINGDOM_API int32 Level(const FDYKPlayerProfileSaveData& Profile);
	DEFENDYOURKINGDOM_API FName ResearchId(FName Branch,int32 Step);
	DEFENDYOURKINGDOM_API int32 BranchLevel(const FDYKPlayerProfileSaveData& Profile,FName Branch);
	DEFENDYOURKINGDOM_API FName NextResearch(const FDYKPlayerProfileSaveData& Profile,FName Branch);
	DEFENDYOURKINGDOM_API void AppendResearchDEV_PLACEHOLDER(FDYKResearchConfig& Config);
	DEFENDYOURKINGDOM_API FDYKHeroAbilityDefinition Ability(const FDYKPlayerProfileSaveData* Profile=nullptr);
	// All hero identities, including story heroes, are unique; only player entries consume the cap.
	DEFENDYOURKINGDOM_API bool CanDeploy(FName Hero,TConstArrayView<FName> PlayerHeroes,TConstArrayView<FName> StoryHeroes,bool bStory);
}
