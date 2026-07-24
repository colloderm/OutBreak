// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OBTraitEffectTypes.generated.h"

class UGameplayAbility;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EOBTraitNumericOperation : uint8
{
	Add,
	Multiply,
	Override
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitRankedValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	TArray<float> ValuesByRank;

	float GetValue(int32 Rank, float DefaultValue = 0.0f) const
	{
		if (Rank <= 0 || ValuesByRank.IsEmpty())
		{
			return DefaultValue;
		}

		return ValuesByRank[FMath::Min(Rank - 1, ValuesByRank.Num() - 1)];
	}
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitRankedTagMagnitude
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayTag DataTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FOBTraitRankedValue Magnitudes;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitEffectSourceKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FName EffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 Rank = 0;

	bool operator==(const FOBTraitEffectSourceKey& Other) const
	{
		return NodeId == Other.NodeId && EffectId == Other.EffectId && Rank == Other.Rank;
	}
};

FORCEINLINE uint32 GetTypeHash(const FOBTraitEffectSourceKey& Key)
{
	return HashCombine(HashCombine(GetTypeHash(Key.NodeId), GetTypeHash(Key.EffectId)), GetTypeHash(Key.Rank));
}

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitGameplayEffectRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitEffectSourceKey Source;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TSoftClassPtr<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	float EffectLevel = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitAbilityGrantRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitEffectSourceKey Source;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TSoftClassPtr<UGameplayAbility> AbilityClass;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag InputTag;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 AbilityLevel = 1;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitGameplayTagRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitEffectSourceKey Source;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag GameplayTag;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitAttributeRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitEffectSourceKey Source;

	// TODO(Integration): Map this stable tag to a concrete FGameplayAttribute in the GAS adapter.
	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag AttributeId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	EOBTraitNumericOperation Operation = EOBTraitNumericOperation::Add;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	float Magnitude = 0.0f;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitCapabilityRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitEffectSourceKey Source;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag CapabilityTag;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitEffectPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitGameplayEffectRequest> GameplayEffects;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitAbilityGrantRequest> AbilityGrants;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitGameplayTagRequest> GameplayTags;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitAttributeRequest> AttributeModifiers;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitCapabilityRequest> Capabilities;

	bool IsEmpty() const
	{
		return GameplayEffects.IsEmpty()
			&& AbilityGrants.IsEmpty()
			&& GameplayTags.IsEmpty()
			&& AttributeModifiers.IsEmpty()
			&& Capabilities.IsEmpty();
	}
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitAppliedEffectHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGuid RuntimeHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitEffectSourceKey Source;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FName AdapterHandleType;

	bool IsValid() const
	{
		return RuntimeHandle.IsValid();
	}
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitAppliedSourceState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitEffectSourceKey Source;

	// Runtime-only provenance. Never serialize these handles into SaveGame data. The future
	// adapter must publish a source here only after all requests for that source apply atomically.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitAppliedEffectHandle> Handles;
};
