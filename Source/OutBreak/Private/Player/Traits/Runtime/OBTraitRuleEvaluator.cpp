// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Runtime/OBTraitRuleEvaluator.h"

#include "Player/Traits/Data/OBTraitTreeData.h"

namespace OBTraitRuleEvaluator
{
	FOBTraitInvestmentValidation Failure(
		EOBTraitValidationFailure Reason,
		FGameplayTag BlockingId = FGameplayTag(),
		int32 RequiredValue = 0,
		int32 ActualValue = 0)
	{
		FOBTraitInvestmentValidation Validation;
		Validation.Condition = FOBTraitConditionResult::Fail(
			Reason,
			BlockingId,
			RequiredValue,
			ActualValue);
		return Validation;
	}
}

FOBTraitInvestmentValidation FOBTraitRuleEvaluator::EvaluateSingleInvestment(
	const UOBTraitTreeData& Tree,
	const FOBTraitPlayerState& State,
	FGameplayTag NodeId,
	const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot)
{
	using namespace OBTraitRuleEvaluator;

	if (!ExternalSnapshot.bChangesAllowed)
	{
		return Failure(EOBTraitValidationFailure::ChangesNotAllowed);
	}

	const UOBTraitNodeDefinition* Node = Tree.FindNode(NodeId);
	if (!Node)
	{
		return Failure(EOBTraitValidationFailure::NodeNotFound, NodeId);
	}

	if (!Tree.FindBranch(Node->GetBranchId()) || Node->GetMaxRank() <= 0)
	{
		return Failure(EOBTraitValidationFailure::DefinitionInvalid, NodeId);
	}

	FOBTraitInvestmentValidation Validation;
	Validation.CurrentRank = State.GetRank(NodeId);
	Validation.NextRank = Validation.CurrentRank + 1;
	if (Validation.CurrentRank >= Node->GetMaxRank())
	{
		Validation.Condition = FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::MaxRankReached,
			NodeId,
			Node->GetMaxRank(),
			Validation.CurrentRank);
		return Validation;
	}

	if (!Node->GetSpecialtyScope().IsEmpty()
		&& !Node->GetSpecialtyScope().HasTagExact(State.SelectedSpecialtyId))
	{
		Validation.Condition = FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::SpecialtyMismatch,
			State.SelectedSpecialtyId);
		return Validation;
	}

	for (const FOBTraitPrerequisite& Prerequisite : Node->GetPrerequisites())
	{
		const int32 ActualRank = State.GetRank(Prerequisite.RequiredNodeId);
		if (ActualRank < Prerequisite.RequiredRank)
		{
			Validation.Condition = FOBTraitConditionResult::Fail(
				EOBTraitValidationFailure::PrerequisiteNotMet,
				Prerequisite.RequiredNodeId,
				Prerequisite.RequiredRank,
				ActualRank);
			return Validation;
		}
	}

	if (!Node->GetExclusiveGroups().IsEmpty())
	{
		for (const FOBTraitNodeState& ExistingState : State.NodeStates)
		{
			if (ExistingState.GetRank() <= 0 || ExistingState.NodeId == NodeId)
			{
				continue;
			}

			const UOBTraitNodeDefinition* ExistingNode = Tree.FindNode(ExistingState.NodeId);
			if (ExistingNode && Node->GetExclusiveGroups().HasAnyExact(ExistingNode->GetExclusiveGroups()))
			{
				Validation.Condition = FOBTraitConditionResult::Fail(
					EOBTraitValidationFailure::ExclusiveConflict,
					ExistingState.NodeId);
				return Validation;
			}
		}
	}

	FOBTraitConditionContext ConditionContext;
	ConditionContext.Tree = &Tree;
	ConditionContext.PlayerState = &State;
	ConditionContext.ExternalSnapshot = &ExternalSnapshot;
	for (const UOBTraitConditionDefinition* Condition : Node->GetUnlockConditions())
	{
		if (!Condition)
		{
			Validation.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid, NodeId);
			return Validation;
		}

		const FOBTraitConditionResult ConditionResult = Condition->Evaluate(ConditionContext);
		if (!ConditionResult.bPassed)
		{
			Validation.Condition = ConditionResult;
			return Validation;
		}
	}

	Validation.PointCost = Node->GetPointCostForRank(Validation.NextRank);
	if (Validation.PointCost < 0)
	{
		Validation.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid, NodeId);
		return Validation;
	}

	if (State.PointLedger.AvailablePoints < Validation.PointCost)
	{
		Validation.MissingPoints = Validation.PointCost - State.PointLedger.AvailablePoints;
		Validation.Condition = FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::InsufficientPoints,
			NodeId,
			Validation.PointCost,
			State.PointLedger.AvailablePoints);
		return Validation;
	}

	Validation.Condition = FOBTraitConditionResult::Pass();
	return Validation;
}

FOBTraitInvestmentResult FOBTraitRuleEvaluator::EvaluateInvestmentRequest(
	const UOBTraitTreeData& Tree,
	const FOBTraitPlayerState& State,
	const FOBTraitInvestmentRequest& Request,
	const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot)
{
	FOBTraitPlayerState WorkingState = State;
	FOBTraitInvestmentResult Result;
	TryApplyInvestment(Tree, WorkingState, Request, ExternalSnapshot, Result);
	return Result;
}

bool FOBTraitRuleEvaluator::TryApplyInvestment(
	const UOBTraitTreeData& Tree,
	FOBTraitPlayerState& InOutState,
	const FOBTraitInvestmentRequest& Request,
	const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot,
	FOBTraitInvestmentResult& OutResult)
{
	OutResult = FOBTraitInvestmentResult();
	OutResult.PreviousRank = InOutState.GetRank(Request.NodeId);

	if (!Request.RequestId.IsValid() || !Request.NodeId.IsValid()
		|| Request.InvestmentCount <= 0 || Request.InvestmentCount > MaxInvestmentsPerRequest)
	{
		OutResult.Validation.Condition = FOBTraitConditionResult::Fail(
			Request.InvestmentCount <= 0 || Request.InvestmentCount > MaxInvestmentsPerRequest
				? EOBTraitValidationFailure::InvalidInvestmentCount
				: EOBTraitValidationFailure::InvalidRequest);
		return false;
	}

	if (InOutState.HasProcessedRequest(Request.RequestId))
	{
		OutResult.Validation.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DuplicateRequest);
		return false;
	}

	FOBTraitPlayerState WorkingState = InOutState;
	int32 TotalPointsSpent = 0;
	for (int32 InvestmentIndex = 0; InvestmentIndex < Request.InvestmentCount; ++InvestmentIndex)
	{
		const FOBTraitInvestmentValidation Validation = EvaluateSingleInvestment(
			Tree,
			WorkingState,
			Request.NodeId,
			ExternalSnapshot);

		if (!Validation.Condition.bPassed)
		{
			OutResult.Validation = Validation;
			OutResult.NewRank = InOutState.GetRank(Request.NodeId);
			return false;
		}

		if (!WorkingState.AddInvestment(Request.NodeId, Validation.PointCost, FGuid::NewGuid()))
		{
			OutResult.Validation.Condition = FOBTraitConditionResult::Fail(
				EOBTraitValidationFailure::DefinitionInvalid,
				Request.NodeId);
			return false;
		}

		TotalPointsSpent += Validation.PointCost;
		OutResult.Validation = Validation;
	}

	WorkingState.RecordProcessedRequest(Request.RequestId);
	OutResult.PreviousRank = InOutState.GetRank(Request.NodeId);
	OutResult.NewRank = WorkingState.GetRank(Request.NodeId);
	OutResult.TotalPointsSpent = TotalPointsSpent;
	InOutState = MoveTemp(WorkingState);
	return true;
}

bool FOBTraitRuleEvaluator::TryResetAll(
	FOBTraitPlayerState& InOutState,
	const FOBTraitResetRequest& Request,
	const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot,
	FOBTraitResetResult& OutResult)
{
	OutResult = FOBTraitResetResult();
	if (!Request.RequestId.IsValid() || Request.ScopeId.IsValid())
	{
		// TODO(Integration): Define branch/specialty/partial reset policy before accepting a ScopeId.
		OutResult.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::InvalidRequest, Request.ScopeId);
		return false;
	}
	if (!ExternalSnapshot.bChangesAllowed)
	{
		OutResult.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::ChangesNotAllowed);
		return false;
	}
	if (InOutState.HasProcessedRequest(Request.RequestId))
	{
		OutResult.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DuplicateRequest);
		return false;
	}

	FOBTraitPlayerState WorkingState = InOutState;
	OutResult.RefundedPoints = WorkingState.ResetAllInvestments(FGuid::NewGuid());
	WorkingState.RecordProcessedRequest(Request.RequestId);
	OutResult.Condition = FOBTraitConditionResult::Pass();
	InOutState = MoveTemp(WorkingState);
	return true;
}

bool FOBTraitRuleEvaluator::TrySelectSpecialty(
	const UOBTraitTreeData& Tree,
	FOBTraitPlayerState& InOutState,
	const FOBTraitSpecialtySelectionRequest& Request,
	const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot,
	FOBTraitSpecialtySelectionResult& OutResult)
{
	OutResult = FOBTraitSpecialtySelectionResult();
	OutResult.PreviousSpecialtyId = InOutState.SelectedSpecialtyId;
	OutResult.NewSpecialtyId = InOutState.SelectedSpecialtyId;

	if (!Request.RequestId.IsValid() || !Request.SpecialtyId.IsValid() || !Tree.FindSpecialty(Request.SpecialtyId))
	{
		OutResult.Condition = FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::InvalidRequest,
			Request.SpecialtyId);
		return false;
	}
	if (!ExternalSnapshot.bChangesAllowed)
	{
		OutResult.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::ChangesNotAllowed);
		return false;
	}
	if (InOutState.HasProcessedRequest(Request.RequestId))
	{
		OutResult.Condition = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DuplicateRequest);
		return false;
	}

	for (const FOBTraitNodeState& NodeState : InOutState.NodeStates)
	{
		if (NodeState.GetRank() <= 0)
		{
			continue;
		}

		const UOBTraitNodeDefinition* Node = Tree.FindNode(NodeState.NodeId);
		if (Node && !Node->GetSpecialtyScope().IsEmpty()
			&& !Node->GetSpecialtyScope().HasTagExact(Request.SpecialtyId))
		{
			// TODO(Integration): A future policy may auto-refund incompatible nodes instead of rejecting.
			OutResult.Condition = FOBTraitConditionResult::Fail(
				EOBTraitValidationFailure::SpecialtyMismatch,
				NodeState.NodeId);
			return false;
		}
	}

	InOutState.SelectedSpecialtyId = Request.SpecialtyId;
	InOutState.RecordProcessedRequest(Request.RequestId);
	++InOutState.StateRevision;
	OutResult.NewSpecialtyId = Request.SpecialtyId;
	OutResult.Condition = FOBTraitConditionResult::Pass();
	return true;
}
