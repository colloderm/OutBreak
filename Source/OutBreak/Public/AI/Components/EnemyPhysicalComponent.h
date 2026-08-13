// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBaseActorComponent.h"
#include "Components/TimelineComponent.h"
#include "AI/Data/EnemyState.h"
#include "EnemyPhysicalComponent.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FLimbData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	bool bIsHas = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	float MaxDurability = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runtime")
	float Durability = 0.0f;

	FLimbData() = default;

	explicit FLimbData(const float InMaxDurability)
		: MaxDurability(InMaxDurability),
		  Durability(InMaxDurability)
	{
	}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyPhysicalComponent : public UEnemyBaseActorComponent
{
	GENERATED_BODY()

public:
	UEnemyPhysicalComponent();

	void SetHealth(float NewHealth);
	void ResetForPool();

	UFUNCTION(BlueprintPure, Category = "Enemy|Physical|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Physical|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Physical|Health")
	float GetLimbDurability(FName LimbBoneName) const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Physical|Health")
	float GetLimbMaxDurability(FName LimbBoneName) const;

protected:
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	void ApplyDamage(float DamageAmount);
	void ActionPhysical(
		const FHitResult& HitResult,
		float DamageAmount,
		const FVector& ShotDirection);
	void BloodVFX(const FHitResult& HitResult);
	
	// 나이아가라는 복제되지 않는다. 피격 연출은 서버가 모두에게 쏜다.
	// Unreliable: 연출이라 부하 시 버려도 된다.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_BloodVFX(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReact(
		EEnemyHitReactRegion Region,
		FName PhysicsBoneName,
		FVector_NetQuantizeNormal ImpulseDirection,
		bool bAllowMontage);

	ELocomotionWalkRunState EvaluateLocomotionState() const;

	UFUNCTION(BlueprintPure, Category = "Physical|State")
	EEnemyMissingArmState GetMissingArmState() const;
private:
	void InitializeHealthStateFromSettings();
	void InitializeLimbStateFromSettings();
	void SynchronizeLocomotionState();
	bool IsLimbPresent(FName LimbBoneName) const;

	EEnemyHitReactRegion ResolveHitReactRegion(
		const FHitResult& HitResult,
		const FEnemyPhysicalReact& PhysicalReact) const;
	EEnemyHitReactRegion ResolveHitReactRegionFromBone(
		FName BoneName) const;
	UAnimMontage* ResolveHitReactMontage(
		EEnemyHitReactRegion Region,
		const FEnemyPhysicalReact& PhysicalReact) const;
	FName ResolvePhysicsBoneName(
		EEnemyHitReactRegion Region,
		FName HitBoneName) const;
	FName ResolveLimbBoneName(EEnemyHitReactRegion Region) const;
	bool ShouldSkipHitReactPresentation() const;
	void PlayHitReactPresentation(
		EEnemyHitReactRegion Region,
		FName PhysicsBoneName,
		const FVector& ImpulseDirection,
		bool bAllowMontage);
	void StopHitReactPresentation(bool bPreservePhysics);
	void ReleaseHitReactActionLock();
	void HandleHitReactMontageEnded(
		UAnimMontage* Montage,
		bool bInterrupted);
	void HandleHitReactFallbackFinished();

	// 조각 메시는 뼈 이름으로 찾는다. 호출부마다 넘기면 클라에서 재현할 수 없다.
	void ActionLimb(FName BoneName, float Damage);

	// 서버·클라 공통 파괴 연출. 상태 판정은 ActionLimb/OnRep에서만 한다.
	void ApplyLimbDestruction(FName BoneName);

	UStaticMesh* GetLimbMesh(FName BoneName) const;

	UFUNCTION()
	void OnRep_DestroyedLimbs();

	// TMap은 복제 불가라 파괴된 뼈만 배열로 복제한다.
	UPROPERTY(ReplicatedUsing = OnRep_DestroyedLimbs)
	TArray<FName> DestroyedLimbs;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Status|Health",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "1.0",
			UIMin = "1.0",
			DisplayName = "Overall Max Health"))
	float MaxHealth = 175.0f;

	// Keep the property names for existing Blueprint serialization. The
	// display names and category are the editor-facing API.
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Status|Health|Limb Durability",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "1.0",
			UIMin = "1.0",
			DisplayName = "Head Max Durability"))
	float Head_MaxDurability = 40.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Status|Health|Limb Durability",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "1.0",
			UIMin = "1.0",
			DisplayName = "Right Arm Max Durability"))
	float Arm_R_MaxDurability = 40.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Status|Health|Limb Durability",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "1.0",
			UIMin = "1.0",
			DisplayName = "Left Arm Max Durability"))
	float Arm_L_MaxDurability = 40.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Status|Health|Limb Durability",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "1.0",
			UIMin = "1.0",
			DisplayName = "Right Leg Max Durability"))
	float Leg_R_MaxDurability = 40.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Status|Health|Limb Durability",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "1.0",
			UIMin = "1.0",
			DisplayName = "Left Leg Max Durability"))
	float Leg_L_MaxDurability = 40.0f;

	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Transient,
		Category = "Status|Health|Runtime",
		meta = (AllowPrivateAccess = "true"))
	float Health = 175.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Debug|Physical",
		meta = (AllowPrivateAccess = "true"))
	bool bIsDrawDebug = false;

	FTimeline ReactTimeline;
	FName CacheBoneName = NAME_None;
	bool bIsHit = false;
	bool bOwnsHitReactActionLock = false;
	EEnemyHitReactRegion ActiveHitReactRegion =
		EEnemyHitReactRegion::None;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveHitReactMontage = nullptr;

	FTimerHandle HitReactFallbackTimerHandle;
	
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> TargetMesh;
	
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> ProxyMesh = nullptr;
	
	
	void Action_Dead();
	
	
private:
	void DrawDebugLimb();
	
	UPROPERTY(
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Transient,
		Category = "Status|Health|Runtime",
		meta = (AllowPrivateAccess = "true"))
	TMap<FName, FLimbData> Limbes;
	
	
private: /* Timeline */
	UFUNCTION()
	void HandleReactTimeline(float value);
	UFUNCTION()
	void HandleReactTimelineFinished();
};
