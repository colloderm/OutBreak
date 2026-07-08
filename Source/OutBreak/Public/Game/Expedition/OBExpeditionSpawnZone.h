// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBExpeditionSpawnZone.generated.h"

/**
 * 파티 스폰 지점(존). 레벨에 여러 개 배치하고 서로 멀리 떨어뜨린다(3~5분 도보).
 * - 세션 시작 시 GameMode가 파티별로 랜덤 배정.
 * - 파티원은 이 존 반경 내 네비 위 랜덤 지점에 스폰.
 */
UCLASS()
class OUTBREAK_API AOBExpeditionSpawnZone : public AActor
{
	GENERATED_BODY()

public:
	AOBExpeditionSpawnZone();
	
	// 파티원 산개 반경(cm). 존 중심에서 설정 반경 내 네비 지점에 흩뿌리기
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float ScatterRadius = 800.f;
	
	// 존 반경 내 네비메시 위 랜덤 스폰 트랜스폼 반환
	FTransform GetScatteredSpawnTransform() const;
	
protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	
};
