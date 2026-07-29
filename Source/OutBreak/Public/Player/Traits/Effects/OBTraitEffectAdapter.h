// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Player/Traits/Effects/OBTraitEffectTypes.h"
#include "OBTraitEffectAdapter.generated.h"

UINTERFACE(MinimalAPI)
class UOBTraitEffectAdapter : public UInterface
{
	GENERATED_BODY()
};

/**
 * Injection boundary between the isolated trait core and gameplay systems.
 * No implementation is provided in this phase. An integration adapter may later translate
 * requests into GAS, Character, Weapon, Movement, Noise, AI, or interaction operations.
 */
class OUTBREAK_API IOBTraitEffectAdapter
{
	GENERATED_BODY()

public:
	virtual bool ApplyGameplayEffect(
		const FOBTraitGameplayEffectRequest& Request,
		FOBTraitAppliedEffectHandle& OutHandle) = 0;
	virtual void RemoveGameplayEffect(const FOBTraitAppliedEffectHandle& Handle) = 0;

	virtual bool GrantAbility(
		const FOBTraitAbilityGrantRequest& Request,
		FOBTraitAppliedEffectHandle& OutHandle) = 0;
	virtual void RevokeAbility(const FOBTraitAppliedEffectHandle& Handle) = 0;

	virtual bool AddGameplayTag(
		const FOBTraitGameplayTagRequest& Request,
		FOBTraitAppliedEffectHandle& OutHandle) = 0;
	virtual void RemoveGameplayTag(const FOBTraitAppliedEffectHandle& Handle) = 0;

	virtual bool ModifyAttribute(
		const FOBTraitAttributeRequest& Request,
		FOBTraitAppliedEffectHandle& OutHandle) = 0;
	virtual void RemoveAttributeModification(const FOBTraitAppliedEffectHandle& Handle) = 0;

	virtual bool UnlockCapability(
		const FOBTraitCapabilityRequest& Request,
		FOBTraitAppliedEffectHandle& OutHandle) = 0;
	virtual void LockCapability(const FOBTraitAppliedEffectHandle& Handle) = 0;

	virtual void NotifyTraitStateChanged(FGameplayTag NodeId, int32 OldRank, int32 NewRank) = 0;
};
