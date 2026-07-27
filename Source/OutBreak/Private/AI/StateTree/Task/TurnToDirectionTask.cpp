#include "AI/StateTree/Task/TurnToDirectionTask.h"

#include "AIController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTTTurnToDirectionTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.ControlledPawn))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.bStopMovementOnEnter)
	{
		if (AAIController* AIController =
			Cast<AAIController>(
				InstanceData.ControlledPawn->GetController()))
		{
			AIController->StopMovement();
		}
	}

	return UpdateRotation(InstanceData, 0.0f);
}

EStateTreeRunStatus FSTTTurnToDirectionTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	return UpdateRotation(InstanceData, DeltaTime);
}

EStateTreeRunStatus FSTTTurnToDirectionTask::UpdateRotation(
	FInstanceDataType& InstanceData,
	const float DeltaTime)
{
	APawn* ControlledPawn = InstanceData.ControlledPawn.Get();
	if (!IsValid(ControlledPawn))
	{
		return EStateTreeRunStatus::Failed;
	}

	FVector Direction = InstanceData.Direction;
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		return EStateTreeRunStatus::Failed;
	}

	const float CurrentYaw = ControlledPawn->GetActorRotation().Yaw;
	const float TargetYaw = Direction.Rotation().Yaw;
	const float Tolerance = FMath::Clamp(
		InstanceData.FacingTolerance,
		0.0f,
		180.0f);
	const float RemainingYaw =
		FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);

	float NewYaw = CurrentYaw;
	if (FMath::Abs(RemainingYaw) <= Tolerance ||
		InstanceData.RotationSpeed <= 0.0f)
	{
		NewYaw = TargetYaw;
	}
	else
	{
		const float MaxYawStep =
			InstanceData.RotationSpeed *
			FMath::Max(0.0f, DeltaTime);
		NewYaw = FMath::FixedTurn(CurrentYaw, TargetYaw, MaxYawStep);
	}

	ControlledPawn->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
	if (AController* Controller = ControlledPawn->GetController())
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Yaw = NewYaw;
		Controller->SetControlRotation(ControlRotation);
	}

	return FMath::Abs(
		FMath::FindDeltaAngleDegrees(NewYaw, TargetYaw)) <= Tolerance
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}
