
#include "AI/StateTree/Task/PlayMontageTask.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTTPlayMontageTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	InstanceData.bMontageStarted = false;
	InstanceData.PlayingAnimInstance.Reset();

	ACharacter* Character = InstanceData.ControlledPawn;

	if (!IsValid(Character) ||
		!IsValid(InstanceData.TargetActor) ||
		!IsValid(InstanceData.AttackMontage))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!IsTargetInAttackRange(InstanceData))
	{
		return EStateTreeRunStatus::Failed;
	}

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

		if (InstanceData.bSetFocusOnTarget)
		{
			AIController->SetFocus(
				InstanceData.TargetActor,
				EAIFocusPriority::Gameplay);
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
		if (IsValid(AIController) &&
			InstanceData.bSetFocusOnTarget)
		{
			AIController->ClearFocus(
				EAIFocusPriority::Gameplay);
		}

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
		!IsValid(InstanceData.TargetActor) ||
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

	/*
	 * 공격 중 Target이 사라지거나 너무 멀어졌으면 공격 중단.
	 *
	 * 실제 게임에서는 AttackRange보다 조금 큰
	 * AttackAbortRange를 따로 두는 것이 더 안정적이다.
	 */
	if (!IsTargetInAttackRange(InstanceData))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AnimInstance->Montage_IsPlaying(
		InstanceData.AttackMontage))
	{
		return EStateTreeRunStatus::Running;
	}

	/*
	 * Montage 재생이 끝났으므로 Attack State 완료.
	 */
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

		if (IsValid(AIController) &&
			InstanceData.bSetFocusOnTarget)
		{
			AIController->ClearFocus(
				EAIFocusPriority::Gameplay);
		}
	}

	InstanceData.PlayingAnimInstance.Reset();
	InstanceData.bMontageStarted = false;
}

bool FSTTPlayMontageTask::IsTargetInAttackRange(
	const FInstanceDataType& InstanceData) const
{
	if (!IsValid(InstanceData.ControlledPawn) ||
		!IsValid(InstanceData.TargetActor))
	{
		return false;
	}

	const float AttackRangeSquared =
		FMath::Square(InstanceData.AttackRange);

	const float DistanceSquared =
		FVector::DistSquared(
			InstanceData.ControlledPawn->GetActorLocation(),
			InstanceData.TargetActor->GetActorLocation());

	return DistanceSquared <= AttackRangeSquared;
}