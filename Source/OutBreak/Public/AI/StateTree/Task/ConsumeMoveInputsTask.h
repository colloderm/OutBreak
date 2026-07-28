#pragma once

#include "CoreMinimal.h"
#include "AI/Data/EnemyState.h"
#include "StateTreePropertyRef.h"
#include "Tasks/StateTreeAITask.h"
#include "ConsumeMoveInputsTask.generated.h"

class APawn;

/** Mutable bool binding which is reset when the consume task runs. */
USTRUCT()
struct OUTBREAK_API FSTTBoolToConsume
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (RefType = "bool"))
	FStateTreePropertyRef Value;
};

USTRUCT()
struct OUTBREAK_API FSTTConsumeMoveInputsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<APawn> ControlledPawn = nullptr;

	/** Optional mutable location to reset to zero. */
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (RefType = "/Script/CoreUObject.Vector", Optional))
	FStateTreePropertyRef DestinationToConsume;

	/** Mutable flags to reset to false. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FSTTBoolToConsume> BoolsToConsume;

	/** Optional authoritative stimulus to consume from EnemyMemoryComponent. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EEnemyStimulusType StimulusToConsume = EEnemyStimulusType::None;
};

/** Consumes move inputs and completes immediately. */
USTRUCT(
	meta = (
		DisplayName = "Consume Move Inputs",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FSTTConsumeMoveInputsTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTConsumeMoveInputsTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
