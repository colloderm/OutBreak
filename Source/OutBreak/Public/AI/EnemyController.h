// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DetourCrowdAIController.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyController.generated.h"

class UStateTreeAIComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class UEnemyMemoryComponent;

UCLASS()
class OUTBREAK_API AEnemyController : public ADetourCrowdAIController
{
	GENERATED_BODY()


public:
	// Sets default values for this actor's properties
	AEnemyController();

	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	void InitializeComponents();

	void Dead(float CleanupDelay);
	void SuspendForPool();
	void ResumeFromPool();
	void InvestigateNoise(const FVector& NoiseLocation);

	UFUNCTION(BlueprintPure, Category = "AI|Memory")
	UEnemyMemoryComponent* GetEnemyMemoryComponent() const
	{
		return EnemyMemoryComponent.Get();
	}
	
	// 월드의 모든 적이 이 액터를 즉시 잊는다. 탈출한 플레이어를 계속 쫓지 않게 한다.
	static void ForgetActorForAll(UWorld* World, AActor* Actor);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

private:
	
	/* ===================================== Components ==================================== */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Memory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyMemoryComponent> EnemyMemoryComponent;
	
	void InitializeMemoryComponent();
	void HandleMemoryUpdated();
	
	/* ==================================================================================== */
	
	
	/* =================================== AI Perception =================================== */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;
	
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;
	
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;
	
	void InitializeAIPerception();

	void ApplySightAffiliationFilter();
	
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus);
	
	UFUNCTION()
	void HandleTargetPerceptionForgotten(AActor* UpdatedActor); 
	
private:

	/* ==================================================================================== */
	
	
	/* ==================================== State Tree ==================================== */
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProxySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;
	
	void InitializeStateTree();
	void StopStateTreeLogic(const FString& Reason);
	

	/* ==================================================================================== */
	
	
	
};
