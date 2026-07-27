#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "TurnToDirectionTask.generated.h"

class APawn;

USTRUCT()
struct OUTBREAK_API FSTTTurnToDirectionTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;

	/** World-space direction to face. Only X/Y are used. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector Direction = FVector::ForwardVector;

	/** Yaw rotation speed. A value of zero rotates instantly. */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float RotationSpeed = 360.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingTolerance = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bStopMovementOnEnter = true;
};

USTRUCT(
	meta = (
		DisplayName = "Turn To Direction",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FSTTTurnToDirectionTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTTurnToDirectionTaskInstanceData;

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

private:
	static EStateTreeRunStatus UpdateRotation(
		FInstanceDataType& InstanceData,
		float DeltaTime);
};
