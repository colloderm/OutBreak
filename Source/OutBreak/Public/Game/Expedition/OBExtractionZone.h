#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Game/Expedition/OBExpeditionTypes.h"
#include "Game/Expedition/OBHelicopterTypes.h"
#include "OBExtractionZone.generated.h"

class AOBExpeditionGameState;
class AOBExtractionSite;
class AOBHelicopterRoute;
class AOBInsertionHelicopter;
class AOBSignalFlare;
class USceneComponent;
class USphereComponent;

/**
 * Extraction call site. Entering the call trigger launches a flare; the server
 * then schedules a helicopter, opens a boarding window, and only settles loot
 * after the boarded helicopter departs.
 */
UCLASS(Blueprintable)
class OUTBREAK_API AOBExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	AOBExtractionZone();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ConfigureAsPersonal(uint8 InTeamId);
	void ConfigureExtractionSite(AOBExtractionSite* Site);
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	void SetActiveWindow(int32 InStartSec, int32 InEndSec);

	UFUNCTION(BlueprintPure, Category = "Extraction")
	const FOBExtractionCallState& GetCallState() const { return CallState; }

	UFUNCTION(BlueprintPure, Category = "Extraction")
	float GetArrivalSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Extraction")
	float GetBoardingSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Extraction")
	bool IsActiveNow() const;

	UFUNCTION(BlueprintPure, Category = "Extraction")
	uint8 GetOwningTeamId() const { return OwningTeamId; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction")
	void OnExtractionActiveChanged(bool bActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction|Presentation", meta = (DisplayName = "On Call Phase Changed"))
	void BP_OnCallPhaseChanged(EOBExtractionCallPhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction|Presentation", meta = (DisplayName = "On Flare Launched"))
	void BP_OnFlareLaunched(AOBSignalFlare* Flare);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction|Presentation", meta = (DisplayName = "On Helicopter Spawned"))
	void BP_OnHelicopterSpawned(AOBInsertionHelicopter* Helicopter);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction|Presentation", meta = (DisplayName = "On Passenger Boarded"))
	void BP_OnPassengerBoarded(AController* Passenger);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnCallTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnBoardingTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnBoardingTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleHelicopterPhaseChanged(AOBInsertionHelicopter* Helicopter, EOBExtractionCallPhase NewPhase);

	UFUNCTION()
	void HandleHelicopterBoardingReady(AOBInsertionHelicopter* Helicopter);

	UFUNCTION()
	void HandleHelicopterDepartureCompleted(AOBInsertionHelicopter* Helicopter);

	UFUNCTION()
	void OnRep_CallState();

	bool CanPlayerExtract(AController* Controller) const;
	void CheckActiveState();
	void StartExtractionCall(AController* CallingController);
	void SpawnExtractionHelicopter();
	void OpenBoardingWindow();
	void ProcessBoarding(float DeltaSeconds);
	void BoardPassenger(AController* Controller);
	void StartDeparture();
	void FinishExtractionFlight();
	void AbortExtraction(const FString& Reason);
	void ResetForReuse();
	void SetCallPhase(EOBExtractionCallPhase NewPhase);
	float GetServerTime() const;
	AOBExpeditionGameState* GetExpeditionGameState() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> BoardingTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LandingAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> FlareLaunchAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
	EOBExtractionType ExtractType = EOBExtractionType::Public;

	/** Reuses the old property: now this is the hold time at the helicopter door. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (ClampMin = "0.1"))
	float HoldTime = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (ClampMin = "0"))
	int32 ActiveStartSec = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (ClampMin = "0"))
	int32 ActiveEndSec = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
	FGameplayTag RequiredItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extraction|Classes")
	TSubclassOf<AOBSignalFlare> SignalFlareClass;

	/** Optional per-zone override. GameMode's extraction class is used when empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extraction|Classes")
	TSubclassOf<AOBInsertionHelicopter> ExtractionHelicopterClass;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Extraction|Flight Path")
	TObjectPtr<AOBHelicopterRoute> ApproachRoute;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Extraction|Flight Path")
	TObjectPtr<AOBHelicopterRoute> ExitRoute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Timing", meta = (ClampMin = "1"))
	float HelicopterCallDelay = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Timing", meta = (ClampMin = "1"))
	float InboundLeadTime = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Timing", meta = (ClampMin = "1"))
	float BoardingWindowSeconds = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Timing", meta = (ClampMin = "0.1"))
	float DepartureSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Timing", meta = (ClampMin = "0"))
	float CooldownSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Timing")
	bool bReusable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Flight Path")
	FVector HelicopterSpawnOffset = FVector(-20000.f, 0.f, 8000.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Rules")
	bool bPublicAllowsAllTeams = true;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CallState)
	FOBExtractionCallState CallState;

	UPROPERTY(Replicated)
	uint8 OwningTeamId = 0;

	UPROPERTY(Transient)
	TObjectPtr<AOBSignalFlare> ActiveFlare;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AController>> BoardedControllers;

	UPROPERTY(Transient)
	TMap<TObjectPtr<AController>, float> BoardingProgress;

	FTimerHandle ActiveCheckTimer;
	float CooldownEndsServerTime = 0.f;
	bool bLastActiveState = false;
};
