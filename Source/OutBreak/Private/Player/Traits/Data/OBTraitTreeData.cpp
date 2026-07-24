// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Data/OBTraitTreeData.h"

#include "Player/Traits/Conditions/OBTraitCondition.h"
#include "Player/Traits/Effects/OBTraitEffectDefinition.h"

namespace OBTraitTreeValidation
{
	FText Message(const FString& Value)
	{
		return FText::FromString(Value);
	}
}

int32 UOBTraitNodeDefinition::GetPointCostForRank(int32 Rank) const
{
	return Ranks.IsValidIndex(Rank - 1) ? Ranks[Rank - 1].PointCost : INDEX_NONE;
}

int32 UOBTraitNodeDefinition::GetCumulativePointCost(int32 Rank) const
{
	const int32 SafeRank = FMath::Clamp(Rank, 0, Ranks.Num());
	int32 Total = 0;
	for (int32 RankIndex = 0; RankIndex < SafeRank; ++RankIndex)
	{
		Total += FMath::Max(0, Ranks[RankIndex].PointCost);
	}
	return Total;
}

FPrimaryAssetId UOBTraitTreeData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("OBTraitTree")), GetFName());
}

const UOBTraitNodeDefinition* UOBTraitTreeData::FindNode(FGameplayTag NodeId) const
{
	const TObjectPtr<UOBTraitNodeDefinition>* FoundNode = Nodes.FindByPredicate([NodeId](const UOBTraitNodeDefinition* Node)
	{
		return Node && Node->GetNodeId() == NodeId;
	});
	return FoundNode ? FoundNode->Get() : nullptr;
}

const UOBTraitNodeDefinition* UOBTraitTreeData::FindNodeByName(FName NodeId) const
{
	const TObjectPtr<UOBTraitNodeDefinition>* FoundNode = Nodes.FindByPredicate([NodeId](const UOBTraitNodeDefinition* Node)
	{
		return Node && Node->GetNodeId().GetTagName() == NodeId;
	});
	return FoundNode ? FoundNode->Get() : nullptr;
}

const FOBTraitBranchDefinition* UOBTraitTreeData::FindBranch(FGameplayTag BranchId) const
{
	return Branches.FindByPredicate([BranchId](const FOBTraitBranchDefinition& Branch)
	{
		return Branch.BranchId == BranchId;
	});
}

const FOBTraitBranchDefinition* UOBTraitTreeData::FindBranchByName(FName BranchId) const
{
	return Branches.FindByPredicate([BranchId](const FOBTraitBranchDefinition& Branch)
	{
		return Branch.BranchId.GetTagName() == BranchId;
	});
}

const FOBTraitSpecialtyDefinition* UOBTraitTreeData::FindSpecialty(FGameplayTag SpecialtyId) const
{
	return Specialties.FindByPredicate([SpecialtyId](const FOBTraitSpecialtyDefinition& Specialty)
	{
		return Specialty.SpecialtyId == SpecialtyId;
	});
}

const FOBTraitSpecialtyDefinition* UOBTraitTreeData::FindSpecialtyByName(FName SpecialtyId) const
{
	return Specialties.FindByPredicate([SpecialtyId](const FOBTraitSpecialtyDefinition& Specialty)
	{
		return Specialty.SpecialtyId.GetTagName() == SpecialtyId;
	});
}

const UOBTraitSignatureAbilityDefinition* UOBTraitTreeData::FindSignatureAbility(FGameplayTag SignatureId) const
{
	const TObjectPtr<UOBTraitSignatureAbilityDefinition>* FoundSignature = SignatureAbilities.FindByPredicate(
		[SignatureId](const UOBTraitSignatureAbilityDefinition* Signature)
	{
		return Signature && Signature->GetSignatureId() == SignatureId;
	});
	return FoundSignature ? FoundSignature->Get() : nullptr;
}

FName UOBTraitTreeData::ResolveLegacyId(FName CandidateId, bool* bOutRedirectCycle) const
{
	if (bOutRedirectCycle)
	{
		*bOutRedirectCycle = false;
	}

	TSet<FName> VisitedIds;
	FName CurrentId = CandidateId;
	while (!CurrentId.IsNone())
	{
		if (VisitedIds.Contains(CurrentId))
		{
			if (bOutRedirectCycle)
			{
				*bOutRedirectCycle = true;
			}
			return NAME_None;
		}

		VisitedIds.Add(CurrentId);
		const FOBTraitLegacyIdRedirect* Redirect = LegacyIdRedirects.FindByPredicate([CurrentId](const FOBTraitLegacyIdRedirect& Entry)
		{
			return Entry.OldId == CurrentId;
		});

		if (!Redirect)
		{
			return CurrentId;
		}

		CurrentId = Redirect->NewId;
	}

	return NAME_None;
}

FOBTraitDefinitionValidationReport UOBTraitTreeData::ValidateDefinition() const
{
	using namespace OBTraitTreeValidation;
	FOBTraitDefinitionValidationReport Report;

	TSet<FGameplayTag> BranchIds;
	for (const FOBTraitBranchDefinition& Branch : Branches)
	{
		if (!Branch.BranchId.IsValid() || BranchIds.Contains(Branch.BranchId))
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Trait branch ID is invalid or duplicated.")),
				Branch.BranchId);
		}
		BranchIds.Add(Branch.BranchId);
	}

	TSet<FGameplayTag> SignatureIds;
	for (const UOBTraitSignatureAbilityDefinition* Signature : SignatureAbilities)
	{
		if (!Signature || !Signature->GetSignatureId().IsValid() || SignatureIds.Contains(Signature->GetSignatureId()))
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Signature ability ID is invalid or duplicated.")),
				Signature ? Signature->GetSignatureId() : FGameplayTag());
			continue;
		}

		SignatureIds.Add(Signature->GetSignatureId());
		if (Signature->GetAbilityClass().IsNull())
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Warning,
				Message(TEXT("Signature ability has no ability asset assigned.")),
				Signature->GetSignatureId());
		}
		if (Signature->GetUnlockConditions().Contains(nullptr))
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Signature ability contains a null unlock condition.")),
				Signature->GetSignatureId());
		}
	}

	TSet<FGameplayTag> SpecialtyIds;
	for (const FOBTraitSpecialtyDefinition& Specialty : Specialties)
	{
		if (!Specialty.SpecialtyId.IsValid() || SpecialtyIds.Contains(Specialty.SpecialtyId))
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Trait specialty ID is invalid or duplicated.")),
				Specialty.SpecialtyId);
		}
		SpecialtyIds.Add(Specialty.SpecialtyId);

		for (const FGameplayTag& BranchId : Specialty.LinkedBranchIds)
		{
			if (!BranchIds.Contains(BranchId))
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Specialty references an unknown trait branch.")),
					Specialty.SpecialtyId,
					BranchId);
			}
		}

		for (const FGameplayTag& SignatureId : Specialty.SignatureAbilityIds)
		{
			if (!SignatureIds.Contains(SignatureId))
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Specialty references an unknown signature ability.")),
					Specialty.SpecialtyId,
					SignatureId);
			}
		}
	}

	TSet<FGameplayTag> NodeIds;
	for (const UOBTraitNodeDefinition* Node : Nodes)
	{
		if (!Node || !Node->GetNodeId().IsValid() || NodeIds.Contains(Node->GetNodeId()))
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Trait node ID is invalid or duplicated.")),
				Node ? Node->GetNodeId() : FGameplayTag());
			continue;
		}

		NodeIds.Add(Node->GetNodeId());
	}

	for (const UOBTraitNodeDefinition* Node : Nodes)
	{
		if (!Node || !Node->GetNodeId().IsValid())
		{
			continue;
		}

		if (!BranchIds.Contains(Node->GetBranchId()))
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Trait node references an unknown branch.")),
				Node->GetNodeId(),
				Node->GetBranchId());
		}

		for (const FGameplayTag& SpecialtyId : Node->GetSpecialtyScope())
		{
			const FOBTraitSpecialtyDefinition* Specialty = FindSpecialty(SpecialtyId);
			if (!Specialty)
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Trait node references an unknown specialty.")),
					Node->GetNodeId(),
					SpecialtyId);
			}
			else if (!Specialty->LinkedBranchIds.HasTagExact(Node->GetBranchId()))
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Trait node specialty scope is not linked to the node branch.")),
					Node->GetNodeId(),
					SpecialtyId);
			}
		}

		if (Node->GetRanks().IsEmpty())
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Trait node must define at least one rank.")),
				Node->GetNodeId());
		}

		for (const FOBTraitRankDefinition& Rank : Node->GetRanks())
		{
			if (Rank.PointCost < 0)
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Trait rank cost cannot be negative.")),
					Node->GetNodeId());
			}
		}

		for (const FOBTraitPrerequisite& Prerequisite : Node->GetPrerequisites())
		{
			const UOBTraitNodeDefinition* RequiredNode = FindNode(Prerequisite.RequiredNodeId);
			if (!RequiredNode || Prerequisite.RequiredRank <= 0 || Prerequisite.RequiredRank > RequiredNode->GetMaxRank())
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Trait prerequisite is invalid.")),
					Node->GetNodeId(),
					Prerequisite.RequiredNodeId);
			}
		}

		TSet<FName> EffectIds;
		for (const UOBTraitEffectDefinition* Effect : Node->GetEffects())
		{
			if (!Effect || Effect->GetEffectId().IsNone() || EffectIds.Contains(Effect->GetEffectId()))
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Trait effect ID is invalid or duplicated within a node.")),
					Node->GetNodeId());
				continue;
			}

			EffectIds.Add(Effect->GetEffectId());
			if (Effect->GetMinActiveRank() > Node->GetMaxRank()
				|| (Effect->GetMaxActiveRank() > 0 && Effect->GetMaxActiveRank() < Effect->GetMinActiveRank()))
			{
				Report.AddIssue(
					EOBTraitDefinitionIssueSeverity::Error,
					Message(TEXT("Trait effect rank range is invalid.")),
					Node->GetNodeId());
			}
		}

		if (Node->GetUnlockConditions().Contains(nullptr))
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Trait node contains a null unlock condition.")),
				Node->GetNodeId());
		}
	}

	TMap<FGameplayTag, uint8> VisitState;
	TFunction<void(const UOBTraitNodeDefinition*)> VisitNode;
	VisitNode = [this, &Report, &VisitState, &VisitNode](const UOBTraitNodeDefinition* Node)
	{
		if (!Node)
		{
			return;
		}

		uint8& State = VisitState.FindOrAdd(Node->GetNodeId());
		if (State == 2)
		{
			return;
		}
		if (State == 1)
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				OBTraitTreeValidation::Message(TEXT("Trait prerequisite cycle detected.")),
				Node->GetNodeId());
			return;
		}

		State = 1;
		for (const FOBTraitPrerequisite& Prerequisite : Node->GetPrerequisites())
		{
			VisitNode(FindNode(Prerequisite.RequiredNodeId));
		}
		State = 2;
	};

	for (const UOBTraitNodeDefinition* Node : Nodes)
	{
		VisitNode(Node);
	}

	TSet<FName> RedirectSources;
	for (const FOBTraitLegacyIdRedirect& Redirect : LegacyIdRedirects)
	{
		bool bCycle = false;
		ResolveLegacyId(Redirect.OldId, &bCycle);
		if (Redirect.OldId.IsNone() || Redirect.NewId.IsNone() || RedirectSources.Contains(Redirect.OldId) || bCycle)
		{
			Report.AddIssue(
				EOBTraitDefinitionIssueSeverity::Error,
				Message(TEXT("Trait legacy ID redirect is invalid, duplicated, or cyclic.")));
		}
		RedirectSources.Add(Redirect.OldId);
	}

	return Report;
}
