// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Effects/OBTraitEffectPlanBuilder.h"

#include "Player/Traits/Data/OBTraitTreeData.h"
#include "Player/Traits/Effects/OBTraitEffectDefinition.h"
#include "Player/Traits/Runtime/OBTraitRuntimeState.h"

namespace OBTraitEffectPlanBuilder
{
	void AddSources(const FOBTraitEffectPlan& Plan, TSet<FOBTraitEffectSourceKey>& OutSources)
	{
		for (const FOBTraitGameplayEffectRequest& Request : Plan.GameplayEffects)
		{
			OutSources.Add(Request.Source);
		}
		for (const FOBTraitAbilityGrantRequest& Request : Plan.AbilityGrants)
		{
			OutSources.Add(Request.Source);
		}
		for (const FOBTraitGameplayTagRequest& Request : Plan.GameplayTags)
		{
			OutSources.Add(Request.Source);
		}
		for (const FOBTraitAttributeRequest& Request : Plan.AttributeModifiers)
		{
			OutSources.Add(Request.Source);
		}
		for (const FOBTraitCapabilityRequest& Request : Plan.Capabilities)
		{
			OutSources.Add(Request.Source);
		}
	}

	template <typename RequestType>
	void CopyUnappliedRequests(
		const TArray<RequestType>& DesiredRequests,
		const TSet<FOBTraitEffectSourceKey>& AppliedSources,
		TArray<RequestType>& OutRequests)
	{
		for (const RequestType& Request : DesiredRequests)
		{
			if (!AppliedSources.Contains(Request.Source))
			{
				OutRequests.Add(Request);
			}
		}
	}
}

FOBTraitEffectPlan FOBTraitEffectPlanBuilder::BuildDesiredPlan(
	const UOBTraitTreeData& Tree,
	const FOBTraitPlayerState& State)
{
	FOBTraitEffectPlan Plan;
	for (const FOBTraitNodeState& NodeState : State.NodeStates)
	{
		const int32 CurrentRank = NodeState.GetRank();
		const UOBTraitNodeDefinition* Node = Tree.FindNode(NodeState.NodeId);
		if (!Node || CurrentRank <= 0)
		{
			continue;
		}

		for (const UOBTraitEffectDefinition* Effect : Node->GetEffects())
		{
			if (Effect)
			{
				Effect->BuildEffectPlan(NodeState.NodeId, CurrentRank, Plan);
			}
		}
	}
	return Plan;
}

FOBTraitEffectReconciliationPlan FOBTraitEffectPlanBuilder::BuildReconciliationPlan(
	const UOBTraitTreeData& Tree,
	const FOBTraitPlayerState& State,
	const TArray<FOBTraitAppliedSourceState>& AppliedSources)
{
	using namespace OBTraitEffectPlanBuilder;
	FOBTraitEffectReconciliationPlan Reconciliation;
	const FOBTraitEffectPlan DesiredPlan = BuildDesiredPlan(Tree, State);

	TSet<FOBTraitEffectSourceKey> DesiredSourceKeys;
	AddSources(DesiredPlan, DesiredSourceKeys);

	TSet<FOBTraitEffectSourceKey> AppliedSourceKeys;
	for (const FOBTraitAppliedSourceState& AppliedSource : AppliedSources)
	{
		AppliedSourceKeys.Add(AppliedSource.Source);
		if (!DesiredSourceKeys.Contains(AppliedSource.Source))
		{
			Reconciliation.SourcesToRemove.AddUnique(AppliedSource.Source);
		}
	}

	CopyUnappliedRequests(
		DesiredPlan.GameplayEffects,
		AppliedSourceKeys,
		Reconciliation.EffectsToApply.GameplayEffects);
	CopyUnappliedRequests(
		DesiredPlan.AbilityGrants,
		AppliedSourceKeys,
		Reconciliation.EffectsToApply.AbilityGrants);
	CopyUnappliedRequests(
		DesiredPlan.GameplayTags,
		AppliedSourceKeys,
		Reconciliation.EffectsToApply.GameplayTags);
	CopyUnappliedRequests(
		DesiredPlan.AttributeModifiers,
		AppliedSourceKeys,
		Reconciliation.EffectsToApply.AttributeModifiers);
	CopyUnappliedRequests(
		DesiredPlan.Capabilities,
		AppliedSourceKeys,
		Reconciliation.EffectsToApply.Capabilities);

	return Reconciliation;
}
