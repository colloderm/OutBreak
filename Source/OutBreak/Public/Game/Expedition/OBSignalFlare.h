#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBSignalFlare.generated.h"

class UNiagaraSystem;
class UProjectileMovementComponent;
class USceneComponent;
class USoundBase;

/** Replicated signal flare. Assign Niagara and sound uassets on a Blueprint child. */
UCLASS(Blueprintable)
class OUTBREAK_API AOBSignalFlare : public AActor
{
	GENERATED_BODY()

public:
	AOBSignalFlare();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Signal Flare")
	bool HasBurst() const { return bBurst; }

protected:
	void Burst();
	void PlayLaunchPresentation();
	void PlayBurstPresentation();

	UFUNCTION()
	void OnRep_Burst();

	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Flare", meta = (DisplayName = "On Flare Launched"))
	void BP_OnFlareLaunched();

	UFUNCTION(BlueprintImplementableEvent, Category = "Signal Flare", meta = (DisplayName = "On Flare Burst"))
	void BP_OnFlareBurst();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare|Visual")
	TObjectPtr<UNiagaraSystem> TrailSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare|Visual")
	TObjectPtr<UNiagaraSystem> BurstSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare|Visual")
	TObjectPtr<UNiagaraSystem> PersistentSmokeSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare|Audio")
	TObjectPtr<USoundBase> LaunchSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare|Audio")
	TObjectPtr<USoundBase> BurstSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare", meta = (ClampMin = "100"))
	float LaunchSpeed = 4000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare", meta = (ClampMin = "0.1"))
	float FuseSeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare", meta = (ClampMin = "1"))
	float LifeAfterBurst = 45.f;

	UPROPERTY(ReplicatedUsing = OnRep_Burst, BlueprintReadOnly, Category = "Signal Flare")
	bool bBurst = false;

private:
	FTimerHandle BurstTimer;
};
