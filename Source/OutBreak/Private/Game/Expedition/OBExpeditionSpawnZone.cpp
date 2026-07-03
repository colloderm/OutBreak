// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Expedition/OBExpeditionSpawnZone.h"

#include "NavigationSystem.h"
#include "Components/BillboardComponent.h"


AOBExpeditionSpawnZone::AOBExpeditionSpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	
#if WITH_EDITORONLY_DATA
	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(Root);
#endif
}

FTransform AOBExpeditionSpawnZone::GetScatteredSpawnTransform() const
{
	const FVector Center = GetActorLocation();
	FVector Chosen = Center;
	
	// 네비메시 위 랜덤 지점 -> 파티원이 벽/공중/장애물에 스폰되는 것 방지
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Out;
		if (Nav->GetRandomReachablePointInRadius(Center, ScatterRadius, Out))
		{
			Chosen = Out.Location;
		}
	}
	
	// 바닥에서 살짝 띄워 캡슐 하단 관통 방지
	Chosen.Z += 100.f;
	return FTransform(GetActorRotation(), Chosen);
}


