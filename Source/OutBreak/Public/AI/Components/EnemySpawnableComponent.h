#pragma once

#include "CoreMinimal.h"
#include "AI/Spawning/EnemySpawnTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "EnemySpawnableComponent.generated.h"

class AEnemyCharacter;
class UAnimMontage;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FEnemySpawnPhaseChanged,
	EEnemyPoolPhase, PreviousPhase,
	EEnemyPoolPhase, NewPhase);

UCLASS(ClassGroup=(EnemyAI), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemySpawnableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemySpawnableComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeAsPooled(FName InPoolKey, EEnemyPopulationRole InPopulationRole);
	void ReserveForActivation(
		FName InPoolKey,
		FName InSectorId,
		EEnemyPopulationRole InPopulationRole);
	void AdoptAsSectorBase(FName InPoolKey, FName InSectorId);
	void BeginSpawnPresentation(
		const FTransform& SpawnTransform,
		UAnimMontage* SpawnMontage,
		float PresentationDuration,
		const FEnemyNoiseEvent& NoiseEvent);
	void ScheduleReturnToPool(float Delay);
	void ReturnToPoolNow();
	void CommandInvestigateNoise(const FVector& NoiseLocation);

	/** Optional AnimNotify hook. The authoritative timer remains the fallback. */
	UFUNCTION(BlueprintCallable, Category="Enemy Pool")
	void NotifySpawnPresentationReady();

	EEnemyPoolPhase GetPoolPhase() const { return SpawnState.Phase; }
	FName GetPoolKey() const { return SpawnState.PoolKey; }
	FName GetSectorId() const { return SpawnState.SectorId; }
	EEnemyPopulationRole GetPopulationRole() const { return SpawnState.PopulationRole; }
	int32 GetActivationId() const { return SpawnState.ActivationId; }
	bool IsBudgeted() const;

	UPROPERTY(BlueprintAssignable, Category="Enemy Pool")
	FEnemySpawnPhaseChanged OnSpawnPhaseChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FPrimitiveCollisionSnapshot
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
		ECollisionChannel ObjectType = ECC_WorldDynamic;
		FCollisionResponseContainer Responses;
		FTransform RelativeTransform = FTransform::Identity;
		bool bGenerateOverlapEvents = false;
		bool bSimulatePhysics = false;
		bool bIsRootComponent = false;
	};

	void CaptureCollisionSnapshot();
	void DisableOwnerCollision();
	void RestoreCollisionSnapshot();
	void SuspendOwner();
	bool SnapOwnerToGround(FString& OutFailureReason);
	void RestoreOwnerForActivation();
	void ResumeOwnerAI();
	void UpdateReinforcementCleanup();
	void SetSpawnPhase(EEnemyPoolPhase NewPhase);
	void ApplySpawnState(EEnemyPoolPhase PreviousPhase);
	void PlayPresentationCosmetics();
	void FinishSpawnPresentationServer(int32 ExpectedActivationId);
	AEnemyCharacter* GetEnemyCharacter() const;
	double GetServerTimeSeconds() const;

	UFUNCTION()
	void OnRep_SpawnState(FEnemySpawnRepState PreviousState);

	UFUNCTION()
	void OnRep_ActiveSpawnMontage();

	UPROPERTY(ReplicatedUsing=OnRep_SpawnState, VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy Pool", meta=(AllowPrivateAccess="true"))
	FEnemySpawnRepState SpawnState;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveSpawnMontage, Transient)
	TObjectPtr<UAnimMontage> ActiveSpawnMontage = nullptr;

	UPROPERTY(Transient)
	FVector PendingNoiseLocation = FVector::ZeroVector;

	TArray<FPrimitiveCollisionSnapshot> CollisionSnapshots;
	FTimerHandle PresentationTimerHandle;
	FTimerHandle PoolReturnTimerHandle;
	int32 LastPresentedActivationId = 0;
	bool bCollisionSnapshotCaptured = false;
	bool bHasPendingNoiseCommand = false;
	double LastCombatActivityTime = 0.0;
};
