
#include "AI/StateTree/Task/PlayMontageTask.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "AI/EnemyCharacter.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTTPlayMontageTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	InstanceData.bMontageStarted = false;
	InstanceData.PlayingAnimInstance.Reset();

	AEnemyCharacter* Character = InstanceData.ControlledPawn;

	if (!IsValid(Character) ||
		!IsValid(InstanceData.AttackMontage))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	Character->

	USkeletalMeshComponent* Mesh = Character->GetMesh();

	if (!IsValid(Mesh))
	{
		return EStateTreeRunStatus::Failed;
	}

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();

	if (!IsValid(AnimInstance))
	{
		return EStateTreeRunStatus::Failed;
	}

	AAIController* AIController =
		Cast<AAIController>(Character->GetController());

	if (IsValid(AIController))
	{
		if (InstanceData.bStopMovementOnEnter)
		{
			AIController->StopMovement();
		}
	}

	const float MontageDuration =
		Character->PlayAnimMontage(
			InstanceData.AttackMontage,
			InstanceData.PlayRate,
			InstanceData.StartSectionName);

	/*
	 * PlayAnimMontage()는 재생 실패 시 0을 반환한다.
	 */
	if (MontageDuration <= 0.0f)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.PlayingAnimInstance = AnimInstance;
	InstanceData.bMontageStarted = true;

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTPlayMontageTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	if (!InstanceData.bMontageStarted)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!IsValid(InstanceData.ControlledPawn) ||
		!IsValid(InstanceData.AttackMontage))
	{
		return EStateTreeRunStatus::Failed;
	}

	UAnimInstance* AnimInstance =
		InstanceData.PlayingAnimInstance.Get();

	if (!IsValid(AnimInstance))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	if (AnimInstance->Montage_IsPlaying(
		InstanceData.AttackMontage))
	{
		return EStateTreeRunStatus::Running;
	}
	
	return EStateTreeRunStatus::Succeeded;
}

void FSTTPlayMontageTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	UAnimInstance* AnimInstance =
		InstanceData.PlayingAnimInstance.Get();

	/*
	 * 정상 종료된 Montage는 이미 재생 중이 아니므로
	 * 여기서는 아무것도 중단되지 않는다.
	 *
	 * Target 소실이나 상위 State 전환으로 강제 종료된 경우에만
	 * 재생 중인 Montage가 중단된다.
	 */
	if (InstanceData.bStopMontageOnExit &&
		IsValid(AnimInstance) &&
		IsValid(InstanceData.AttackMontage) &&
		AnimInstance->Montage_IsPlaying(
			InstanceData.AttackMontage))
	{
		AnimInstance->Montage_Stop(
			InstanceData.MontageBlendOutTime,
			InstanceData.AttackMontage);
	}

	if (IsValid(InstanceData.ControlledPawn))
	{
		AAIController* AIController =
			Cast<AAIController>(
				InstanceData.ControlledPawn->GetController());
	}

	InstanceData.PlayingAnimInstance.Reset();
	InstanceData.bMontageStarted = false;
}