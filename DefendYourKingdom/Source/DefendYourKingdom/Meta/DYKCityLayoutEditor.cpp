#include "DYKCityLayoutEditor.h"
#include "DYKCityEconomy.h"
#include "../Saving/DYKProfileService.h"

bool FDYKCityLayoutEditor::Begin(const FDYKPlayerProfileSaveData& P)
{
	if (bActive || !DYKCityEconomy::Validate(P)
		|| !DYKCityTerritory::Validate(P,DYKCityTerritory::DEV_PLACEHOLDER())
		|| P.Buildings.Num() != P.CityEconomy.Instances.Num()) return false;
	Original = P;
	Draft.Reset();
	for (const auto& State : P.CityEconomy.Instances)
	{
		const auto& B = P.Buildings[State.BuildingIndex];
		Draft.Add({State.InstanceId,B.CellOrigin,B.RotationQuarterTurns,true});
	}
	CommandId = FGuid::NewGuid();
	bActive = true;
	return true;
}

bool FDYKCityLayoutEditor::ValidateDraft(const TArray<FDYKCityLayoutEntry>& Layout, bool RequireAll) const
{
	if (!bActive || Layout.Num() != Original.CityEconomy.Instances.Num()) return false;
	TSet<FGuid> Ids;
	TArray<FDYKBuildingSaveData> Placed;
	for (const auto& Entry : Layout)
	{
		const auto* State = Original.CityEconomy.Instances.FindByPredicate([&](const auto& S) { return S.InstanceId == Entry.InstanceId; });
		if (!State || Ids.Contains(Entry.InstanceId)) return false;
		Ids.Add(Entry.InstanceId);
		if (!Entry.bPlaced) { if (RequireAll) return false; else continue; }
		auto B = Original.Buildings[State->BuildingIndex];
		B.CellOrigin = Entry.Origin; B.RotationQuarterTurns = Entry.QuarterTurns;
		Placed.Add(B);
	}
	return DYKCityTerritory::ValidateLayout(Original,Placed,DYKCityTerritory::DEV_PLACEHOLDER());
}

bool FDYKCityLayoutEditor::HasConflict(const FDYKPlayerProfileSaveData& P) const
{
	return bActive && (!DYKCityEconomy::Validate(P) || !MatchesOriginal(P)
		|| !DYKCityTerritory::Validate(P,DYKCityTerritory::DEV_PLACEHOLDER()));
}

bool FDYKCityLayoutEditor::Store(FGuid Instance)
{
	if (!bActive) return false;
	auto* Entry = Draft.FindByPredicate([Instance](const auto& E) { return E.InstanceId == Instance; });
	if (!Entry || !Entry->bPlaced) return false;
	Entry->bPlaced = false;
	return true;
}

bool FDYKCityLayoutEditor::CanPlace(FGuid Instance, FIntPoint Origin, int32 Turns) const
{
	if (!bActive) return false;
	auto Candidate = Draft;
	auto* Entry = Candidate.FindByPredicate([Instance](const auto& E) { return E.InstanceId == Instance; });
	if (!Entry) return false;
	Entry->Origin = Origin; Entry->QuarterTurns = Turns; Entry->bPlaced = true;
	return ValidateDraft(Candidate,false);
}

bool FDYKCityLayoutEditor::RotateStored(FGuid Instance,int32 Turns)
{
	if (!bActive || Turns<0 || Turns>3) return false;
	auto* Entry=Draft.FindByPredicate([Instance](const auto& E){return E.InstanceId==Instance;});
	if (!Entry || Entry->bPlaced) return false;
	Entry->QuarterTurns=Turns;
	return true;
}

bool FDYKCityLayoutEditor::Place(FGuid Instance, FIntPoint Origin, int32 Turns)
{
	if (!CanPlace(Instance,Origin,Turns)) return false;
	auto& Entry = *Draft.FindByPredicate([Instance](const auto& E) { return E.InstanceId == Instance; });
	Entry.Origin = Origin; Entry.QuarterTurns = Turns; Entry.bPlaced = true;
	return true;
}

bool FDYKCityLayoutEditor::MatchesOriginal(const FDYKPlayerProfileSaveData& P) const
{
	if (P.Buildings.Num() != Original.Buildings.Num() || P.CityEconomy.Instances.Num() != Draft.Num()) return false;
	for (const auto& State : Original.CityEconomy.Instances)
	{
		const auto* Current = P.CityEconomy.Instances.FindByPredicate([&](const auto& S) { return S.InstanceId == State.InstanceId; });
		if (!Current || Current->BuildingIndex != State.BuildingIndex) return false;
		const auto& A = P.Buildings[Current->BuildingIndex];
		const auto& B = Original.Buildings[State.BuildingIndex];
		if (A.BuildingId != B.BuildingId || A.CellOrigin != B.CellOrigin || A.RotationQuarterTurns != B.RotationQuarterTurns) return false;
	}
	return true;
}

bool FDYKCityLayoutEditor::Commit(IDYKProfileService& Service)
{
	if (!ValidateDraft(Draft,true)) return false;
	const bool Saved = Service.ApplyProfileCommand(CommandId,[this](auto& P)
	{
		if (!DYKCityEconomy::Validate(P) || !MatchesOriginal(P)
			|| !DYKCityTerritory::Validate(P,DYKCityTerritory::DEV_PLACEHOLDER())) return false;
		// Patch only placement onto the latest profile: concurrent collection and timers survive.
		for (const auto& Entry : Draft)
		{
			const auto& State = *P.CityEconomy.Instances.FindByPredicate([&](const auto& S) { return S.InstanceId == Entry.InstanceId; });
			auto& B = P.Buildings[State.BuildingIndex];
			B.CellOrigin = Entry.Origin; B.RotationQuarterTurns = Entry.QuarterTurns;
		}
		return DYKCityEconomy::Validate(P) && DYKCityTerritory::Validate(P,DYKCityTerritory::DEV_PLACEHOLDER());
	});
	if (Saved) Cancel();
	return Saved;
}

void FDYKCityLayoutEditor::Cancel()
{
	bActive = false;
	Draft.Reset();
	Original = FDYKPlayerProfileSaveData();
	CommandId.Invalidate();
}
