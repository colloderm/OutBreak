// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "Player/Traits/Data/OBTraitCoreTypes.h"
#include "OBTraitCondition.generated.h"

class UOBTraitTreeData;
struct FOBTraitPlayerState;
struct FOBTraitExternalEvaluationSnapshot;

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitConditionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	bool bPassed = true;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	EOBTraitValidationFailure FailureReason = EOBTraitValidationFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag BlockingId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 RequiredValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 ActualValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText Message;

	static FOBTraitConditionResult Pass();
	static FOBTraitConditionResult Fail(
		EOBTraitValidationFailure FailureReason,
		FGameplayTag BlockingId = FGameplayTag(),
		int32 RequiredValue = 0,
		int32 ActualValue = 0,
		const FText& Message = FText::GetEmpty());
};

struct OUTBREAK_API FOBTraitConditionContext
{
	const UOBTraitTreeData* Tree = nullptr;
	const FOBTraitPlayerState* PlayerState = nullptr;
	const FOBTraitExternalEvaluationSnapshot* ExternalSnapshot = nullptr;
};

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitConditionDefinition : public UObject
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const
		PURE_VIRTUAL(UOBTraitConditionDefinition::Evaluate, return FOBTraitConditionResult::Pass(););
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_NodeRank : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag RequiredNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 RequiredRank = 1;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_TotalInvestedPoints : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	int32 RequiredPoints = 0;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_BranchInvestedPoints : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	int32 RequiredPoints = 0;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_SelectedSpecialty : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer AllowedSpecialtyIds;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_PlayerLevel : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	int32 RequiredLevel = 0;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_GameplayTagQuery : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	FGameplayTagQuery RequiredTags;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_ChangesAllowed : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_AllOf : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOBTraitConditionDefinition>> Conditions;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_AnyOf : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOBTraitConditionDefinition>> Conditions;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitCondition_Not : public UOBTraitConditionDefinition
{
	GENERATED_BODY()

public:
	virtual FOBTraitConditionResult Evaluate(const FOBTraitConditionContext& Context) const override;

private:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UOBTraitConditionDefinition> Condition;
};
