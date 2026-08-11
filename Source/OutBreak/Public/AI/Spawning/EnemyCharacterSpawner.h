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

	bool CanSpawnForNoise(const FEnemyNoiseEvent& NoiseEvent, FString* OutFailureReason = nullptr) const;
	FTransform ResolveSpawnTransform() const;
	TSubclassOf<AEnemyCharacter> ResolveEnemyClass() const;
	FName ResolvePoolKey() const;
	UAnimMontage* ResolveSpawnMontage() const;
	float ResolvePresentationDuration() const;
	int32 ResolveWarmPoolCount() const;
	void MarkUsed();

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="s"))
	float ReuseCooldown = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="cm"))
	float MinPlayerDistance = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation", meta=(AllowPrivateAccess="true"))
	bool bRequireNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Validation", meta=(AllowPrivateAccess="true"))
	bool bEnabled = true;

	double LastUsedTime = -DBL_MAX;
};
