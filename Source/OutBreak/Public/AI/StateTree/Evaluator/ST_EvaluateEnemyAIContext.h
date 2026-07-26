// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "ST_EvaluateEnemyAIContext.generated.h"


class AActor;
class AAIController;

/**
 * 
 */
USTRUCT()
struct OUTBREAK_API FSTEvaluateEnemyAIContextInstanceData
{
	GENERATED_BODY()
	/*
	 * StateTree AI Component Schema의 Actor Context와 바인딩.
	 *
	 * 실제 실행 시에는 AIController가 제어 중인 Pawn입니다.
	 * Schema의 원본 타입에 맞춰 AActor로 받고 내부에서 APawn으로 변환합니다.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Context")
	TObjectPtr<AActor> ContextActor = nullptr;

	/*
	 * StateTree AI Component Schema의 AIController Context와 바인딩.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/*
	 * 공격 가능 거리.
	 *
	 * 직접 설정할 수도 있고 StateTree Root Parameter와
	 * 바인딩할 수도 있습니다.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float AttackRange = 150.0f;

	/*
	 * Target이 시야에서 사라진 뒤 기억하는 시간.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float ForgetTargetTime = 5.0f;

	/*
	 * 현재 추적 중인 Target.
	 *
	 * 시야에서 잠시 사라져도 ForgetTargetTime 동안 유지됩니다.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter")
	TObjectPtr<AActor> TargetActor = nullptr;

	/*
	 * Target을 마지막으로 확인한 위치.
	 *
	 * Target이 시야에서 사라진 뒤에는 실제 Target 위치를 읽지 않고
	 * 이 위치를 사용해야 AI가 플레이어 위치를 투시하지 않습니다.
	 */
	UPROPERTY(
		VisibleAnywhere,
		Category = "Output")
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(
		VisibleAnywhere,
		Category = "Output")
	bool bHasValidTarget = false;

	UPROPERTY(
		VisibleAnywhere,
		Category = "Output")
	bool bTargetVisible = false;

	UPROPERTY(
		VisibleAnywhere,
		Category = "Output")
	bool bTargetInAttackRange = false;

	/*
	 * Pawn에서 Target 마지막 확인 위치까지의 거리.
	 */
	UPROPERTY(
		VisibleAnywhere,
		Category = "Output")
	float TargetDistance = 0.0f;

	/*
	 * Target 위치 Z - Pawn 위치 Z.
	 *
	 * 양수이면 Target이 Pawn보다 위에 있고,
	 * 음수이면 Target이 Pawn보다 아래에 있습니다.
	 */
	UPROPERTY(
		VisibleAnywhere,
		Category = "Output")
	float TargetHeightDifference = 0.0f;

	/*
	 * Target을 마지막으로 본 뒤 지난 시간.
	 */
	UPROPERTY(
		VisibleAnywhere,
		Category = "Output")
	float TimeSinceTargetSeen = 0.0f;
	
};


USTRUCT(
	meta = (
		DisplayName = "Evaluate Enemy AI Context",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FSTEvaluateEnemyAIContext
	: public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()
	
	using FInstanceDataType =
		FSTEvaluateEnemyAIContextInstanceData;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}


private:
	void UpdateContext(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const;

	static void ClearTarget(
		FInstanceDataType& InstanceData);
};
