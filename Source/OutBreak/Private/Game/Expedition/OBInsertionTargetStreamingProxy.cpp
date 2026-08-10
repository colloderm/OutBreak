#include "Game/Expedition/OBInsertionTargetStreamingProxy.h"

#include "Components/SceneComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"

AOBInsertionTargetStreamingProxy::AOBInsertionTargetStreamingProxy()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("InsertionTargetStreamingSource"));
	StreamingSource->Priority = EStreamingSourcePriority::Highest;
	StreamingSource->DebugColor = FColor::Orange;
	StreamingSource->DisableStreamingSource();
}

void AOBInsertionTargetStreamingProxy::Configure(const FVector& WorldLocation, float Radius)
{
	StreamingRadius = FMath::Max(1000.f, Radius);
	SetActorLocation(WorldLocation, false, nullptr, ETeleportType::TeleportPhysics);

	FStreamingSourceShape Shape;
	Shape.bUseGridLoadingRange = false;
	Shape.Radius = StreamingRadius;
	Shape.bIsSector = false;
	Shape.Location = FVector::ZeroVector;
	StreamingSource->Shapes.Reset();
	StreamingSource->Shapes.Add(Shape);
	StreamingSource->EnableStreamingSource();
}

bool AOBInsertionTargetStreamingProxy::IsTargetStreamingCompleted() const
{
	return StreamingSource && StreamingSource->IsStreamingSourceEnabled()
		&& StreamingSource->IsStreamingCompleted();
}

void AOBInsertionTargetStreamingProxy::DeactivateStreaming()
{
	if (StreamingSource)
	{
		StreamingSource->DisableStreamingSource();
	}
}
