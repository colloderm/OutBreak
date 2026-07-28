// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Data/EnemyState.h"
#include "EnemyMemoryComponent.generated.h"


struct FAIStimulus;

DECLARE_MULTICAST_DELEGATE(FOnEnemyMemoryUpdated);

UCLASS(ClassGroup=(EnemyAI), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyMemoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyMemoryComponent();

	void UpdateFromPerception(
		AActor* UpdatedActor,
		const FAIStimulus& Stimulus);

	void HandlePerceptionForgotten(AActor* ForgottenActor);
	
	AActor* GetTargetActor() const { return TargetActor.Get(); }
	bool HasValidTarget() const { return IsValid(TargetActor); }
	bool IsTargetVisible() const { return bTargetVisible && HasValidTarget(); }
	const FVector& GetLastKnownTargetLocation() const { return LastKnownTargetLocation; }
	float GetTimeSinceTargetSeen() const;

	bool HasActionableStimulus() const { return StimulusType != EEnemyStimulusType::None; }
	EEnemyStimulusType GetStimulusType() const { return StimulusType; }
	const FVector& GetLastStimulusLocation() const { return LastStimulusLocation; }
	bool HasHearingStimulus() const { return StimulusType == EEnemyStimulusType::Hearing; }
	const FVector& GetLastHeardLocation() const { return LastHeardLocation; }
	bool HasDamageStimulus() const { return StimulusType == EEnemyStimulusType::Damage; }
	const FVector& GetLastDamageDirection() const { return LastDamageDirection; }

	/**
	 * Clears the current stimulus only when it still matches ExpectedType.
	 * This prevents a stale consumer from clearing a newer stimulus.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Memory")
	bool ConsumeStimulus(EEnemyStimulusType ExpectedType);

	/**
	 * A single wake-up signal for every meaningful memory update.
	 * Consumers must read the complete snapshot instead of inferring state from
	 * the notification itself.
	 */
	FOnEnemyMemoryUpdated OnMemoryUpdated;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	
private:
	void UpdateSightMemory(
		AActor* UpdatedActor,
		const FAIStimulus& Stimulus);

	void UpdateHearingMemory(
		AActor* UpdatedActor,
		const FAIStimulus& Stimulus);

	void UpdateDamageMemory(
		AActor* UpdatedActor,
		const FAIStimulus& Stimulus);

	void ClearStimulusMemory();
	void BroadcastMemoryUpdated();
	FVector ResolveStimulusLocation(
		const AActor& UpdatedActor,
		const FAIStimulus& Stimulus) const;
	FVector ResolveDamageDirection(
		const AActor& UpdatedActor,
		const FAIStimulus& Stimulus) const;
	double GetCurrentTimeSeconds() const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	bool bTargetVisible = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	EEnemyStimulusType StimulusType = EEnemyStimulusType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	FVector LastStimulusLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	FVector LastHeardLocation = FVector::ZeroVector;

	/** Unit vector from the damage receiver toward the source of the hit. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	FVector LastDamageDirection = FVector::ZeroVector;

	double LastTargetSeenTime = 0.0;
	double LastStimulusTime = 0.0;

	UPROPERTY(EditAnywhere, Category = "AI|Memory", meta = (ClampMin = "0.0"))
	float TargetMemoryDuration = 5.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Memory", meta = (ClampMin = "0.0"))
	float StimulusMemoryDuration = 8.0f;
};
