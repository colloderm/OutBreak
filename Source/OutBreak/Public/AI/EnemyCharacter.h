// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/EnemyState.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#include "EnemyCharacter.generated.h"

class UEnemyAsset;

class USkeletalMeshComponentBudgeted;
class UMotionWarpingComponent;
class UDamageType;
class UPrimitiveComponent;
class UChildActorComponent;
class AModularSkeletalMeshActor;
class UEnemyStatusComponent;
class UEnemyPhysicalComponent;
class UStateTreeAIComponent;
class UAudioComponent;


DECLARE_LOG_CATEGORY_EXTERN(
	LogModularAnimationProxy,
	Log,
	All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FModularAnimationProxyReducedWorkChanged,
	bool,
	bReducedWork);

UCLASS(Blueprintable)
class OUTBREAK_API AEnemyCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()
	
public:
	virtual FGenericTeamId GetGenericTeamId() const override;

private:
	FGenericTeamId TeamId = FGenericTeamId(2);

public:
	AEnemyCharacter(
		const FObjectInitializer& ObjectInitializer);
	
private:
	void InitializeComponents();
	void InitializeAsset();
public:
	
	
	
	/* ====================================== ABA ===================================== */
	UFUNCTION(BlueprintCallable, Category = "Animation Budget")
	void SetAnimationSignificance(float InSignificance);

	UFUNCTION(BlueprintPure, Category = "Animation Budget|Debug")
	FString GetAnimationBudgetDebugSummary() const;
	
	void GetCanMove();


	UPROPERTY(BlueprintAssignable, Category = "Animation Budget")
	FModularAnimationProxyReducedWorkChanged
	OnReducedAnimationWorkChanged;
	/* ================================================================================ */
	
	UEnemyAsset* GetEnemyAsset() const
	{
		return EnemyAsset;
	}
	
	ELocomotionWalkRunState GetLocomotionWalkRunState() const;
	EEnemyMissingArmState GetMissingArmState() const;

	UMotionWarpingComponent* GetMotionWarpingComponent() const
	{
		return MotionWarpingComponent;
	}

	USkeletalMeshComponent* GetChildActorSkeletalMesh();
	
	void StopCharacterMovement();
	
	void Dead();
	bool IsDead() const { return bIsDead; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;
	
	
	UFUNCTION()
	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;
	
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyAsset> EnemyAsset = nullptr;

	
	TObjectPtr<UAudioComponent> CryingSoundComponent;
	
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Death",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float DeathCleanupDelay = 5.0f;

	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Transient,
		Category = "Death",
		meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;
	
	/* ================================================== Components ============================================= */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyStatusComponent> StatusComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyPhysicalComponent> PhysicalComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProxySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UChildActorComponent> ChildActorComponent;
	
	/* =========================================================================================================== */
	

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProxySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> ChildActorSkeletalMesh;
	
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Animation Budget",
		meta = (AllowPrivateAccess = "true"))
	bool bTickEvenIfNotRendered = false;

	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Transient,
		Category = "Animation Budget",
		meta = (AllowPrivateAccess = "true"))
	bool bReducedAnimationWork = false;

	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Transient,
		Category = "Animation Budget",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "0.0",
			ClampMax = "1.0"))
	float CurrentAnimationSignificance = 1.0f;

	bool bHasAppliedBudgetState = false;

	float LastAppliedAnimationSignificance = 1.0f;

	bool bLastAppliedTickEvenIfNotRendered = false;

	void ApplyAnimationBudgetSettings();

	bool EnsureAnimationBudgetRegistration() const;

	void ApplyAnimationBudgetSignificance();

	void HandleReducedWorkChanged(
		USkeletalMeshComponentBudgeted* InComponent,
		bool bInReducedWork);
};
