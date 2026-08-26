// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBCaptureStreamingSource.generated.h"

class USceneComponent;
class UWorldPartitionStreamingSourceComponent;

/**
 * 트레일러 촬영용 World Partition 강제 로딩 액터.
 *
 * 왜 필요한가:
 * - WP의 스트리밍 소스는 카메라가 아니라 플레이어 폰이다. `pause` + `ToggleDebugCamera`로
 *   카메라만 상공에 올리면 아래 도시는 언로드된 빈 땅으로 찍힌다(콘티 A2, C3, E2, G2).
 * - 레벨에 이 액터를 놓아 두면 플레이어 위치와 무관하게 그 반경이 계속 로드된다.
 *
 * 왜 AOBInsertionTargetStreamingProxy를 안 쓰는가:
 * - 그쪽은 UCLASS(Transient)라 레벨에 배치해도 저장되지 않고, 스트리밍도 C++의
 *   Configure() 호출로만 켜진다. 배치용으로는 쓸 수 없다.
 *
 * 적용 범위: PIE / 스탠드얼론에서만 동작한다. 에디터 뷰포트의 WP 로딩은
 * 스트리밍 소스가 아니라 World Partition 창의 Loaded Region이 결정한다.
 *
 * ★ 촬영이 끝나면 레벨에서 제거할 것. 남기면 그 반경이 런타임 내내 메모리에 상주한다.
 */
UCLASS()
class OUTBREAK_API AOBCaptureStreamingSource : public AActor
{
	GENERATED_BODY()

public:
	AOBCaptureStreamingSource();

protected:
	virtual void BeginPlay() override;

	// 강제로 로드해 둘 반경(cm). 와이드샷이 담는 범위보다 넉넉해야 한다.
	// 초고공 와이드(A2)는 50000(500m)부터 시험할 것. 넓힐수록 메모리를 먹는다.
	UPROPERTY(EditAnywhere, Category = "Capture", meta = (ClampMin = "1000.0"))
	float Radius = 50000.f;

	// 끄면 배치해 둔 채로 무력화된다. 컷마다 켜고 끄며 비교할 때 쓴다.
	UPROPERTY(EditAnywhere, Category = "Capture")
	bool bEnabled = true;

	UPROPERTY(VisibleAnywhere, Category = "Capture")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Capture")
	TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;
};
