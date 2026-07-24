// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DetourCrowdAIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyController.generated.h"

class UStateTreeAIComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;

UENUM(BlueprintType)
enum class EEnemyAlertState : uint8
{
	Idle,
	Suspicious,
	Chase,
	Investigating,
	Combat
};


UCLASS()
class OUTBREAK_API AEnemyController : public ADetourCrowdAIController
{
	GENERATED_BODY()


public:
	// Sets default values for this actor's properties
	AEnemyController();

	void InitializeComponents();
	

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void OnPossess(APawn* inPawn) override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	
	/* =================================== AI Perception =================================== */
private:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;
	
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;
	
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;
	
	void InitializeAIPerception();
	
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus);
	
	UFUNCTION()
	void HandleTargetPerceptionForgotten(AActor* UpdatedActor); 
	
	void HandleSightStimulus(AActor* UpdatedActor, const FAIStimulus& Stimulus);
	
	void HandleHearingStimulus(AActor* UpdatedActor, const FAIStimulus& Stimulus);
	
	void HandleDamageStimulus(AActor* UpdatedActor, const FAIStimulus& Stimulus);
	
	// AI Perception using variable
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> PerceptionTarget;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	bool bHasPerceptionTarget = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	EEnemyAlertState AlertState = EEnemyAlertState::Idle;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	bool bCanSeeTarget = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TOptional<FVector> SelfToHearingDirection = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TOptional<FVector> SlefToDamageDirection = FVector::ZeroVector;
	
	/* ==================================================================================== */
	
	
	/* ==================================== State Tree ==================================== */
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProxySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;
	
	void InitializeStateTree();
	

public:
	AActor* GetCurrentTargetActor() const
	{
		return PerceptionTarget;
	}
	
	/* ==================================================================================== */
	
};
