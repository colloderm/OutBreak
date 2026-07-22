// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "ST_CheckAttackRange.generated.h"

class AActor;
class APawn;

USTRUCT()
struct OUTBREAK_API FSTTCheckAttackRangeInstanceData
{
	GENERATED_BODY()

	/*
	 * State Tree Context의 Pawn과 바인딩
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;

	/*
	 * AI Perception에서 획득한 TargetActor와 바인딩
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Input")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float AttackRange = 150.0f;
};

USTRUCT(
	meta = (
		DisplayName = "Check Attack Range",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FSTTCheckAttackRange
	: public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType =
		FSTTCheckAttackRangeInstanceData;

	FSTTCheckAttackRange();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;
};
