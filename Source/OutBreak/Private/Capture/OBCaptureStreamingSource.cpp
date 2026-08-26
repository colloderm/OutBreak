// Fill out your copyright notice in the Description page of Project Settings.

#include "Capture/OBCaptureStreamingSource.h"

#include "Components/SceneComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"

AOBCaptureStreamingSource::AOBCaptureStreamingSource()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	// ★ 이게 없으면 이 액터 자신이 WP에 의해 언로드된다. 플레이어가 멀어지는 순간
	//   스트리밍 소스가 사라져서, 정확히 필요한 상황에 무력해진다.
	//
	// 런타임에 값이 없어도 문제없다 — 이 플래그는 쿠킹 시점에 WP 스트리밍 데이터로 구워지고, 패키징된 빌드는 그 결과만 읽는다.
	bIsSpatiallyLoaded = false;
#endif

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("CaptureStreamingSource"));
	StreamingSource->Priority = EStreamingSourcePriority::Highest;
	StreamingSource->DebugColor = FColor::Cyan;   // wp.Runtime.ToggleDrawRuntimeHash2D 에서 구분용
	StreamingSource->DisableStreamingSource();
}

void AOBCaptureStreamingSource::BeginPlay()
{
	Super::BeginPlay();

	if (!bEnabled || !StreamingSource)
	{
		return;
	}

	// bUseGridLoadingRange=false 로 두어야 아래 Radius가 실제로 쓰인다.
	// true면 그리드 기본 로딩 범위를 따라가 이 값이 무시된다.
	FStreamingSourceShape Shape;
	Shape.bUseGridLoadingRange = false;
	Shape.Radius = FMath::Max(1000.f, Radius);
	Shape.bIsSector = false;
	Shape.Location = FVector::ZeroVector;

	StreamingSource->Shapes.Reset();
	StreamingSource->Shapes.Add(Shape);
	StreamingSource->EnableStreamingSource();

	UE_LOG(LogTemp, Log, TEXT("[Capture] 스트리밍 소스 활성 %s 반경=%.0f"),
		*GetActorLocation().ToCompactString(), Shape.Radius);
}
