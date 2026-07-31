// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "AttackNotifyState.generated.h"

class AActor;
class UAnimSequenceBase;
class UGameplayEffect;
class USkeletalMeshComponent;

/** Traces the area between two sockets for the lifetime of the notify state. */
UCLASS(Blueprintable, meta = (DisplayName = "Attack Trace"))
class OUTBREAK_API UAttackNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAttackNotifyState(const FObjectInitializer& ObjectInitializer);

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** Socket used as the beginning of the attack trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Trace|Sockets")
	FName StartSocketName = NAME_None;

	/** Socket used as the end of the attack trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Trace|Sockets")
	FName EndSocketName = NAME_None;

	/** Radius of the sphere swept between the two sockets. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Attack Trace|Collision",
		meta = (ClampMin = "0.1", UIMin = "0.1"))
	float TraceRadius = 10.0f;

	/** Object types that can be collected by this notify instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Trace|Collision")
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	/** Actors of these classes (including derived classes) are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Trace|Filtering")
	TArray<TSubclassOf<AActor>> IgnoredActorClasses;

	/** Ignore the actor that owns the skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Trace|Filtering")
	bool bIgnoreMeshOwner = true;

	/** Run gameplay collision collection only on the authoritative instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Trace|Networking")
	bool bAuthorityOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Attack Trace|Collision")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Attack Trace|Debug")
	bool bDrawDebugTrace = false;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		AdvancedDisplay,
		Category = "Attack Trace|Debug",
		meta = (ClampMin = "0.0"))
	float DebugDrawDuration = 0.0f;

	/** GameplayEffect applied once to every actor hit during this notify window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Trace|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Value passed to the damage GameplayEffect through SetByCaller.Damage. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Attack Trace|Damage",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageAmount = 20.0f;

	/** Level used when creating the outgoing GameplayEffect spec. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		AdvancedDisplay,
		Category = "Attack Trace|Damage",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageEffectLevel = 1.0f;

	/**
	 * Called exactly once from NotifyEnd with the unique, valid actors collected
	 * during this notify window, after the configured damage effect is applied.
	 * Override this event for additional hit reactions or attack effects.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Attack Trace")
	void ProcessCollectedActors(
		USkeletalMeshComponent* MeshComp,
		AActor* AttackOwner,
		const TArray<AActor*>& CollectedActors) const;

	virtual void ProcessCollectedActors_Implementation(
		USkeletalMeshComponent* MeshComp,
		AActor* AttackOwner,
		const TArray<AActor*>& CollectedActors) const;

private:
	struct FAttackTraceRuntimeState
	{
		TMap<TWeakObjectPtr<AActor>, FHitResult> CollectedHits;
	};

	void TraceAndCollect(USkeletalMeshComponent* MeshComp);
	void ApplyDamageEffect(AActor* AttackOwner, const FHitResult& Hit) const;
	bool ShouldIgnoreActor(const AActor* Candidate, const AActor* MeshOwner) const;
	bool CanRunTrace(const USkeletalMeshComponent* MeshComp) const;
	void RemoveStaleRuntimeStates();

	// Notify objects are shared by animation assets. Keep mutable data isolated per mesh instance.
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FAttackTraceRuntimeState> RuntimeStates;
};
