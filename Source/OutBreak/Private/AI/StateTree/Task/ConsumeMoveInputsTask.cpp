#include "AI/StateTree/Task/ConsumeMoveInputsTask.h"

#include "AI/Components/EnemyMemoryComponent.h"
#include "AI/EnemyController.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTTConsumeMoveInputsTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (FVector* Destination =
		InstanceData.DestinationToConsume.GetMutablePtr<FVector>(Context))
	{
		*Destination = FVector::ZeroVector;
	}

	for (const FSTTBoolToConsume& BoolToConsume :
		 InstanceData.BoolsToConsume)
	{
		if (bool* Value =
			BoolToConsume.Value.GetMutablePtr<bool>(Context))
		{
			*Value = false;
		}
	}

	if (InstanceData.StimulusToConsume !=
		EEnemyStimulusType::None)
	{
		const APawn* ControlledPawn =
			InstanceData.ControlledPawn.Get();
		const AEnemyController* EnemyController =
			IsValid(ControlledPawn)
				? Cast<AEnemyController>(ControlledPawn->GetController())
				: nullptr;
		UEnemyMemoryComponent* MemoryComponent =
			IsValid(EnemyController)
				? EnemyController->GetEnemyMemoryComponent()
				: nullptr;

		if (IsValid(MemoryComponent))
		{
			MemoryComponent->ConsumeStimulus(
				InstanceData.StimulusToConsume);
		}
	}

	return EStateTreeRunStatus::Succeeded;
}
