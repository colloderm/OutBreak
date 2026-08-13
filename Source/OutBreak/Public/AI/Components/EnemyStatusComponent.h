// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Data/EnemyState.h"
#include "EnemyBaseActorComponent.h"
#include "EnemyStatusComponent.generated.h"




UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyStatusComponent : public UEnemyBaseActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyStatusComponent();

	void ApplyActionState(EEnemyActionState NewState);
	void ApplyTimedActionState(
		EEnemyActionState NewState,
		float Duration);
	void ClearActionState(EEnemyActionState StateToClear);
	void ClearTimedActionState();
	void SetDead();
	void ResetForPool();

	UFUNCTION(BlueprintPure, Category = "Enemy|Status")
	EEnemyActionState GetActionState() const { return ActionState; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Status")
	bool CanMove() const { return ActionState == EEnemyActionState::Active; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Status")
	bool CanAct() const { return ActionState == EEnemyActionState::Active; }


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_ActionState();

	void HandleTimedActionStateExpired();
	void SetActionStateInternal(EEnemyActionState NewState);
	void ClearActionStateTimer();
	static int32 GetStatePriority(EEnemyActionState State);

	UPROPERTY(
		ReplicatedUsing = OnRep_ActionState,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Enemy|Status",
		meta = (AllowPrivateAccess = "true"))
	EEnemyActionState ActionState = EEnemyActionState::Active;

	FTimerHandle ActionStateTimerHandle;
	EEnemyActionState TimedActionState = EEnemyActionState::Active;
	double TimedActionStateEndTime = 0.0;
};
