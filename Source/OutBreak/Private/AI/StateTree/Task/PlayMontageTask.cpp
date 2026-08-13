
#include "AI/StateTree/Task/PlayMontageTask.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "AI/EnemyCharacter.h"
#include "GameFramework/Controller.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTTPlayMontageTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	InstanceData.bMontageStarted = false;
	InstanceData.bWaitingForTargetRotation = false;
	InstanceData.PlayingAnimInstance.Reset();

	AEnemyCharacter* Character = InstanceData.ControlledPawn;

	if (!IsValid(Character) ||
		!IsValid(InstanceData.AttackMontage) ||
		!Character->CanAct())
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
	}

	InstanceData.PlayingAnimInstance = AnimInstance;

	if (InstanceData.bRotateToTargetBeforePlaying &&
		IsValid(InstanceData.TargetActor))
	{
		InstanceData.bWaitingForTargetRotation = true;
		if (!RotateTowardTarget(InstanceData, 0.0f))
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.bWaitingForTargetRotation = false;
	}

	return StartMontage(InstanceData);
}

EStateTreeRunStatus FSTTPlayMontageTask::StartMontage(
	FInstanceDataType& InstanceData) const
{
	AEnemyCharacter* Character = InstanceData.ControlledPawn.Get();
	if (!IsValid(Character) ||
		!IsValid(InstanceData.AttackMontage) ||
		!IsValid(InstanceData.PlayingAnimInstance.Get()) ||
		!Character->CanAct())
	{
		return EStateTreeRunStatus::Failed;
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
	
	// 서버에서만 재생됐다. 클라에도 같은 몽타주를 보낸다.
	Character->Multicast_PlayMontage(
		InstanceData.AttackMontage,
		InstanceData.PlayRate,
		InstanceData.StartSectionName);

	InstanceData.bMontageStarted = true;

	return EStateTreeRunStatus::Running;
}

bool FSTTPlayMontageTask::RotateTowardTarget(
	FInstanceDataType& InstanceData,
	const float DeltaTime) const
{
	AEnemyCharacter* Character = InstanceData.ControlledPawn.Get();
	AActor* TargetActor = InstanceData.TargetActor.Get();
	if (!IsValid(Character) || !IsValid(TargetActor))
	{
		return true;
	}

	FVector TargetDirection =
		TargetActor->GetActorLocation() - Character->GetActorLocation();
	TargetDirection.Z = 0.0f;
	if (!TargetDirection.Normalize())
	{
		return true;
	}

	const float CurrentYaw = Character->GetActorRotation().Yaw;
	const float TargetYaw = TargetDirection.Rotation().Yaw;
	const float Tolerance = FMath::Clamp(
		InstanceData.TargetFacingTolerance,
		0.0f,
		180.0f);
	const float RemainingYaw =
		FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);

	float NewYaw = CurrentYaw;
	if (FMath::Abs(RemainingYaw) <= Tolerance ||
		InstanceData.TargetRotationSpeed <= 0.0f)
	{
		NewYaw = TargetYaw;
	}
	else
	{
		const float MaxYawStep =
			InstanceData.TargetRotationSpeed *
			FMath::Max(0.0f, DeltaTime);
		NewYaw = FMath::FixedTurn(CurrentYaw, TargetYaw, MaxYawStep);
	}

	Character->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
	if (AController* Controller = Character->GetController())
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Yaw = NewYaw;
		Controller->SetControlRotation(ControlRotation);
	}

	return FMath::Abs(
		FMath::FindDeltaAngleDegrees(NewYaw, TargetYaw)) <= Tolerance;
}

EStateTreeRunStatus FSTTPlayMontageTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	if (InstanceData.bWaitingForTargetRotation)
	{
		if (!IsValid(InstanceData.ControlledPawn) ||
			!IsValid(InstanceData.AttackMontage) ||
			!IsValid(InstanceData.PlayingAnimInstance.Get()) ||
			!InstanceData.ControlledPawn->CanAct())
		{
			return EStateTreeRunStatus::Failed;
		}

		if (IsValid(InstanceData.TargetActor) &&
			!RotateTowardTarget(InstanceData, DeltaTime))
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.bWaitingForTargetRotation = false;
		return StartMontage(InstanceData);
	}

	if (!InstanceData.bMontageStarted)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!IsValid(InstanceData.ControlledPawn) ||
		!IsValid(InstanceData.AttackMontage) ||
		!InstanceData.ControlledPawn->CanAct())
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
	InstanceData.bWaitingForTargetRotation = false;
}
