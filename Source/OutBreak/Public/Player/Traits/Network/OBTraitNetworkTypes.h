// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Traits/Runtime/OBTraitRuleEvaluator.h"
#include "OBTraitNetworkTypes.generated.h"

struct FOBTraitPlayerState;

UENUM(BlueprintType)
enum class EOBTraitMutationCommandType : uint8
{
	Invest,
	Reset,
	SelectSpecialty
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitMutationCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Network")
	EOBTraitMutationCommandType CommandType = EOBTraitMutationCommandType::Invest;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Network")
	FGuid RequestId;

	// Node, reset scope, or specialty ID depending on CommandType.
	UPROPERTY(BlueprintReadWrite, Category = "Trait|Network")
	FGameplayTag TargetId;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Network", Meta = (ClampMin = "1"))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitMutationResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	EOBTraitMutationCommandType CommandType = EOBTraitMutationCommandType::Invest;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	FOBTraitConditionResult Result;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	int32 ResultingStateRevision = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitNodeStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	FGameplayTag NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	int32 InvestedPoints = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	int32 StateRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	FGameplayTag SelectedSpecialtyId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	int32 LifetimeEarnedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	int32 AvailablePoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	int32 TotalInvestedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Network")
	TArray<FOBTraitNodeStateSnapshot> Nodes;

	static FOBTraitStateSnapshot FromRuntimeState(const FOBTraitPlayerState& State);
};
