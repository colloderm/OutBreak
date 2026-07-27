// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "CharacterMovementTask.generated.h"

class AEnemyCharacter;
/**
 * 
 */
USTRUCT()
struct OUTBREAK_API FCharacterMovementModifyInstanceData
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere,
		Category = "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float WantMaxWalkSpeed = 300.0f;

	/*
	 * State 진입 당시의 속도.
	 */
	UPROPERTY(
		Transient)
	float OriginMovementSpeed = 0.0f;

	/*
	 * 실제로 속도 변경이 적용되었는지 표시합니다.
	 *
	 * EnterState가 실패한 뒤 ExitState가 호출되더라도
	 * 잘못된 값으로 복원하지 않도록 사용합니다.
	 */
	UPROPERTY(
		Transient)
	bool bMovementSpeedApplied = false;
};
USTRUCT(
	meta = (
		DisplayName = "Modify Character Movement",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FCharacterMovementModify
	: public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType =
		FCharacterMovementModifyInstanceData;

	FCharacterMovementModify()
	{
		/*
		 * 이 Task는 State 완료를 결정하지 않고,
		 * State가 활성화된 동안 속도만 수정합니다.
		 */
		bConsideredForCompletion = false;

		/*
		 * 매 Tick 처리할 내용이 없습니다.
		 */
		bShouldCallTick = false;
	}

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

