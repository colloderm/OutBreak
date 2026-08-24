#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBSignalFlare.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UProjectileMovementComponent;
class USceneComponent;
class USoundBase;

/**
 * Replicated extraction signal flare. Flight, shared Niagara defaults, audio,
 * noise reporting, and burst lifetime are configured in Project Settings.
 * Blueprint children can still supply visual components and presentation events.
 */
UCLASS(Blueprintable)
class OUTBREAK_API AOBSignalFlare : public AActor
{
	GENERATED_BODY()

public:
	AOBSignalFlare();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Signal Flare")
	bool HasBurst() const { return bBurst; }

protected:
	void Burst();
	void ConfigureFromProjectSettings();
	void StopOwnedNiagaraComponents();
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

	/** Legacy per-BP fallback. The project setting now controls runtime launch speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare", meta = (ClampMin = "100"))
	float LaunchSpeed = 4000.f;

	/** Legacy safety timeout fallback retained for existing Blueprint assets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare", meta = (ClampMin = "0.1"))
	float FuseSeconds = 2.5f;

	/** Legacy destroy-delay fallback retained for existing Blueprint assets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signal Flare", meta = (ClampMin = "1"))
	float LifeAfterBurst = 45.f;

	/** Final authoritative burst point, replicated so every client spawns effects at the same location. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Flare")
	FVector_NetQuantize BurstLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_Burst, BlueprintReadOnly, Category = "Signal Flare")
	bool bBurst = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SpawnedTrailComponent;

	FVector LaunchLocation = FVector::ZeroVector;
	FVector LaunchDirection = FVector::UpVector;
	float BurstHeight = 5000.f;
	float DestroyDelayAfterBurst = 2.f;
	FTimerHandle SafetyBurstTimer;
	bool bLaunchPresentationPlayed = false;
	bool bBurstPresentationPlayed = false;
};
