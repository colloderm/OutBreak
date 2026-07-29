// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Effects/OBTraitEffectDefinition.h"

bool UOBTraitEffectDefinition::IsActiveAtRank(int32 Rank) const
{
	return Rank >= MinActiveRank && (MaxActiveRank <= 0 || Rank <= MaxActiveRank);
}

void UOBTraitEffectDefinition::BuildEffectPlan(
	FGameplayTag NodeId,
	int32 CurrentRank,
	FOBTraitEffectPlan& OutPlan) const
{
	// Intentionally empty. New effect types extend this class instead of adding a central switch.
}

FOBTraitEffectSourceKey UOBTraitEffectDefinition::MakeSourceKey(FGameplayTag NodeId, int32 CurrentRank) const
{
	FOBTraitEffectSourceKey Source;
	Source.NodeId = NodeId;
	Source.EffectId = EffectId;
	Source.Rank = CurrentRank;
	return Source;
}

void UOBTraitEffect_GameplayEffect::BuildEffectPlan(
	FGameplayTag NodeId,
	int32 CurrentRank,
	FOBTraitEffectPlan& OutPlan) const
{
	if (!IsActiveAtRank(CurrentRank) || GameplayEffectClass.IsNull())
	{
		return;
	}

	FOBTraitGameplayEffectRequest& Request = OutPlan.GameplayEffects.AddDefaulted_GetRef();
	Request.Source = MakeSourceKey(NodeId, CurrentRank);
	Request.GameplayEffectClass = GameplayEffectClass;
	Request.EffectLevel = EffectLevels.GetValue(CurrentRank, 1.0f);

	for (const FOBTraitRankedTagMagnitude& RankedMagnitude : SetByCallerMagnitudes)
	{
		if (RankedMagnitude.DataTag.IsValid())
		{
			Request.SetByCallerMagnitudes.Add(
				RankedMagnitude.DataTag,
				RankedMagnitude.Magnitudes.GetValue(CurrentRank));
		}
	}
}

void UOBTraitEffect_AbilityGrant::BuildEffectPlan(
	FGameplayTag NodeId,
	int32 CurrentRank,
	FOBTraitEffectPlan& OutPlan) const
{
	if (!IsActiveAtRank(CurrentRank) || AbilityClass.IsNull())
	{
		return;
	}

	FOBTraitAbilityGrantRequest& Request = OutPlan.AbilityGrants.AddDefaulted_GetRef();
	Request.Source = MakeSourceKey(NodeId, CurrentRank);
	Request.AbilityClass = AbilityClass;
	Request.InputTag = InputTag;
	Request.AbilityLevel = AbilityLevelsByRank.IsEmpty()
		? 1
		: AbilityLevelsByRank[FMath::Min(CurrentRank - 1, AbilityLevelsByRank.Num() - 1)];
}

void UOBTraitEffect_GameplayTag::BuildEffectPlan(
	FGameplayTag NodeId,
	int32 CurrentRank,
	FOBTraitEffectPlan& OutPlan) const
{
	if (!IsActiveAtRank(CurrentRank) || !GrantedTag.IsValid())
	{
		return;
	}

	FOBTraitGameplayTagRequest& Request = OutPlan.GameplayTags.AddDefaulted_GetRef();
	Request.Source = MakeSourceKey(NodeId, CurrentRank);
	Request.GameplayTag = GrantedTag;
}

void UOBTraitEffect_Attribute::BuildEffectPlan(
	FGameplayTag NodeId,
	int32 CurrentRank,
	FOBTraitEffectPlan& OutPlan) const
{
	if (!IsActiveAtRank(CurrentRank) || !AttributeId.IsValid())
	{
		return;
	}

	FOBTraitAttributeRequest& Request = OutPlan.AttributeModifiers.AddDefaulted_GetRef();
	Request.Source = MakeSourceKey(NodeId, CurrentRank);
	Request.AttributeId = AttributeId;
	Request.Operation = Operation;
	Request.Magnitude = Magnitudes.GetValue(CurrentRank);
}

void UOBTraitEffect_Capability::BuildEffectPlan(
	FGameplayTag NodeId,
	int32 CurrentRank,
	FOBTraitEffectPlan& OutPlan) const
{
	if (!IsActiveAtRank(CurrentRank) || !CapabilityTag.IsValid())
	{
		return;
	}

	FOBTraitCapabilityRequest& Request = OutPlan.Capabilities.AddDefaulted_GetRef();
	Request.Source = MakeSourceKey(NodeId, CurrentRank);
	Request.CapabilityTag = CapabilityTag;
}
