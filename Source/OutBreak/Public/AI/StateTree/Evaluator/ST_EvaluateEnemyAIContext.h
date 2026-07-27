// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Data/EnemyState.h"
#include "StateTreeEvaluatorBase.h"
#include "ST_EvaluateEnemyAIContext.generated.h"

class AActor;
class AEnemyCharacter;
class AEnemyController;
class UEnemyMemoryComponent;

USTRUCT()
struct OUTBREAK_API FSTEvaluateEnemyAIContextInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyCharacter> ContextActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyController> AIController = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Context")
	TObjectPtr<UEnemyMemoryComponent> MemoryComponent = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (ClampMin = "0.0"))
	float AttackRange = 150.0f;

	// Kept so existing StateTree assets retain their serialized layout. Memory
	// lifetime is now configured on UEnemyMemoryComponent.
	UPROPERTY(
		EditAnywhere,
		Category = "Parameter",
		meta = (
			ClampMin = "0.0",
			DeprecatedProperty,
			DeprecationMessage = "Configure TargetMemoryDuration on EnemyMemoryComponent."))
	float ForgetTargetTime = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasValidTarget = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bTargetVisible = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bTargetInAttackRange = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float TargetDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float TargetHeightDifference = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float TimeSinceTargetSeen = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasActionableStimulus = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	EEnemyStimulusType StimulusType = EEnemyStimulusType::None;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FVector LastStimulusLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasHearingStimulus = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FVector LastHeardLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasDamageStimulus = false;

	/** Unit vector from the damaged enemy toward the source of the hit. */
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FVector LastDamageDirection = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	ELocomotionWalkRunState LocomotionState =
		ELocomotionWalkRunState::Walking;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	EEnemyMissingArmState MissingArmState =
		EEnemyMissingArmState::None;
};

USTRUCT(
	meta = (
		DisplayName = "Evaluate Enemy AI Context",
		Category = "OutBreak|AI"))
struct OUTBREAK_API FSTEvaluateEnemyAIContext
	: public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType =
		FSTEvaluateEnemyAIContextInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual void TreeStart(
		FStateTreeExecutionContext& Context) const override;

	virtual void Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;

private:
	void UpdateContext(
		FStateTreeExecutionContext& Context) const;

	static UEnemyMemoryComponent* ResolveMemoryComponent(
		FInstanceDataType& InstanceData,
		AEnemyCharacter* EnemyCharacter);

	static void SynchronizePhysicalState(
		FInstanceDataType& InstanceData,
		const AEnemyCharacter& EnemyCharacter);

	static bool SynchronizeMemoryState(
		FInstanceDataType& InstanceData,
		const UEnemyMemoryComponent& MemoryComponent);

	static void UpdateSpatialState(
		FInstanceDataType& InstanceData,
		const AEnemyCharacter& EnemyCharacter,
		const AActor& TargetActor);

	static void ClearTargetState(
		FInstanceDataType& InstanceData);

	static void ClearMemoryState(
		FInstanceDataType& InstanceData);

	static void ClearPhysicalState(
		FInstanceDataType& InstanceData);
};
