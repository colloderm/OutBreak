// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "PlayMontageTask.generated.h"

/**
 * 
 */

class AEnemyCharacter;
class AActor;
class APawn;
class UAnimInstance;
class UAnimMontage;

USTRUCT()
struct OUTBREAK_API FSTTPlayMontageTaskInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(
		EditAnywhere,
		Category = "Context")
	TObjectPtr<AEnemyCharacter> ControlledPawn = nullptr;
	
	/*
	 * 재생할 공격 Montage.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;
	
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	/*
	 * NAME_None이면 Montage 처음부터 재생한다.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter")
	FName StartSectionName = NAME_None;

	/* 실행 시 움직임을 멈출지. */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter")
	bool bStopMovementOnEnter = true;

	/*
	 * 공격 중 State가 강제로 종료되면 Montage도 중단한다.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter")
	bool bStopMontageOnExit = true;

	/* 몽타주 */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float MontageBlendOutTime = 0.15f;

protected:
	/*
	 * 실제 Montage를 재생한 AnimInstance.
	 */
	UPROPERTY()
	TWeakObjectPtr<UAnimInstance> PlayingAnimInstance;

	UPROPERTY()
	bool bMontageStarted = false;

	friend struct FSTTPlayMontageTask;
};


USTRUCT(meta = (DisplayName = "Task_PlayMontage"), Category = "OutBreak|AI")
struct OUTBREAK_API FSTTPlayMontageTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTPlayMontageTaskInstanceData;

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

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};


