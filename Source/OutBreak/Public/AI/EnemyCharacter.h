// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TimelineComponent.h"
#include "EnemyCharacter.generated.h"

class USkeletalMeshComponentBudgeted;
class UMotionWarpingComponent;
class UDamageType;
class UPrimitiveComponent;

DECLARE_LOG_CATEGORY_EXTERN(
	LogModularAnimationProxy,
	Log,
	All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FModularAnimationProxyReducedWorkChanged,
	bool,
	bReducedWork);

UCLASS(Blueprintable)
class OUTBREAK_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter(
		const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Animation Budget")
	void SetAnimationSignificance(float InSignificance);

	UFUNCTION(BlueprintPure, Category = "Animation Budget|Debug")
	FString GetAnimationBudgetDebugSummary() const;

	UPROPERTY(BlueprintAssignable, Category = "Animation Budget")
	FModularAnimationProxyReducedWorkChanged
	OnReducedAnimationWorkChanged;
	

	UMotionWarpingComponent* GetMotionWarpingComponent() const
	{
		return MotionWarpingComponent;
	}

	

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

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
	
	UPROPERTY(EditAnywhere, Category="Physica|React", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCurveFloat> ReactCurveFloat;
	
	UPROPERTY(EditAnywhere, Category="Physica|React", meta = (AllowPrivateAccess = "true"))
	float ReactScale;
	
	/* Physical Material */
	UPROPERTY(EditAnywhere, Category="Physica|Material", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPhysicalMaterial> PM_Head;
	
	UPROPERTY(EditAnywhere, Category="Physica|Material", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPhysicalMaterial> PM_Torso;
	
	UPROPERTY(EditAnywhere, Category="Physica|Material", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPhysicalMaterial> PM_Arm_R;
	
	UPROPERTY(EditAnywhere, Category="Physica|Material", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPhysicalMaterial> PM_Arm_L;
	
	UPROPERTY(EditAnywhere, Category="Physica|Material", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPhysicalMaterial> PM_Leg_R;
	
	UPROPERTY(EditAnywhere, Category="Physica|Material", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPhysicalMaterial> PM_Leg_L;
	
	
	FTimeline ReactTimeline;
	FName CacheBoneName = NAME_None;
	
	
	UFUNCTION()
	void HandleReactTimeline(float value);
	
	UFUNCTION()
	void HandleReactTimelineFinished();
	
	bool bIsHit = false;
	
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