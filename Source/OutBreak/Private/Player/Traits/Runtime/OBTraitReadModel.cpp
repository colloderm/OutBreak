// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Runtime/OBTraitReadModel.h"

#include "Player/Traits/Data/OBTraitTreeData.h"
#include "Player/Traits/Data/OBTraitTreeLayoutData.h"
#include "Player/Traits/Runtime/OBTraitRuleEvaluator.h"
#include "Player/Traits/Runtime/OBTraitRuntimeState.h"

FOBTraitTreeReadModel FOBTraitReadModelBuilder::Build(
	const UOBTraitTreeData& Tree,
	const UOBTraitTreeLayoutData* Layout,
	const FOBTraitPlayerState& State,
	const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot)
{
	FOBTraitTreeReadModel ReadModel;
	ReadModel.DefinitionVersion = Tree.GetDefinitionVersion();
	ReadModel.LayoutVersion = Layout ? Layout->GetLayoutVersion() : 0;
	ReadModel.StateRevision = State.StateRevision;
	ReadModel.AvailablePoints = State.PointLedger.AvailablePoints;
	ReadModel.LifetimeEarnedPoints = State.PointLedger.LifetimeEarnedPoints;
	ReadModel.TotalInvestedPoints = State.GetTotalInvestedPoints();
	ReadModel.SelectedSpecialtyId = State.SelectedSpecialtyId;

	for (const FOBTraitSpecialtyDefinition& Specialty : Tree.GetSpecialties())
	{
		FOBTraitSpecialtyReadModel& SpecialtyReadModel = ReadModel.Specialties.AddDefaulted_GetRef();
		SpecialtyReadModel.SpecialtyId = Specialty.SpecialtyId;
		SpecialtyReadModel.DisplayName = Specialty.DisplayName;
		SpecialtyReadModel.Description = Specialty.Description;
		SpecialtyReadModel.Icon = Specialty.Icon;
		SpecialtyReadModel.LinkedBranchIds = Specialty.LinkedBranchIds;
		SpecialtyReadModel.SignatureAbilityIds = Specialty.SignatureAbilityIds;
		SpecialtyReadModel.bSelected = Specialty.SpecialtyId == State.SelectedSpecialtyId;
	}

	for (const FOBTraitBranchDefinition& Branch : Tree.GetBranches())
	{
		FOBTraitBranchReadModel& BranchReadModel = ReadModel.Branches.AddDefaulted_GetRef();
		BranchReadModel.BranchId = Branch.BranchId;
		BranchReadModel.DisplayName = Branch.DisplayName;
		BranchReadModel.Description = Branch.Description;
		BranchReadModel.Icon = Branch.Icon;
		BranchReadModel.SortOrder = Branch.SortOrder;
		BranchReadModel.InvestedPoints = State.GetBranchInvestedPoints(Tree, Branch.BranchId);
		BranchReadModel.bInitiallyVisible = Branch.bInitiallyVisible;
	}

	for (const UOBTraitNodeDefinition* Node : Tree.GetNodes())
	{
		if (!Node)
		{
			continue;
		}

		FOBTraitNodeReadModel& NodeReadModel = ReadModel.Nodes.AddDefaulted_GetRef();
		NodeReadModel.NodeId = Node->GetNodeId();
		NodeReadModel.BranchId = Node->GetBranchId();
		NodeReadModel.SpecialtyScope = Node->GetSpecialtyScope();
		NodeReadModel.DisplayName = Node->GetDisplayName();
		NodeReadModel.Description = Node->GetDescription();
		NodeReadModel.Icon = Node->GetIcon();
		NodeReadModel.Tier = Node->GetTier();
		NodeReadModel.CurrentRank = State.GetRank(Node->GetNodeId());
		NodeReadModel.MaxRank = Node->GetMaxRank();
		NodeReadModel.InvestedPoints = State.GetNodeInvestedPoints(Node->GetNodeId());
		NodeReadModel.NextRankCost = NodeReadModel.CurrentRank < NodeReadModel.MaxRank
			? Node->GetPointCostForRank(NodeReadModel.CurrentRank + 1)
			: 0;
		NodeReadModel.Prerequisites = Node->GetPrerequisites();
		NodeReadModel.ExclusiveGroups = Node->GetExclusiveGroups();
		NodeReadModel.bInitiallyUnlocked = Node->IsInitiallyUnlocked();
		NodeReadModel.bCapstone = Node->IsCapstone();

		for (const FOBTraitRankDefinition& Rank : Node->GetRanks())
		{
			NodeReadModel.RankEffectDescriptions.Add(Rank.EffectDescription);
		}
		if (Node->GetRanks().IsValidIndex(NodeReadModel.CurrentRank))
		{
			NodeReadModel.NextRankEffectDescription = Node->GetRanks()[NodeReadModel.CurrentRank].EffectDescription;
		}

		NodeReadModel.Investability = FOBTraitRuleEvaluator::EvaluateSingleInvestment(
			Tree,
			State,
			Node->GetNodeId(),
			ExternalSnapshot).Condition;

		if (Layout)
		{
			if (const FOBTraitNodeLayout* NodeLayout = Layout->FindNodeLayout(Node->GetNodeId()))
			{
				NodeReadModel.LayoutPosition = NodeLayout->Position;
				NodeReadModel.LayoutLayer = NodeLayout->Layer;
				NodeReadModel.LayoutSortOrder = NodeLayout->SortOrder;
			}
		}

		for (const FOBTraitPrerequisite& Prerequisite : Node->GetPrerequisites())
		{
			FOBTraitConnectionReadModel& Connection = ReadModel.Connections.AddDefaulted_GetRef();
			Connection.FromNodeId = Prerequisite.RequiredNodeId;
			Connection.ToNodeId = Node->GetNodeId();
			Connection.RequiredRank = Prerequisite.RequiredRank;

			if (Layout)
			{
				if (const FOBTraitConnectionLayout* ConnectionLayout = Layout->FindConnectionLayout(
					Connection.FromNodeId,
					Connection.ToNodeId))
				{
					Connection.ControlPoints = ConnectionLayout->ControlPoints;
					Connection.bVisible = ConnectionLayout->bVisible;
				}
			}
		}
	}

	FOBTraitConditionContext ConditionContext;
	ConditionContext.Tree = &Tree;
	ConditionContext.PlayerState = &State;
	ConditionContext.ExternalSnapshot = &ExternalSnapshot;
	for (const UOBTraitSignatureAbilityDefinition* Signature : Tree.GetSignatureAbilities())
	{
		if (!Signature)
		{
			continue;
		}

		FOBTraitSignatureReadModel& SignatureReadModel = ReadModel.SignatureAbilities.AddDefaulted_GetRef();
		SignatureReadModel.SignatureId = Signature->GetSignatureId();
		SignatureReadModel.DisplayName = Signature->GetDisplayName();
		SignatureReadModel.Description = Signature->GetDescription();
		SignatureReadModel.Icon = Signature->GetIcon();
		SignatureReadModel.InputTag = Signature->GetInputTag();
		SignatureReadModel.SlotTag = Signature->GetSlotTag();
		SignatureReadModel.UnlockState = FOBTraitConditionResult::Pass();

		for (const UOBTraitConditionDefinition* Condition : Signature->GetUnlockConditions())
		{
			if (!Condition)
			{
				SignatureReadModel.UnlockState = FOBTraitConditionResult::Fail(
					EOBTraitValidationFailure::DefinitionInvalid,
					Signature->GetSignatureId());
				break;
			}

			SignatureReadModel.UnlockState = Condition->Evaluate(ConditionContext);
			if (!SignatureReadModel.UnlockState.bPassed)
			{
				break;
			}
		}
	}

	ReadModel.Branches.Sort([](const FOBTraitBranchReadModel& Left, const FOBTraitBranchReadModel& Right)
	{
		return Left.SortOrder < Right.SortOrder;
	});
	ReadModel.Nodes.Sort([](const FOBTraitNodeReadModel& Left, const FOBTraitNodeReadModel& Right)
	{
		if (Left.LayoutLayer != Right.LayoutLayer)
		{
			return Left.LayoutLayer < Right.LayoutLayer;
		}
		return Left.LayoutSortOrder < Right.LayoutSortOrder;
	});
	ReadModel.Connections.Sort([](const FOBTraitConnectionReadModel& Left, const FOBTraitConnectionReadModel& Right)
	{
		if (Left.FromNodeId != Right.FromNodeId)
		{
			return Left.FromNodeId.ToString() < Right.FromNodeId.ToString();
		}
		return Left.ToNodeId.ToString() < Right.ToNodeId.ToString();
	});

	return ReadModel;
}
