#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "TimedMoveToTask.generated.h"

class AActor;
class AAIController;
class APawn;
class UNavigationQueryFilter;

USTRUCT()
struct OUTBREAK_API FSTTTimedMoveToTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;

	/** Used when TargetActor is not set. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector Destination = FVector::ZeroVector;

	/** Optional moving goal. Takes priority over Destination. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** Maximum time spent moving. Timeout stops movement and succeeds. */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float MoveDuration = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UNavigationQueryFilter> NavigationFilter;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bUsePathfinding = true;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bProjectGoalLocation = true;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bAllowPartialPath = true;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bAllowStrafe = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bReachTestIncludesAgentRadius = true;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bReachTestIncludesGoalRadius = true;

protected:
	UPROPERTY(Transient)
	float ElapsedTime = 0.0f;

	UPROPERTY(Transient)
	bool bMoveRequested = false;

	UPROPERTY(Transient)
	bool bUsingTargetActor = false;

	friend struct FSTTTimedMoveToTask;
};

/** Move To with a maximum duration. Movement is always stopped on exit. */
USTRUCT(
	meta = (
		DisplayName = "Timed Move To",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FSTTTimedMoveToTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTTimedMoveToTaskInstanceData;

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

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

private:
	static AAIController* ResolveController(
		const FInstanceDataType& InstanceData);

	static void StopMovement(FInstanceDataType& InstanceData);
};
