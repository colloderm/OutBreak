// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "UtlitesTask.generated.h"


class UCharacterMovement;
/**
 * 
 */
USTRUCT()
struct OUTBREAK_API FCharacterMovementModifyTaskInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(
		EditAnywhere,
		Category = "Context")
	TObjectPtr<UCharacterMovement> CharacterMovement;
};

USTRUCT(meta = (DisplayName = "Task_CharacterMovementModify"), Category = "OutBreak|AI")
struct OUTBREAK_API FCharacterMovementModifyTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	
	FCharacterMovementModifyTask();
	
	using FInstanceDataType = FCharacterMovementModifyTaskInstanceData;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	
	virtual EStateTreeRunStatus EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

};
