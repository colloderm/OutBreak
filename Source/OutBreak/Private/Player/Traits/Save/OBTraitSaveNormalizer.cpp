// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Save/OBTraitSaveNormalizer.h"

#include "Player/Traits/Data/OBTraitTreeData.h"
#include "Player/Traits/Runtime/OBTraitRuntimeState.h"

namespace OBTraitSaveNormalizer
{
	FText Message(const FString& Value)
	{
		return FText::FromString(Value);
	}

	int32 SumCosts(const TArray<int32>& Costs)
	{
		int32 Total = 0;
		for (const int32 Cost : Costs)
		{
			Total += FMath::Max(0, Cost);
		}
		return Total;
	}

	TArray<int32> BuildPaidCosts(
		const UOBTraitNodeDefinition& Node,
		const FOBTraitSaveNode& SavedNode,
		int32 NormalizedRank)
	{
		TArray<int32> Result;
		Result.Reserve(NormalizedRank);
		for (int32 Rank = 1; Rank <= NormalizedRank; ++Rank)
		{
			const int32 SavedIndex = Rank - 1;
			const int32 PaidCost = SavedNode.PaidCostsByRank.IsValidIndex(SavedIndex)
				? SavedNode.PaidCostsByRank[SavedIndex]
				: Node.GetPointCostForRank(Rank);
			Result.Add(FMath::Max(0, PaidCost));
		}

		if (SavedNode.PaidCostsByRank.IsEmpty() && SavedNode.LegacyInvestedPoints > 0 && !Result.IsEmpty())
		{
			int32 Difference = SavedNode.LegacyInvestedPoints - SumCosts(Result);
			if (Difference > 0)
			{
				Result.Last() += Difference;
			}
			else
			{
				for (int32 RankIndex = Result.Num() - 1; RankIndex >= 0 && Difference < 0; --RankIndex)
				{
					const int32 Reduction = FMath::Min(Result[RankIndex], -Difference);
					Result[RankIndex] -= Reduction;
					Difference += Reduction;
				}
			}
		}

		return Result;
	}

	struct FNormalizedCandidate
	{
		FName SourceId;
		FGameplayTag NodeId;
		TArray<int32> PaidCostsByRank;
	};
}

FOBTraitSaveData FOBTraitSaveNormalizer::MakeSaveData(
	const UOBTraitTreeData& Tree,
	const FOBTraitPlayerState& State)
{
	FOBTraitSaveData SaveData;
	SaveData.SchemaVersion = FOBTraitSaveData::LatestSchemaVersion;
	SaveData.TraitTreeId = Tree.GetPrimaryAssetId();
	SaveData.TraitDefinitionVersion = Tree.GetDefinitionVersion();
	SaveData.SelectedSpecialtyId = State.SelectedSpecialtyId.GetTagName();
	SaveData.LifetimeEarnedPoints = State.PointLedger.LifetimeEarnedPoints;
	SaveData.SavedTotalSpentPoints = State.GetTotalInvestedPoints();

	for (const FOBTraitNodeState& StateEntry : State.NodeStates)
	{
		if (StateEntry.GetRank() <= 0)
		{
			continue;
		}

		FOBTraitSaveNode& SavedNode = SaveData.Nodes.AddDefaulted_GetRef();
		SavedNode.NodeId = StateEntry.NodeId.GetTagName();
		SavedNode.Rank = StateEntry.GetRank();
		SavedNode.PaidCostsByRank = StateEntry.PaidCostsByRank;
		SavedNode.LegacyInvestedPoints = StateEntry.GetInvestedPoints();
	}

	return SaveData;
}

bool FOBTraitSaveNormalizer::Normalize(
	const UOBTraitTreeData& Tree,
	const FOBTraitSaveData& SaveData,
	FOBTraitPlayerState& OutState,
	FOBTraitSaveMigrationReport& OutReport)
{
	using namespace OBTraitSaveNormalizer;
	OutState = FOBTraitPlayerState();
	OutReport = FOBTraitSaveMigrationReport();

	if (Tree.ValidateDefinition().HasErrors())
	{
		OutReport.AddIssue(
			EOBTraitDefinitionIssueSeverity::Error,
			NAME_None,
			NAME_None,
			Message(TEXT("Trait definition is invalid; Save data was not restored.")));
		return false;
	}

	if (SaveData.SchemaVersion > FOBTraitSaveData::LatestSchemaVersion)
	{
		OutReport.AddIssue(
			EOBTraitDefinitionIssueSeverity::Error,
			NAME_None,
			NAME_None,
			Message(TEXT("Trait Save schema is newer than this runtime and cannot be restored safely.")));
		return false;
	}

	if (SaveData.TraitTreeId.IsValid() && SaveData.TraitTreeId != Tree.GetPrimaryAssetId())
	{
		OutReport.AddIssue(
			EOBTraitDefinitionIssueSeverity::Error,
			SaveData.TraitTreeId.PrimaryAssetName,
			Tree.GetPrimaryAssetId().PrimaryAssetName,
			Message(TEXT("Trait Save belongs to a different trait tree.")));
		return false;
	}

	if (SaveData.SchemaVersion < FOBTraitSaveData::LatestSchemaVersion
		|| SaveData.TraitDefinitionVersion != Tree.GetDefinitionVersion())
	{
		OutReport.AddIssue(
			EOBTraitDefinitionIssueSeverity::Warning,
			NAME_None,
			NAME_None,
			Message(TEXT("Trait Save version differs from the current definition; normalization rules were applied.")));
	}

	const int32 LifetimeEarnedPoints = FMath::Max(0, SaveData.LifetimeEarnedPoints);
	TMap<FGameplayTag, FNormalizedCandidate> CandidateByNodeId;

	for (const FOBTraitSaveNode& SavedNode : SaveData.Nodes)
	{
		bool bRedirectCycle = false;
		const FName ResolvedId = Tree.ResolveLegacyId(SavedNode.NodeId, &bRedirectCycle);
		if (bRedirectCycle)
		{
			OutReport.RefundedPoints += FMath::Max(SavedNode.LegacyInvestedPoints, SumCosts(SavedNode.PaidCostsByRank));
			OutReport.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				SavedNode.NodeId,
				NAME_None,
				Message(TEXT("Trait ID redirect cycle detected; saved investment was refunded.")));
			continue;
		}

		const UOBTraitNodeDefinition* Node = Tree.FindNodeByName(ResolvedId);
		if (!Node)
		{
			OutReport.RefundedPoints += FMath::Max(SavedNode.LegacyInvestedPoints, SumCosts(SavedNode.PaidCostsByRank));
			OutReport.AddIssue(
				EOBTraitDefinitionIssueSeverity::Warning,
				SavedNode.NodeId,
				ResolvedId,
				Message(TEXT("Saved trait no longer exists; its investment was refunded.")));
			continue;
		}

		const int32 RequestedRank = FMath::Max(0, SavedNode.Rank);
		const int32 NormalizedRank = FMath::Min(RequestedRank, Node->GetMaxRank());
		TArray<int32> PaidCosts = BuildPaidCosts(*Node, SavedNode, NormalizedRank);

		if (SavedNode.PaidCostsByRank.Num() > NormalizedRank)
		{
			for (int32 RankIndex = NormalizedRank; RankIndex < SavedNode.PaidCostsByRank.Num(); ++RankIndex)
			{
				OutReport.RefundedPoints += FMath::Max(0, SavedNode.PaidCostsByRank[RankIndex]);
			}
		}
		if (RequestedRank > NormalizedRank)
		{
			OutReport.AddIssue(
				EOBTraitDefinitionIssueSeverity::Warning,
				SavedNode.NodeId,
				ResolvedId,
				Message(TEXT("Saved trait rank exceeded the current maximum and was clamped.")));
		}

		FNormalizedCandidate Candidate;
		Candidate.SourceId = SavedNode.NodeId;
		Candidate.NodeId = Node->GetNodeId();
		Candidate.PaidCostsByRank = MoveTemp(PaidCosts);

		if (FNormalizedCandidate* Existing = CandidateByNodeId.Find(Candidate.NodeId))
		{
			const bool bUseCandidate = Candidate.PaidCostsByRank.Num() > Existing->PaidCostsByRank.Num()
				|| (Candidate.PaidCostsByRank.Num() == Existing->PaidCostsByRank.Num()
					&& SumCosts(Candidate.PaidCostsByRank) > SumCosts(Existing->PaidCostsByRank));

			OutReport.RefundedPoints += bUseCandidate
				? SumCosts(Existing->PaidCostsByRank)
				: SumCosts(Candidate.PaidCostsByRank);
			if (bUseCandidate)
			{
				*Existing = MoveTemp(Candidate);
			}

			OutReport.AddIssue(
				EOBTraitDefinitionIssueSeverity::Warning,
				SavedNode.NodeId,
				ResolvedId,
				Message(TEXT("Multiple saved records resolved to one trait; the less advanced record was refunded.")));
		}
		else
		{
			CandidateByNodeId.Add(Candidate.NodeId, MoveTemp(Candidate));
		}
	}

	TArray<FNormalizedCandidate> Candidates;
	CandidateByNodeId.GenerateValueArray(Candidates);
	Candidates.Sort([](const FNormalizedCandidate& Left, const FNormalizedCandidate& Right)
	{
		return Left.NodeId.ToString() < Right.NodeId.ToString();
	});

	int32 AcceptedSpentPoints = 0;
	for (FNormalizedCandidate& Candidate : Candidates)
	{
		FOBTraitNodeState StateEntry;
		StateEntry.NodeId = Candidate.NodeId;
		for (const int32 PaidCost : Candidate.PaidCostsByRank)
		{
			const int32 SafeCost = FMath::Max(0, PaidCost);
			if (AcceptedSpentPoints + SafeCost > LifetimeEarnedPoints)
			{
				OutReport.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Candidate.SourceId,
					Candidate.NodeId.GetTagName(),
					Message(TEXT("Saved investment exceeded lifetime earned points and was truncated.")));
				break;
			}

			StateEntry.PaidCostsByRank.Add(SafeCost);
			AcceptedSpentPoints += SafeCost;
		}

		if (StateEntry.GetRank() > 0)
		{
			OutState.NodeStates.Add(MoveTemp(StateEntry));
		}
	}

	bool bSpecialtyRedirectCycle = false;
	const FName ResolvedSpecialtyId = Tree.ResolveLegacyId(SaveData.SelectedSpecialtyId, &bSpecialtyRedirectCycle);
	if (!bSpecialtyRedirectCycle)
	{
		if (const FOBTraitSpecialtyDefinition* Specialty = Tree.FindSpecialtyByName(ResolvedSpecialtyId))
		{
			OutState.SelectedSpecialtyId = Specialty->SpecialtyId;
		}
		else if (!SaveData.SelectedSpecialtyId.IsNone())
		{
			OutReport.AddIssue(
				EOBTraitDefinitionIssueSeverity::Warning,
				SaveData.SelectedSpecialtyId,
				ResolvedSpecialtyId,
				Message(TEXT("Saved specialty no longer exists and was cleared.")));
		}
	}
	else
	{
		OutReport.AddIssue(
			EOBTraitDefinitionIssueSeverity::Error,
			SaveData.SelectedSpecialtyId,
			NAME_None,
			Message(TEXT("Saved specialty ID redirect cycle was detected and the specialty was cleared.")));
	}

	// Prune structurally invalid dependent nodes until the state reaches a fixed point.
	bool bRemovedNode = true;
	while (bRemovedNode)
	{
		bRemovedNode = false;
		for (int32 StateIndex = OutState.NodeStates.Num() - 1; StateIndex >= 0; --StateIndex)
		{
			const FOBTraitNodeState& StateEntry = OutState.NodeStates[StateIndex];
			const UOBTraitNodeDefinition* Node = Tree.FindNode(StateEntry.NodeId);
			bool bValid = Node != nullptr;

			if (bValid && !Node->GetSpecialtyScope().IsEmpty())
			{
				bValid = Node->GetSpecialtyScope().HasTagExact(OutState.SelectedSpecialtyId);
			}

			if (bValid)
			{
				for (const FOBTraitPrerequisite& Prerequisite : Node->GetPrerequisites())
				{
					if (OutState.GetRank(Prerequisite.RequiredNodeId) < Prerequisite.RequiredRank)
					{
						bValid = false;
						break;
					}
				}
			}

			if (!bValid)
			{
				OutReport.RefundedPoints += StateEntry.GetInvestedPoints();
				OutReport.AddIssue(
					EOBTraitDefinitionIssueSeverity::Warning,
					StateEntry.NodeId.GetTagName(),
					StateEntry.NodeId.GetTagName(),
					Message(TEXT("Saved trait no longer satisfies structural requirements and was refunded.")));
				OutState.NodeStates.RemoveAt(StateIndex);
				bRemovedNode = true;
			}
		}
	}

	TMap<FGameplayTag, FGameplayTag> ExclusiveGroupOwner;
	for (int32 StateIndex = OutState.NodeStates.Num() - 1; StateIndex >= 0; --StateIndex)
	{
		const FOBTraitNodeState& StateEntry = OutState.NodeStates[StateIndex];
		const UOBTraitNodeDefinition* Node = Tree.FindNode(StateEntry.NodeId);
		bool bConflict = false;
		if (Node)
		{
			for (const FGameplayTag& GroupId : Node->GetExclusiveGroups())
			{
				if (ExclusiveGroupOwner.Contains(GroupId))
				{
					bConflict = true;
					break;
				}
			}
		}

		if (bConflict)
		{
			OutReport.RefundedPoints += StateEntry.GetInvestedPoints();
			OutReport.AddIssue(
				EOBTraitDefinitionIssueSeverity::Warning,
				StateEntry.NodeId.GetTagName(),
				StateEntry.NodeId.GetTagName(),
				Message(TEXT("Mutually exclusive saved traits conflicted; one record was refunded.")));
			OutState.NodeStates.RemoveAt(StateIndex);
			continue;
		}

		if (Node)
		{
			for (const FGameplayTag& GroupId : Node->GetExclusiveGroups())
			{
				ExclusiveGroupOwner.Add(GroupId, Node->GetNodeId());
			}

			if (!Node->GetUnlockConditions().IsEmpty())
			{
				OutReport.bRequiresExternalConditionRevalidation = true;
			}
		}
	}

	const int32 NormalizedSpentPoints = OutState.GetTotalInvestedPoints();
	if (FMath::Max(0, SaveData.SavedTotalSpentPoints) != NormalizedSpentPoints)
	{
		OutReport.AddIssue(
			EOBTraitDefinitionIssueSeverity::Warning,
			NAME_None,
			NAME_None,
			Message(TEXT("Saved spent-point audit value differed from normalized node costs and was recomputed.")));
	}
	OutState.PointLedger.InitializeBalances(LifetimeEarnedPoints, NormalizedSpentPoints);
	OutState.StateRevision = 1;
	return true;
}
