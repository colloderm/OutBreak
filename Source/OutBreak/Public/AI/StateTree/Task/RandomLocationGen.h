// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "RandomLocationGen.generated.h"

class APawn;

USTRUCT()
struct OUTBREAK_API FRandomLocationGenInstanceData
{
	GENERATED_BODY()

	/** 랜덤 위치를 생성할 기준 캐릭터입니다. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;

	/** 캐릭터의 현재 위치를 중심으로 검색할 원의 반지름입니다. */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SearchRadius = 600.0f;

	/** 다음 Move To Task의 Destination에 바인딩할 랜덤 이동 위치입니다. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FVector RandomLocation = FVector::ZeroVector;
};

/**
 * 현재 캐릭터 주변의 NavMesh에서 도달 가능한 랜덤 위치를 하나 생성합니다.
 * 이동은 수행하지 않으며, RandomLocation을 다음 Move To Task에 전달합니다.
 */
USTRUCT(
	meta = (
		DisplayName = "Generate Random Reachable Location",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FRandomLocationGen : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRandomLocationGenInstanceData;

	FRandomLocationGen();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
