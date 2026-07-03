// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/FlowFieldGoalActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FlowField/FlowFieldRecastNavMesh.h"

AFlowFieldGoalActor::AFlowFieldGoalActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AFlowFieldGoalActor::RebuildFlowField()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 현재 월드의 FlowField Recast NavMesh를 찾아 Goal Actor 위치로 다시 계산합니다.
	for (TActorIterator<AFlowFieldRecastNavMesh> It(World); It; ++It)
	{
		It->BuildFlowField(GetActorLocation());
		return;
	}
}
