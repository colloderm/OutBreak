// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlowFieldGoalActor.generated.h"

/** 에디터에서 Flow Field의 목적지를 지정하고 재계산하기 위한 테스트 Actor입니다. */
UCLASS()
class OUTBREAK_API AFlowFieldGoalActor : public AActor
{
	GENERATED_BODY()

public:
	AFlowFieldGoalActor();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "FlowField")
	void RebuildFlowField();
};
