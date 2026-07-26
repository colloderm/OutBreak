#include "AI/StateTree/Evaluator/ST_EvaluateEnemyAIContext.h"

#include "AI/Components/EnemyStatusComponent.h"
#include "AI/Data/EnemyAsset.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyController.h"
#include "StateTreeExecutionContext.h"
//
// void FSTEvaluateEnemyAIContext::TreeStart(FStateTreeExecutionContext& Context) const
// {
// 	UpdateContext(Context);
// }
//
// void FSTEvaluateEnemyAIContext::Tick(
// 	FStateTreeExecutionContext& Context,
// 	const float DeltaTime) const
// {
// 	UpdateContext(Context);
// }
//
// void FSTEvaluateEnemyAIContext::UpdateContext(FStateTreeExecutionContext& Context) const
// {
// 	FInstanceDataType& Data = Context.GetInstanceData(*this);
// 	AEnemyCharacter* Character = Cast<AEnemyCharacter>(Data.ContextActor.Get());
// 	AEnemyController* Controller = Cast<AEnemyController>(Data.AIController.Get());
// 	if (!IsValid(Character) && IsValid(Controller))
// 	{
// 		Character = Cast<AEnemyCharacter>(Controller->GetPawn());
// 	}
// 	if (!IsValid(Controller) && IsValid(Character))
// 	{
// 		Controller = Cast<AEnemyController>(Character->GetController());
// 	}
//
// 	if (!IsValid(Character) || !IsValid(Controller))
// 	{
// 		ClearOutputs(Data);
// 		return;
// 	}
//
// 	ClearOutputs(Data);
// 	const FVector CharacterLocation = Character->GetActorLocation();
//
// 	if (const UEnemyAIMemoryComponent* Memory = Controller->GetMemoryComponent(); IsValid(Memory))
// 	{
// 		Data.RememberedTargetActor = Memory->GetTargetActor();
// 		Data.bHasRememberedTarget = Memory->HasValidTarget();
// 		Data.bTargetVisible = Memory->IsTargetVisible();
// 		Data.TargetActor = Data.bTargetVisible ? Memory->GetTargetActor() : nullptr;
// 		// Compatibility safety: legacy StateTree branches commonly bind this
// 		// property as the Combat guard, so hidden remembered targets must be false.
// 		Data.bHasValidTarget = Data.bTargetVisible;
// 		Data.LastKnownTargetLocation = Memory->GetLastKnownTargetLocation();
// 		Data.TimeSinceTargetSeen = Memory->GetTimeSinceTargetSeen();
//
// 		Data.bHasStimulus = Memory->HasActionableStimulus();
// 		Data.LastStimulusLocation = Memory->GetLastStimulusLocation();
// 		Data.StimulusType = Memory->GetStimulusType();
// 	}
//
// 	Data.bShouldEnterCombat = Data.bTargetVisible;
// 	Data.bShouldEnterAlert = !Data.bTargetVisible &&
// 		(Data.bHasRememberedTarget || Data.bHasStimulus);
// 	Data.bShouldEnterPassive = !Data.bShouldEnterCombat && !Data.bShouldEnterAlert;
// 	if (Data.bShouldEnterAlert)
// 	{
// 		if (Data.bHasRememberedTarget)
// 		{
// 			Data.AlertLocation = Data.LastKnownTargetLocation;
// 			Data.bHasAlertLocation = true;
// 			Data.AlertSource = EEnemyAlertSource::RememberedTarget;
// 		}
// 		else if (Data.bHasStimulus)
// 		{
// 			Data.AlertLocation = Data.LastStimulusLocation;
// 			Data.bHasAlertLocation = true;
// 			Data.AlertSource = EEnemyAlertSource::Stimulus;
// 		}
// 	}
//
// 	if (Data.bHasRememberedTarget)
// 	{
// 		const FVector Offset = Data.LastKnownTargetLocation - CharacterLocation;
// 		Data.TargetDistance = FVector(Offset.X, Offset.Y, 0.0f).Size();
// 		Data.TargetHeightDifference = Offset.Z;
//
// 		if (const UEnemyAsset* Asset = Character->GetEnemyAsset(); IsValid(Asset))
// 		{
// 			const FEnemyCombatSettings& Combat = Asset->GetCombatSettings();
// 			Data.bTargetInAttackRange = Data.bTargetVisible &&
// 				Data.TargetDistance <= Combat.AttackRange &&
// 				FMath::Abs(Data.TargetHeightDifference) <= Combat.MaxAttackHeightDifference;
// 		}
// 	}
//
// 	if (const UEnemyStatusComponent* Status = Character->GetStatusComponent(); IsValid(Status))
// 	{
// 		Data.ActionState = Status->GetActionState();
// 		Data.bIsDead = Status->IsDead();
// 		Data.bIsKnockedDown = Data.ActionState == EEnemyActionState::Knockdown;
// 		Data.bIsStunned = Data.ActionState == EEnemyActionState::Stunned;
// 	}
//
// 	if (const UEnemyTerritoryComponent* Territory = Character->GetTerritoryComponent(); IsValid(Territory))
// 	{
// 		Data.HomeLocation = Territory->GetHomeLocation();
// 		Data.bOutsideTerritory = Territory->IsOutsideTerritory(CharacterLocation);
// 	}
// }
//
// void FSTEvaluateEnemyAIContext::ClearOutputs(FInstanceDataType& Data)
// {
// 	Data.TargetActor = nullptr;
// 	Data.RememberedTargetActor = nullptr;
// 	Data.LastKnownTargetLocation = FVector::ZeroVector;
// 	Data.bHasValidTarget = false;
// 	Data.bHasRememberedTarget = false;
// 	Data.bTargetVisible = false;
// 	Data.bShouldEnterCombat = false;
// 	Data.bShouldEnterAlert = false;
// 	Data.bShouldEnterPassive = true;
// 	Data.bTargetInAttackRange = false;
// 	Data.TargetDistance = 0.0f;
// 	Data.TargetHeightDifference = 0.0f;
// 	Data.TimeSinceTargetSeen = 0.0f;
// 	Data.bHasStimulus = false;
// 	Data.LastStimulusLocation = FVector::ZeroVector;
// 	Data.StimulusType = EEnemyStimulusType::None;
// 	Data.AlertLocation = FVector::ZeroVector;
// 	Data.bHasAlertLocation = false;
// 	Data.AlertSource = EEnemyAlertSource::None;
// 	Data.ActionState = EEnemyActionState::Active;
// 	Data.bIsDead = false;
// 	Data.bIsKnockedDown = false;
// 	Data.bIsStunned = false;
// 	Data.HomeLocation = FVector::ZeroVector;
// 	Data.bOutsideTerritory = false;
// }