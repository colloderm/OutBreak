// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "InvestigatingTask.generated.h"


class APawn;
class UNavigationQueryFilter;

USTRUCT()
struct OUTBREAK_API FSTTRandomLocationTaskInstanceData
{
	GENERATED_BODY()
	
	
	UPROPERTY(EditAnywhere,
		Category= "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;
	
	UPROPERTY(
		EditAnywhere,
		Category = "Evaluator")
	FVector LastPerceptionLocation = FVector::ZeroVector;
	
	
	UPROPERTY(
		EditAnywhere,
		Category = "Evaluator")
	bool bHasLastPerceptionLocation = false;
	
	/*
	 * 자극 위치를 중심으로 자사할 반경
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Paramter",
		meta = (ClampMin = "0.0"))
	float InvestigationRadius = 600.f;
	
	UPROPERTY(
		EditAnywhere,
		Category = "Paramter",
		meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 80.f;
	
	/*
	 * 몇 개의 지점을 방문하면 조사를 종료할지
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Paramter",
		meta = (ClampMin = "1"))
	int MaxInvestigationPoints = 4;
	
	/*
	 *  경로 요청 실패 시 매 프레임 재요청하지 않기 위한 간격
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Paramter",
		meta = (ClampMin = "0.0"))
	float RetryInterval = 0.25f;
	
	UPROPERTY(
		EditAnywhere,
		Category = "Paramter")
	TSubclassOf<UNavigationQueryFilter> NavigationFilter;
	
	
protected:
	UPROPERTY()
	FVector CurrentInvestigationLocation = FVector::ZeroVector;
	
	UPROPERTY()
	bool bMoveRequested = false;
	
	UPROPERTY()
	int CompletedPointCount = 0;
	
	UPROPERTY()
	float RetryTimeRemaining = 0.f;
	
	friend struct FSTTRandomLocationTask;
};


USTRUCT(meta = (DisplayName = "Task_RnadomLocation"), Category = "OutBreak|AI")
struct OUTBREAK_API FSTTRandomLocationTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	
	
	using FInstanceDataType = FSTTRandomLocationTaskInstanceData;
	
	FSTTRandomLocationTask();
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
private:
	EStateTreeRunStatus RequestNextInvestigationMove(FInstanceDataType& InstanceData) const;
};
