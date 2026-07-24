// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Player/Traits/Effects/OBTraitEffectTypes.h"
#include "OBTraitEffectDefinition.generated.h"

class UGameplayAbility;
class UGameplayEffect;

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitEffectDefinition : public UObject
{
	GENERATED_BODY()

public:
	bool IsActiveAtRank(int32 Rank) const;
	virtual void BuildEffectPlan(FGameplayTag NodeId, int32 CurrentRank, FOBTraitEffectPlan& OutPlan) const;

	FName GetEffectId() const { return EffectId; }
	int32 GetMinActiveRank() const { return MinActiveRank; }
	int32 GetMaxActiveRank() const { return MaxActiveRank; }

protected:
	FOBTraitEffectSourceKey MakeSourceKey(FGameplayTag NodeId, int32 CurrentRank) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	FName EffectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 MinActiveRank = 1;

	// Zero means there is no upper bound.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	int32 MaxActiveRank = 0;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitEffect_GameplayEffect : public UOBTraitEffectDefinition
{
	GENERATED_BODY()

public:
	virtual void BuildEffectPlan(FGameplayTag NodeId, int32 CurrentRank, FOBTraitEffectPlan& OutPlan) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|GameplayEffect", Meta = (AllowPrivateAccess = "true"))
	TSoftClassPtr<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|GameplayEffect", Meta = (AllowPrivateAccess = "true"))
	FOBTraitRankedValue EffectLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|GameplayEffect", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitRankedTagMagnitude> SetByCallerMagnitudes;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitEffect_AbilityGrant : public UOBTraitEffectDefinition
{
	GENERATED_BODY()

public:
	virtual void BuildEffectPlan(FGameplayTag NodeId, int32 CurrentRank, FOBTraitEffectPlan& OutPlan) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Ability", Meta = (AllowPrivateAccess = "true"))
	TSoftClassPtr<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Ability", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Ability", Meta = (AllowPrivateAccess = "true"))
	TArray<int32> AbilityLevelsByRank;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitEffect_GameplayTag : public UOBTraitEffectDefinition
{
	GENERATED_BODY()

public:
	virtual void BuildEffectPlan(FGameplayTag NodeId, int32 CurrentRank, FOBTraitEffectPlan& OutPlan) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|GameplayTag", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag GrantedTag;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitEffect_Attribute : public UOBTraitEffectDefinition
{
	GENERATED_BODY()

public:
	virtual void BuildEffectPlan(FGameplayTag NodeId, int32 CurrentRank, FOBTraitEffectPlan& OutPlan) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Attribute", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag AttributeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Attribute", Meta = (AllowPrivateAccess = "true"))
	EOBTraitNumericOperation Operation = EOBTraitNumericOperation::Add;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Attribute", Meta = (AllowPrivateAccess = "true"))
	FOBTraitRankedValue Magnitudes;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitEffect_Capability : public UOBTraitEffectDefinition
{
	GENERATED_BODY()

public:
	virtual void BuildEffectPlan(FGameplayTag NodeId, int32 CurrentRank, FOBTraitEffectPlan& OutPlan) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Capability", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag CapabilityTag;
};
