// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/EnemyState.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "GenericTeamAgentInterface.h"

#include "EnemyCharacter.generated.h"

class AOBLootContainer;
class UEnemyAsset;

class USkeletalMeshComponentBudgeted;
class UMotionWarpingComponent;
class UDamageType;
class UPrimitiveComponent;
class UChildActorComponent;
class AModularSkeletalMeshActor;
class UEnemyStatusComponent;
class UEnemyPhysicalComponent;
class UEnemySpawnableComponent;
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
	void ResetForPoolActivation();
	void PrepareForPoolStorage();
	UEnemySpawnableComponent* GetEnemySpawnableComponent() const { return SpawnableComponent; }
	
	// 서버: 마지막 피격 방향/부위. Dead()에서 래그돌 임펄스로 쓴다.
	void NotifyHitForRagdoll(FName BoneName, const FVector& HitDirection);

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
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsDead();

	// 서버·클라 공통 사망 연출. 드랍/수명/컨트롤러 정리는 Dead()에만 둔다.
	void PlayDeathCosmetics();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyAsset> EnemyAsset = nullptr;

	
	TWeakObjectPtr<UAudioComponent> CryingSoundComponent;
	
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Death",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float DeathCleanupDelay = 5.0f;

	UPROPERTY(
		ReplicatedUsing = OnRep_IsDead,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Transient,
		Category = "Death",
		meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;
	
	// 래그돌 임펄스용. 클라도 같은 방향으로 쓰러져야 하므로 복제한다.
	// bIsDead와 같은 프레임에 갱신되어 같은 번치로 도착한다.
	UPROPERTY(Replicated)
	FVector LastHitDirection = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FName LastHitBoneName = NAME_None;
	
	// 처치 시 남길 드랍 테이블. 비우면 아무것도 안 떨어진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", Meta = (RowType = "/Script/OutBreak.OBLootTableRow"))
	FDataTableRowHandle DeathLootRow;

	// 드랍을 담을 컨테이너 클래스(BP_LootDrop).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<AOBLootContainer> LootContainerClass;
	
	// 좀비 질량에 맞춰 BP에서 조정한다. 플레이어보다 가볍게 잡으면 더 과장되게 날아간다.
	UPROPERTY(EditDefaultsOnly, Category = "Death", Meta = (ClampMin = "0.0"))
	float RagdollImpulseStrength = 12000.f;
	
	/* ================================================== Components ============================================= */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyStatusComponent> StatusComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyPhysicalComponent> PhysicalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemySpawnableComponent> SpawnableComponent;
	
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
