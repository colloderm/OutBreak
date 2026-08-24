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
	
	// 이 액터를 대상에서 완전히 지운다(탈출 등 더 이상 존재하지 않는 대상).
	// HandlePerceptionForgotten과 달리 마지막 목격 지점 수색도 남기지 않는다.
	void ForgetTarget(const AActor* Actor);
	
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

	/** Injects an authoritative Director command through the same memory path as hearing. */
	void SetDirectedHearingStimulus(const FVector& NoiseLocation);

	/** Clears target and stimulus state before pool reuse. */
	void ResetMemory();

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
	// 이미 쫓는 표적이 있을 때 새 자극으로 갈아탈지 판정한다.
	bool ShouldSwitchTargetTo(const AActor* Candidate) const;
	
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
	
	// 새 표적이 이만큼 더 가까워야 갈아탄다. 0이면 매 자극마다 표적이 요동친다.
	UPROPERTY(EditAnywhere, Category = "AI|Memory", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetSwitchDistanceMargin = 300.0f;

};
