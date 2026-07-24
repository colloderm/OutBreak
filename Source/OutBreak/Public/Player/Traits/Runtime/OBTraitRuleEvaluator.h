// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Traits/Conditions/OBTraitCondition.h"
#include "Player/Traits/Runtime/OBTraitRuntimeState.h"
#include "OBTraitRuleEvaluator.generated.h"

class UOBTraitTreeData;

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitInvestmentValidation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitConditionResult Condition;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 CurrentRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 NextRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 PointCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 MissingPoints = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitInvestmentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitInvestmentValidation Validation;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 PreviousRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 NewRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 TotalPointsSpent = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitResetResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitConditionResult Condition;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 RefundedPoints = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSpecialtySelectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitConditionResult Condition;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag PreviousSpecialtyId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag NewSpecialtyId;
};

/** Pure trait investment rules. This class performs no RPC, replication, GAS, SaveGame, or UI work. */
class OUTBREAK_API FOBTraitRuleEvaluator
{
public:
	static constexpr int32 MaxInvestmentsPerRequest = 64;

	static FOBTraitInvestmentValidation EvaluateSingleInvestment(
		const UOBTraitTreeData& Tree,
		const FOBTraitPlayerState& State,
		FGameplayTag NodeId,
		const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot);

	static FOBTraitInvestmentResult EvaluateInvestmentRequest(
		const UOBTraitTreeData& Tree,
		const FOBTraitPlayerState& State,
		const FOBTraitInvestmentRequest& Request,
		const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot);

	static bool TryApplyInvestment(
		const UOBTraitTreeData& Tree,
		FOBTraitPlayerState& InOutState,
		const FOBTraitInvestmentRequest& Request,
		const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot,
		FOBTraitInvestmentResult& OutResult);

	static bool TryResetAll(
		FOBTraitPlayerState& InOutState,
		const FOBTraitResetRequest& Request,
		const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot,
		FOBTraitResetResult& OutResult);

	static bool TrySelectSpecialty(
		const UOBTraitTreeData& Tree,
		FOBTraitPlayerState& InOutState,
		const FOBTraitSpecialtySelectionRequest& Request,
		const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot,
		FOBTraitSpecialtySelectionResult& OutResult);
};
