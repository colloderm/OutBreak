// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "InvestingTask.generated.h"


USTRUCT()
struct OUTBREAK_API FSTTInvestingTaskInstanceData
{
	GENERATED_BODY()
	
	
	UPROPERTY(EditAnywhere,
		Category= "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;
	
	UPROPERTY(
		EditAnywhere,
		Category = "Input")
	FVector LastPerceptionLocation = FVector::ZeroVector;
	
protected:
	UPROPERTY()
	TOptional<FVector> InvestingLocation = FVector::ZeroVector;
	
	friend class FSTTInvestingTask;
	
	
};


USTRUCT(meta = (DisplayName = "Task_Invensting"), Category = "OutBreak|AI")
struct OUTBREAK_API FSTTInvestingTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	
	
	using FInstanceDataType = FSTTInvestingTaskInstanceData;
	
	FSTTInvestingTask();
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};
