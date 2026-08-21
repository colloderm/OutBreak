#include "AI/StateTree/Evaluator/ST_EvaluateEnemyAIContext.h"

#include "AI/Components/EnemyMemoryComponent.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyController.h"
#include "StateTreeExecutionContext.h"

void FSTEvaluateEnemyAIContext::TreeStart(
	FStateTreeExecutionContext& Context) const
{
	UpdateContext(Context);
}

void FSTEvaluateEnemyAIContext::Tick(
	FStateTreeExecutionContext& Context,
	const float /*DeltaTime*/) const
{
	UpdateContext(Context);
}

void FSTEvaluateEnemyAIContext::UpdateContext(
	FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	AEnemyCharacter* EnemyCharacter =
		InstanceData.ContextActor.Get();

	if (!IsValid(EnemyCharacter))
	{
		ClearMemoryState(InstanceData);
		ClearPhysicalState(InstanceData);
		return;
	}

	SynchronizePhysicalState(
		InstanceData,
		*EnemyCharacter);

	UEnemyMemoryComponent* MemoryComponent =
		ResolveMemoryComponent(
			InstanceData,
			EnemyCharacter);

	if (!IsValid(MemoryComponent))
	{
		ClearMemoryState(InstanceData);
		return;
	}

	if (!SynchronizeMemoryState(
		InstanceData,
		*MemoryComponent))
	{
		return;
	}

	AActor* TargetActor =
		InstanceData.TargetActor.Get();

	if (!IsValid(TargetActor))
	{
		ClearTargetState(InstanceData);
		return;
	}

	UpdateSpatialState(
		InstanceData,
		*EnemyCharacter,
		*TargetActor);
}

UEnemyMemoryComponent*
FSTEvaluateEnemyAIContext::ResolveMemoryComponent(
	FInstanceDataType& InstanceData,
	AEnemyCharacter* EnemyCharacter)
{
	UEnemyMemoryComponent* MemoryComponent =
		InstanceData.MemoryComponent.Get();

	if (IsValid(MemoryComponent))
	{
		return MemoryComponent;
	}

	AEnemyController* EnemyController =
		InstanceData.AIController.Get();

	if (!IsValid(EnemyController) && IsValid(EnemyCharacter))
	{
		EnemyController =
			Cast<AEnemyController>(
				EnemyCharacter->GetController());
		InstanceData.AIController = EnemyController;
	}

	if (!IsValid(EnemyController))
	{
		return nullptr;
	}

	MemoryComponent =
		EnemyController->GetEnemyMemoryComponent();
	InstanceData.MemoryComponent = MemoryComponent;

	return MemoryComponent;
}

void FSTEvaluateEnemyAIContext::SynchronizePhysicalState(
	FInstanceDataType& InstanceData,
	const AEnemyCharacter& EnemyCharacter)
{
	InstanceData.LocomotionState =
		EnemyCharacter.GetLocomotionWalkRunState();
	InstanceData.MissingArmState =
		EnemyCharacter.GetMissingArmState();
	InstanceData.ActionState =
		EnemyCharacter.GetActionState();
}

bool FSTEvaluateEnemyAIContext::SynchronizeMemoryState(
	FInstanceDataType& InstanceData,
	const UEnemyMemoryComponent& MemoryComponent)
{
	InstanceData.bHasActionableStimulus =
		MemoryComponent.HasActionableStimulus();
	InstanceData.StimulusType =
		MemoryComponent.GetStimulusType();
	InstanceData.LastStimulusLocation =
		MemoryComponent.GetLastStimulusLocation();
	InstanceData.bHasHearingStimulus =
		MemoryComponent.HasHearingStimulus();
	InstanceData.LastHeardLocation =
		MemoryComponent.GetLastHeardLocation();
	InstanceData.bHasDamageStimulus =
		MemoryComponent.HasDamageStimulus();
	InstanceData.LastDamageDirection =
		MemoryComponent.GetLastDamageDirection();

	if (!MemoryComponent.HasValidTarget())
	{
		ClearTargetState(InstanceData);
		return false;
	}

	InstanceData.TargetActor =
		MemoryComponent.GetTargetActor();
	InstanceData.LastKnownTargetLocation =
		MemoryComponent.GetLastKnownTargetLocation();
	InstanceData.bHasValidTarget = true;
	InstanceData.bTargetVisible =
		MemoryComponent.IsTargetVisible();
	InstanceData.TimeSinceTargetSeen =
		MemoryComponent.GetTimeSinceTargetSeen();

	return true;
}

void FSTEvaluateEnemyAIContext::UpdateSpatialState(
	FInstanceDataType& InstanceData,
	const AEnemyCharacter& EnemyCharacter,
	const AActor& TargetActor)
{
	const FVector EvaluationTargetLocation =
		InstanceData.bTargetVisible
			? TargetActor.GetActorLocation()
			: InstanceData.LastKnownTargetLocation;

	const FVector SelfToTarget =
		EvaluationTargetLocation -
		EnemyCharacter.GetActorLocation();

	InstanceData.TargetDistance =
		SelfToTarget.Size2D();
	InstanceData.TargetHeightDifference =
		SelfToTarget.Z;
	InstanceData.bTargetInAttackRange =
		InstanceData.bTargetVisible &&
		InstanceData.TargetDistance <=
			FMath::Max(0.0f, InstanceData.AttackRange);
}

void FSTEvaluateEnemyAIContext::ClearTargetState(
	FInstanceDataType& InstanceData)
{
	InstanceData.TargetActor = nullptr;
	InstanceData.LastKnownTargetLocation = FVector::ZeroVector;
	InstanceData.bHasValidTarget = false;
	InstanceData.bTargetVisible = false;
	InstanceData.bTargetInAttackRange = false;
	InstanceData.TargetDistance = 0.0f;
	InstanceData.TargetHeightDifference = 0.0f;
	InstanceData.TimeSinceTargetSeen = 0.0f;
}

void FSTEvaluateEnemyAIContext::ClearMemoryState(
	FInstanceDataType& InstanceData)
{
	ClearTargetState(InstanceData);
	InstanceData.MemoryComponent = nullptr;
	InstanceData.bHasActionableStimulus = false;
	InstanceData.StimulusType = EEnemyStimulusType::None;
	InstanceData.LastStimulusLocation = FVector::ZeroVector;
	InstanceData.bHasHearingStimulus = false;
	InstanceData.LastHeardLocation = FVector::ZeroVector;
	InstanceData.bHasDamageStimulus = false;
	InstanceData.LastDamageDirection = FVector::ZeroVector;
}

void FSTEvaluateEnemyAIContext::ClearPhysicalState(
	FInstanceDataType& InstanceData)
{
	InstanceData.LocomotionState =
		ELocomotionWalkRunState::Dead;
	InstanceData.MissingArmState =
		EEnemyMissingArmState::None;
	InstanceData.ActionState =
		EEnemyActionState::Dead;
}
