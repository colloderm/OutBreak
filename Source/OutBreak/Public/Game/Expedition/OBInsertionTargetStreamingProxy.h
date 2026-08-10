#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBInsertionTargetStreamingProxy.generated.h"

class USceneComponent;
class UWorldPartitionStreamingSourceComponent;

/**
 * Transient server-side streaming source used while an insertion point is being
 * validated. It keeps the selected region loaded independently of the helicopter.
 */
UCLASS(NotBlueprintable, Transient)
class OUTBREAK_API AOBInsertionTargetStreamingProxy : public AActor
{
	GENERATED_BODY()

public:
	AOBInsertionTargetStreamingProxy();

	void Configure(const FVector& WorldLocation, float Radius);
	bool IsTargetStreamingCompleted() const;
	void DeactivateStreaming();

	float GetStreamingRadius() const { return StreamingRadius; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;

	float StreamingRadius = 10000.f;
};
