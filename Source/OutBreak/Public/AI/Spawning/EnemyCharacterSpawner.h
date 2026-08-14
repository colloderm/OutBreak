#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyCharacterSpawner.generated.h"

class AEnemyCharacter;
class UAnimMontage;
class UArrowComponent;
class UBoxComponent;
class UEnemySpawnProfile;
class USceneComponent;
struct FEnemyNoiseEvent;

UCLASS(Blueprintable)
class OUTBREAK_API AEnemyCharacterSpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemyCharacterSpawner();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool CanSpawnForNoise(const FEnemyNoiseEvent& NoiseEvent, FString* OutFailureReason = nullptr) const;
	FTransform ResolveSpawnTransform() const;
	bool TryResolveSafeSpawnTransform(FTransform& OutTransform, FString* OutFailureReason = nullptr) const;
	TSubclassOf<AEnemyCharacter> ResolveEnemyClass() const;
	FName ResolvePoolKey() const;
	UAnimMontage* ResolveSpawnMontage() const;
	float ResolvePresentationDuration() const;
	int32 ResolveWarmPoolCount() const;
	int32 ResolveInitialSpawnCharges() const;
	int32 ResolveMaxSpawnCharges() const;
	int32 ResolveRechargeAmount() const;
	float ResolveRechargeInterval() const;
	void MarkUsed();

	UFUNCTION(BlueprintPure, Category="Enemy Spawner|Charge")
	int32 GetCurrentSpawnCharges() const { return CurrentSpawnCharges; }

	UFUNCTION(BlueprintImplementableEvent, Category="Enemy Spawner", meta=(DisplayName="On Enemy Emerging"))
	void BP_OnEnemyEmerging(AEnemyCharacter* Enemy, const FVector& NoiseLocation);

	FName GetSectorId() const { return SectorId; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> SpawnDirection;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> OccupancyPreview;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UEnemySpawnProfile> SpawnProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Fallback", meta=(AllowPrivateAccess="true"))
	TSubclassOf<AEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Fallback", meta=(AllowPrivateAccess="true"))
	FName PoolKey = TEXT("DefaultZombie");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Fallback", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> SpawnMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Fallback", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="s"))
	float PresentationDuration = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Fallback", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 WarmPoolCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(AllowPrivateAccess="true"))
	FName SectorId = NAME_None;

	/** Per-instance additions to the Enemy Director's shared charge values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Charge Adjustments", meta=(AllowPrivateAccess="true"))
	int32 InitialChargeBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Charge Adjustments", meta=(AllowPrivateAccess="true"))
	int32 MaxChargeBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Charge Adjustments", meta=(AllowPrivateAccess="true"))
	int32 RechargeAmountBonus = 0;

	/** Added to the shared interval. Negative values recharge this spawner faster. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Charge Adjustments", meta=(AllowPrivateAccess="true", Units="s"))
	float RechargeIntervalAdjustment = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="s"))
	float ReuseCooldown = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float MinPlayerDistance = 1200.0f;

	/** Legacy asset compatibility. Enemy character placement now always requires NavMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement|Legacy", meta=(AllowPrivateAccess="true", DisplayName="Legacy Require Navigation (Always On)"))
	bool bRequireNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float NavigationSearchRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float NavigationSearchHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float CollisionSearchRadius = 900.0f;

	/** Number of concentric rings sampled inside CollisionSearchRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement", meta=(AllowPrivateAccess="true", ClampMin="1", ClampMax="12"))
	int32 PlacementSearchRings = 5;

	/** Candidate count per ring. Adjacent rings are angle-staggered for better coverage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement", meta=(AllowPrivateAccess="true", ClampMin="4", ClampMax="32"))
	int32 PlacementSamplesPerRing = 12;

	/** Extra horizontal gap between a new zombie capsule and an existing zombie capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float MinEnemySpawnSeparation = 20.0f;

	/** Legacy compatibility value. Vertical lifting is intentionally no longer used for spawn placement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement|Legacy", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm", DisplayName="Legacy Max Vertical Adjustment (Unused)"))
	float MaxVerticalSpawnAdjustment = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float SpawnCapsuleClearance = 8.0f;

	/** Legacy asset compatibility. Off-NavMesh fallback is intentionally disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation|Placement|Legacy", meta=(AllowPrivateAccess="true", DisplayName="Legacy Navigation Fallback (Ignored)"))
	bool bAllowNavigationFallback = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation", meta=(AllowPrivateAccess="true"))
	bool bEnabled = true;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Spawn|Charge", meta=(AllowPrivateAccess="true"))
	int32 CurrentSpawnCharges = 0;

	double LastUsedTime = -DBL_MAX;
	FTimerHandle ChargeRechargeTimerHandle;

	void InitializeSpawnCharges();
	void RechargeSpawnCharges();
};
