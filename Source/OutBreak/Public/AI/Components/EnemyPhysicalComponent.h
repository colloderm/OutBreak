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
	float MaxDurability;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Durability;
	
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
	
	void ActionPhysical(FHitResult HitResult, float DamageAmount);

private:
	void ActionLimb(UStaticMesh* MeshAsset, FName BoneName, float Damage);
	ELocomotionWalkRunState EvaluateLocomotionState() const;
	
	float Health = 200.f;
	
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
