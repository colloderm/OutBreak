// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Conditions/OBTraitCondition.h"

#include "Player/Traits/Data/OBTraitTreeData.h"
#include "Player/Traits/Runtime/OBTraitRuntimeState.h"

FOBTraitConditionResult FOBTraitConditionResult::Pass()
{
	return FOBTraitConditionResult();
}

FOBTraitConditionResult FOBTraitConditionResult::Fail(
	EOBTraitValidationFailure FailureReason,
	FGameplayTag BlockingId,
	int32 RequiredValue,
	int32 ActualValue,
	const FText& Message)
{
	FOBTraitConditionResult Result;
	Result.bPassed = false;
	Result.FailureReason = FailureReason;
	Result.BlockingId = BlockingId;
	Result.RequiredValue = RequiredValue;
	Result.ActualValue = ActualValue;
	Result.Message = Message;
	return Result;
}

FOBTraitConditionResult UOBTraitCondition_NodeRank::Evaluate(const FOBTraitConditionContext& Context) const
{
	if (!Context.PlayerState)
	{
		return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid);
	}

	const int32 CurrentRank = Context.PlayerState->GetRank(RequiredNodeId);
	return CurrentRank >= RequiredRank
		? FOBTraitConditionResult::Pass()
		: FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::PrerequisiteNotMet,
			RequiredNodeId,
			RequiredRank,
			CurrentRank);
}

FOBTraitConditionResult UOBTraitCondition_TotalInvestedPoints::Evaluate(
	const FOBTraitConditionContext& Context) const
{
	if (!Context.PlayerState)
	{
		return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid);
	}

	const int32 CurrentPoints = Context.PlayerState->GetTotalInvestedPoints();
	return CurrentPoints >= RequiredPoints
		? FOBTraitConditionResult::Pass()
		: FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::BranchPointsNotMet,
			FGameplayTag(),
			RequiredPoints,
			CurrentPoints);
}

FOBTraitConditionResult UOBTraitCondition_BranchInvestedPoints::Evaluate(
	const FOBTraitConditionContext& Context) const
{
	if (!Context.Tree || !Context.PlayerState)
	{
		return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid);
	}

	const int32 CurrentPoints = Context.PlayerState->GetBranchInvestedPoints(*Context.Tree, BranchId);
	return CurrentPoints >= RequiredPoints
		? FOBTraitConditionResult::Pass()
		: FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::BranchPointsNotMet,
			BranchId,
			RequiredPoints,
			CurrentPoints);
}

FOBTraitConditionResult UOBTraitCondition_SelectedSpecialty::Evaluate(
	const FOBTraitConditionContext& Context) const
{
	if (!Context.PlayerState)
	{
		return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid);
	}

	return (AllowedSpecialtyIds.IsEmpty()
		|| AllowedSpecialtyIds.HasTagExact(Context.PlayerState->SelectedSpecialtyId))
		? FOBTraitConditionResult::Pass()
		: FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::SpecialtyMismatch,
			Context.PlayerState->SelectedSpecialtyId);
}

FOBTraitConditionResult UOBTraitCondition_PlayerLevel::Evaluate(
	const FOBTraitConditionContext& Context) const
{
	if (!Context.ExternalSnapshot)
	{
		return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::RequiredLevelNotMet);
	}

	return Context.ExternalSnapshot->PlayerLevel >= RequiredLevel
		? FOBTraitConditionResult::Pass()
		: FOBTraitConditionResult::Fail(
			EOBTraitValidationFailure::RequiredLevelNotMet,
			FGameplayTag(),
			RequiredLevel,
			Context.ExternalSnapshot->PlayerLevel);
}

FOBTraitConditionResult UOBTraitCondition_GameplayTagQuery::Evaluate(
	const FOBTraitConditionContext& Context) const
{
	if (RequiredTags.IsEmpty())
	{
		return FOBTraitConditionResult::Pass();
	}

	if (!Context.ExternalSnapshot)
	{
		return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::GameplayTagRequirementNotMet);
	}

	return RequiredTags.Matches(Context.ExternalSnapshot->OwnedGameplayTags)
		? FOBTraitConditionResult::Pass()
		: FOBTraitConditionResult::Fail(EOBTraitValidationFailure::GameplayTagRequirementNotMet);
}

FOBTraitConditionResult UOBTraitCondition_ChangesAllowed::Evaluate(
	const FOBTraitConditionContext& Context) const
{
	return Context.ExternalSnapshot && Context.ExternalSnapshot->bChangesAllowed
		? FOBTraitConditionResult::Pass()
		: FOBTraitConditionResult::Fail(EOBTraitValidationFailure::ChangesNotAllowed);
}

FOBTraitConditionResult UOBTraitCondition_AllOf::Evaluate(const FOBTraitConditionContext& Context) const
{
	for (const UOBTraitConditionDefinition* ConditionEntry : Conditions)
	{
		if (!ConditionEntry)
		{
			return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid);
		}

		const FOBTraitConditionResult Result = ConditionEntry->Evaluate(Context);
		if (!Result.bPassed)
		{
			return Result;
		}
	}

	return FOBTraitConditionResult::Pass();
}

FOBTraitConditionResult UOBTraitCondition_AnyOf::Evaluate(const FOBTraitConditionContext& Context) const
{
	FOBTraitConditionResult LastFailure = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::ConditionFailed);
	for (const UOBTraitConditionDefinition* ConditionEntry : Conditions)
	{
		if (!ConditionEntry)
		{
			LastFailure = FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid);
			continue;
		}

		const FOBTraitConditionResult Result = ConditionEntry->Evaluate(Context);
		if (Result.bPassed)
		{
			return Result;
		}
		LastFailure = Result;
	}

	return LastFailure;
}

FOBTraitConditionResult UOBTraitCondition_Not::Evaluate(const FOBTraitConditionContext& Context) const
{
	if (!Condition)
	{
		return FOBTraitConditionResult::Fail(EOBTraitValidationFailure::DefinitionInvalid);
	}

	const FOBTraitConditionResult InnerResult = Condition->Evaluate(Context);
	return InnerResult.bPassed
		? FOBTraitConditionResult::Fail(EOBTraitValidationFailure::ConditionFailed)
		: FOBTraitConditionResult::Pass();
}
