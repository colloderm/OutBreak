// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Traits/Effects/OBTraitEffectTypes.h"

class UOBTraitTreeData;
struct FOBTraitPlayerState;

/**
 * A side-effect-free description of how an integration layer should converge from its
 * currently tracked trait sources to the desired state. It never calls GAS or gameplay code.
 */
struct OUTBREAK_API FOBTraitEffectReconciliationPlan
{
	FOBTraitEffectPlan EffectsToApply;
	TArray<FOBTraitEffectSourceKey> SourcesToRemove;

	bool IsEmpty() const
	{
		return EffectsToApply.IsEmpty() && SourcesToRemove.IsEmpty();
	}
};

class OUTBREAK_API FOBTraitEffectPlanBuilder
{
public:
	static FOBTraitEffectPlan BuildDesiredPlan(
		const UOBTraitTreeData& Tree,
		const FOBTraitPlayerState& State);

	static FOBTraitEffectReconciliationPlan BuildReconciliationPlan(
		const UOBTraitTreeData& Tree,
		const FOBTraitPlayerState& State,
		const TArray<FOBTraitAppliedSourceState>& AppliedSources);
};
