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

	UFUNCTION(BlueprintPure, Category = "Extraction")
	EOBExtractionType GetExtractType() const { return ExtractType; }

	/**
	 * True only for personal zones that received their team assignment through
	 * the expedition GameMode's deferred-spawn path. A personal BP placed
	 * directly in a level has no owning team and is not an assigned extract.
	 */
	UFUNCTION(BlueprintPure, Category = "Extraction|Debug")
	bool IsRuntimeAssignedPersonalZone() const;

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
	virtual void PostInitializeComponents() override;
	
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
	FString GetPlayerExtractionBlockReason(AController* Controller) const;
	void CheckActiveState();
	void DrawDebugStatus() const;
	void StartExtractionCall(AController* CallingController);
	void SpawnExtractionHelicopter();

	// 두 타이밍 값은 BP와 레벨 인스턴스에서 따로 편집된다. LeadTime이 CallDelay보다
	// 커지면 호출 즉시 스폰되고 접근 비행이 0.1초로 뭉개져 헬기가 순간이동한다.
	float GetEffectiveInboundLeadTime() const;
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
	
	/**
	 * BP에 직렬화된 낡은 컴포넌트 값이 C++ 생성자 값을 덮어쓴다(현재 32cm).
	 * 런타임에 다시 써서 직렬화 값과 무관하게 항상 이 값이 되게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (ClampMin = "50.0", Units = "cm"))
	float CallTriggerRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (ClampMin = "50.0", Units = "cm"))
	float BoardingTriggerRadius = 500.f;

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

	// 호출 → 착륙까지. 헬기가 화면에 나타나는 시점은 (이 값 - InboundLeadTime)이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Timing", meta = (ClampMin = "1"))
	float HelicopterCallDelay = 25.f;

	// 착륙 몇 초 전에 스폰해 접근 비행을 시작할지. 이 값이 곧 접근 비행 시간이다.
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

	/** Draw this zone's trigger, anchors, state and local-player eligibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Debug")
	bool bDrawDebugVisualization = false;

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
