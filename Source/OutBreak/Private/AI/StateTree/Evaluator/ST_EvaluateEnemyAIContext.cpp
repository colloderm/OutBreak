// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/StateTree/Evaluator/ST_EvaluateEnemyAIContext.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "StateTreeExecutionContext.h"


void FSTEvaluateEnemyAIContext::TreeStart(
	FStateTreeExecutionContext& Context) const
{
	/*
	 * StateTree 시작 직후에도 Output이 유효하도록
	 * 최초 평가를 한 번 수행합니다.
	 */
	UpdateContext(Context, 0.0f);
}


void FSTEvaluateEnemyAIContext::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	UpdateContext(Context, DeltaTime);
}


void FSTEvaluateEnemyAIContext::UpdateContext(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	APawn* ControlledPawn =
		Cast<APawn>(InstanceData.ContextActor.Get());

	AAIController* AIController =
		InstanceData.AIController.Get();

	if (!IsValid(ControlledPawn) ||
		!IsValid(AIController))
	{
		ClearTarget(InstanceData);
		return;
	}

	UAIPerceptionComponent* PerceptionComponent =
		AIController->GetAIPerceptionComponent();

	if (!IsValid(PerceptionComponent))
	{
		ClearTarget(InstanceData);
		return;
	}

	const FVector PawnLocation =
		ControlledPawn->GetActorLocation();

	/*
	 * 현재 Sight Sense가 성공적으로 감지 중인 Actor들을 가져옵니다.
	 */
	TArray<AActor*> CurrentlySeenActors;

	PerceptionComponent->GetCurrentlyPerceivedActors(
		UAISense_Sight::StaticClass(),
		CurrentlySeenActors);

	AActor* ClosestTarget = nullptr;
	double ClosestDistanceSquared =
		TNumericLimits<double>::Max();

	for (AActor* Candidate : CurrentlySeenActors)
	{
		if (!IsValid(Candidate) ||
			Candidate == ControlledPawn)
		{
			continue;
		}

		/*
		 * 실제 프로젝트에서는 이 위치에 Team 판별,
		 * 사망 여부, Target 가능 인터페이스 등을 추가합니다.
		 *
		 * 예:
		 *
		 * if (!Candidate->Implements<UTargetableInterface>())
		 * {
		 *     continue;
		 * }
		 */

		const double DistanceSquared =
			FVector::DistSquared(
				PawnLocation,
				Candidate->GetActorLocation());

		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestTarget = Candidate;
		}
	}

	if (IsValid(ClosestTarget))
	{
		/*
		 * Target이 현재 보이는 경우.
		 */
		InstanceData.TargetActor = ClosestTarget;

		InstanceData.LastKnownTargetLocation =
			ClosestTarget->GetActorLocation();

		InstanceData.bHasValidTarget = true;
		InstanceData.bTargetVisible = true;
		InstanceData.TimeSinceTargetSeen = 0.0f;
	}
	else
	{
		/*
		 * 현재 보이는 Target이 없는 경우.
		 *
		 * 기존 Target을 즉시 삭제하지 않고
		 * ForgetTargetTime 동안 마지막 위치를 기억합니다.
		 */
		InstanceData.bTargetVisible = false;

		if (!IsValid(InstanceData.TargetActor))
		{
			ClearTarget(InstanceData);
			return;
		}

		InstanceData.TimeSinceTargetSeen +=
			FMath::Max(DeltaTime, 0.0f);

		if (InstanceData.TimeSinceTargetSeen >=
			InstanceData.ForgetTargetTime)
		{
			ClearTarget(InstanceData);
			return;
		}

		InstanceData.bHasValidTarget = true;
	}

	/*
	 * Target이 보이지 않을 때 TargetActor의 현재 위치를 읽으면
	 * AI가 보이지 않는 플레이어를 계속 추적하는 문제가 생깁니다.
	 *
	 * 따라서 거리와 높이 계산은 항상
	 * LastKnownTargetLocation을 기준으로 수행합니다.
	 */
	const FVector PawnToTarget =
		InstanceData.LastKnownTargetLocation -
		PawnLocation;

	InstanceData.TargetDistance =
		PawnToTarget.Size();

	InstanceData.TargetHeightDifference =
		PawnToTarget.Z;

	/*
	 * 단순히 거리만 가까운 것이 아니라,
	 * 현재 Target이 보일 때만 공격 사거리로 판정합니다.
	 */
	InstanceData.bTargetInAttackRange =
		InstanceData.bTargetVisible &&
		InstanceData.TargetDistance <=
			InstanceData.AttackRange;
}


void FSTEvaluateEnemyAIContext::ClearTarget(
	FInstanceDataType& InstanceData)
{
	InstanceData.TargetActor = nullptr;

	InstanceData.LastKnownTargetLocation =
		FVector::ZeroVector;

	InstanceData.bHasValidTarget = false;
	InstanceData.bTargetVisible = false;
	InstanceData.bTargetInAttackRange = false;

	InstanceData.TargetDistance = 0.0f;
	InstanceData.TargetHeightDifference = 0.0f;
	InstanceData.TimeSinceTargetSeen = 0.0f;
}