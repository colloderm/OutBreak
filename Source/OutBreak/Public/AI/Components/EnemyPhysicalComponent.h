// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBaseActorComponent.h"
#include "Components/TimelineComponent.h"
#include "AI/Data/EnemyState.h"
#include "EnemyPhysicalComponent.generated.h"





USTRUCT(BlueprintType)
struct FLimbData
{
	GENERATED_BODY()
	
	bool bIsHas = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDurability = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Durability = 0.0f;
	
	FLimbData() = default;
	
	FLimbData(float inMaxDurability, float inDurability)
		: MaxDurability(inMaxDurability), Durability(inDurability) {}
	
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyPhysicalComponent : public UEnemyBaseActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyPhysicalComponent();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsDrawDebug = false;
	
	void SetHealth(float NewHealth) { Health = NewHealth; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	void ApplyDamage(float DamageAmount);
	void ActionPhysical(const FHitResult& HitResult, float DamageAmount);
	void BloodVFX(const FHitResult& HitResult);
	
	// 나이아가라는 복제되지 않는다. 피격 연출은 서버가 모두에게 쏜다.
	// Unreliable: 연출이라 부하 시 버려도 된다.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_BloodVFX(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal);

	ELocomotionWalkRunState EvaluateLocomotionState() const;

	UFUNCTION(BlueprintPure, Category = "Physical|State")
	EEnemyMissingArmState GetMissingArmState() const;
private:
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
	
	float Health = 175.f;
	
	FTimeline ReactTimeline;
	FName CacheBoneName = NAME_None;
	bool bIsHit = false;;
	
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> TargetMesh;
	
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> ProxyMesh = nullptr;
	
	
	void Action_Dead();
	
	
private:
	void DrawDebugLimb();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess="true"))
	TMap<FName, FLimbData> Limbes;
	
	
private: /* Timeline */
	UFUNCTION()
	void HandleReactTimeline(float value);
	UFUNCTION()
	void HandleReactTimelineFinished();
};
