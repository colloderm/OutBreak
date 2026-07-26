// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Data/EnemyState.h"
#include "EnemyMemoryComponent.generated.h"


struct FAIStimulus;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnEnemyAIMemoryChanged,
	EEnemyMemoryChange);

UCLASS(ClassGroup=(EnemyAI), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyMemoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyMemoryComponent();
	
	AActor* GetTargetActor() const { return TargetActor.Get(); }
	bool HasValidTarget() const { return IsValid(TargetActor); }
	bool IsTargetVisible() const { return bTargetVisible && HasValidTarget(); }
	const FVector& GetLastKnownTargetLocation() const { return LastKnownTargetLocation; }
	float GetTimeSinceTargetSeen() const;

	bool HasActionableStimulus() const { return StimulusType != EEnemyStimulusType::None; }
	EEnemyStimulusType GetStimulusType() const { return StimulusType; }
	const FVector& GetLastStimulusLocation() const { return LastStimulusLocation; }

	FOnEnemyAIMemoryChanged OnMemoryChanged;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	
private:
	friend class AEnemyController;
	

	UPROPERTY(VisibleInstanceOnly, Category = "AI|Memory")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category = "AI|Memory")
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, Category = "AI|Memory")
	bool bTargetVisible = false;

	UPROPERTY(VisibleInstanceOnly, Category = "AI|Memory")
	EEnemyStimulusType StimulusType = EEnemyStimulusType::None;

	UPROPERTY(VisibleInstanceOnly, Category = "AI|Memory")
	FVector LastStimulusLocation = FVector::ZeroVector;

	double LastTargetSeenTime = 0.0;
	double LastStimulusTime = 0.0;
	float TargetMemoryDuration = 5.0f;
	float StimulusMemoryDuration = 8.0f;
};
