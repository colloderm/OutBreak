// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/StateTree/Task/CharacterMovementTask.h"
#include "StateTreeExecutionContext.h"
#include "AI/EnemyCharacter.h"
#include "AI/Components/EnemyMovementComponent.h"

EStateTreeRunStatus FCharacterMovementModify::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	/*
	 * 이전 실행 상태가 남지 않도록 초기화합니다.
	 */
	InstanceData.bMovementSpeedApplied = false;
	InstanceData.OriginMovementSpeed = 0.0f;

	APawn* ControlledPawn =
		InstanceData.ControlledPawn.Get();

	if (!IsValid(ControlledPawn))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"%s::%s: ControlledPawn is invalid."),
			TEXT("Task_CharacterMovementModify"),
			TEXT(__FUNCTION__));

		return EStateTreeRunStatus::Failed;
	}

	UEnemyMovementComponent* MovementComponent =
		Cast<UEnemyMovementComponent>(
			ControlledPawn->GetMovementComponent());

	if (!IsValid(MovementComponent))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"%s::%s: EnemyMovementComponent is invalid. Pawn=%s"),
			TEXT("Task_CharacterMovementModify"),
			TEXT(__FUNCTION__),
			*GetNameSafe(ControlledPawn));

		return EStateTreeRunStatus::Failed;
	}

	/*
	 * State 진입 직전의 실제 속도를 저장합니다.
	 */
	InstanceData.OriginMovementSpeed =
		MovementComponent->MaxWalkSpeed;

	MovementComponent->MaxWalkSpeed =
		FMath::Max(
			0.0f,
			InstanceData.WantMaxWalkSpeed);

	InstanceData.bMovementSpeedApplied = true;

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT(
			"%s::%s: MaxWalkSpeed %.2f -> %.2f"),
		TEXT("Task_CharacterMovementModify"),
		TEXT(__FUNCTION__),
		InstanceData.OriginMovementSpeed,
		MovementComponent->MaxWalkSpeed);

	/*
	 * Succeeded를 반환하면 Task가 즉시 완료됩니다.
	 * State가 유지되는 동안 속도 변경을 유지하려면 Running입니다.
	 */
	return EStateTreeRunStatus::Running;
}

void FCharacterMovementModify::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	/*
	 * EnterState에서 속도를 적용하지 못했다면
	 * 복원할 값도 없습니다.
	 */
	if (InstanceData.bMovementSpeedApplied)
	{
		APawn* ControlledPawn =
			InstanceData.ControlledPawn.Get();

		if (IsValid(ControlledPawn))
		{
			UEnemyMovementComponent* MovementComponent =
				Cast<UEnemyMovementComponent>(
					ControlledPawn->GetMovementComponent());

			if (IsValid(MovementComponent))
			{
				MovementComponent->MaxWalkSpeed =
					InstanceData.OriginMovementSpeed;

				UE_LOG(
					LogTemp,
					Verbose,
					TEXT(
						"%s::%s: MaxWalkSpeed restored to %.2f"),
					TEXT("Task_CharacterMovementModify"),
					TEXT(__FUNCTION__),
					InstanceData.OriginMovementSpeed);
			}
			else
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"%s::%s: Could not restore speed because "
						"EnemyMovementComponent is invalid. Pawn=%s"),
					TEXT("Task_CharacterMovementModify"),
					TEXT(__FUNCTION__),
					*GetNameSafe(ControlledPawn));
			}
		}

		InstanceData.bMovementSpeedApplied = false;
		InstanceData.OriginMovementSpeed = 0.0f;
	}

	FStateTreeAITaskBase::ExitState(
		Context,
		Transition);
}