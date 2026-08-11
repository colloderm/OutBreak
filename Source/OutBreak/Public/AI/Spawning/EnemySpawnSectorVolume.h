#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawnSectorVolume.generated.h"

class UBoxComponent;

UCLASS(Blueprintable)
class OUTBREAK_API AEnemySpawnSectorVolume : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawnSectorVolume();

	bool ContainsLocation(const FVector& WorldLocation) const;
	FName GetSectorId() const { return SectorId; }
	int32 GetSoftCap() const { return SoftCap; }
	int32 GetHardCap() const { return HardCap; }
	int32 GetBaseZombieTarget() const { return BaseZombieTarget; }
	float GetResponseRadiusScale() const { return ResponseRadiusScale; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> SectorBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sector", meta=(AllowPrivateAccess="true"))
	FName SectorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sector", meta=(AllowPrivateAccess="true", ClampMin="1"))
	int32 SoftCap = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sector", meta=(AllowPrivateAccess="true", ClampMin="1"))
	int32 HardCap = 24;

	/** Resident population. Noise reinforcements are added on top of this count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sector", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 BaseZombieTarget = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sector", meta=(AllowPrivateAccess="true", ClampMin="0.1"))
	float ResponseRadiusScale = 1.0f;
};
