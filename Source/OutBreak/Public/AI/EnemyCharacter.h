// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacter.generated.h"

class USkeletalMeshComponentBudgeted;

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

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

private:
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