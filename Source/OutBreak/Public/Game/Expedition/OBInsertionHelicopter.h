#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Expedition/OBHelicopterTypes.h"
#include "OBInsertionHelicopter.generated.h"

class AController;
class AOBExtractionZone;
class AOBHelicopterRoute;
class APawn;
class UAudioComponent;
class UCameraComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UWorldPartitionStreamingSourceComponent;
class AOBInsertionHelicopter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOBInsertionHelicopterPhaseChanged, AOBInsertionHelicopter*, Helicopter, EOBInsertionPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOBExtractionHelicopterPhaseChanged, AOBInsertionHelicopter*, Helicopter, EOBExtractionCallPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOBHelicopterPassengerEvent, AOBInsertionHelicopter*, Helicopter, AController*, Passenger);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOBHelicopterEvent, AOBInsertionHelicopter*, Helicopter);

/**
 * Server-authoritative helicopter logic shared by insertion and extraction.
 * A Blueprint child supplies the skeletal mesh, animation, effects, and audio.
 */
UCLASS(Blueprintable)
class OUTBREAK_API AOBInsertionHelicopter : public AActor
{
	GENERATED_BODY()

public:
	AOBInsertionHelicopter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
		const FVector& SrcLocation) const override;

	FRotator GetCabinViewRotation() const;

	void InitializeInsertion(uint8 InTeamId, AOBHelicopterRoute* OrbitRoute, const FVector& OrbitCenter);
	void BeginInsertionApproach(const FOBLandingZoneResult& LandingZone);

	void InitializeExtraction(AOBExtractionZone* InExtractionZone);
	void BeginExtractionApproach(AOBHelicopterRoute* ApproachRoute, const FTransform& LandingTransform, float TotalApproachSeconds);
	void BeginExtractionDeparture(AOBHelicopterRoute* ExitRoute, float DepartureSeconds);

	UFUNCTION(BlueprintCallable, Category = "Helicopter|Passengers")
	bool SeatPassenger(AController* Controller);

	UFUNCTION(BlueprintCallable, Category = "Helicopter|Passengers")
	void ReleaseAllPassengers(const FVector& GroundCenter);
	
	/**
	 * 탈출 정산 시점 전용. FinalizePassenger와 달리 콜리전·이동을 되살리지 않는다.
	 * 이미 숨겨진 폰을 그 자리에 고정해 헬기가 사라져도 추락하지 않게 한다.
	 */
	void ReleaseExtractedPassengers();

	/** Removes one tracked passenger without treating a disconnect as a successful insertion. */
	bool RemovePassenger(AController* Controller, bool bNotifyDeployment = false);

	UFUNCTION(BlueprintPure, Category = "Helicopter|Passengers")
	int32 GetSeatCapacity() const { return PassengerSeats.Num(); }

	UFUNCTION(BlueprintPure, Category = "Helicopter|Passengers")
	int32 GetPassengerCount() const
	{
		return HasAuthority() ? PassengerControllers.Num() : ReplicatedPassengerStates.Num();
	}

	UFUNCTION(BlueprintPure, Category = "Helicopter|Passengers")
	TArray<FOBHelicopterPassengerNetState> GetReplicatedPassengerStates() const
	{
		return ReplicatedPassengerStates;
	}

	UFUNCTION(BlueprintPure, Category = "Helicopter|Passengers")
	FTransform GetSeatTransform(int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category = "Helicopter")
	EOBHelicopterMission GetMission() const { return Mission; }

	UFUNCTION(BlueprintPure, Category = "Helicopter")
	EOBInsertionPhase GetInsertionPhase() const { return InsertionPhase; }

	UFUNCTION(BlueprintPure, Category = "Helicopter")
	EOBExtractionCallPhase GetExtractionPhase() const { return ExtractionPhase; }

	UFUNCTION(BlueprintPure, Category = "Helicopter")
	uint8 GetTeamId() const { return TeamId; }

	UFUNCTION(BlueprintPure, Category = "Helicopter")
	FVector GetResolvedGroundLocation() const { return ResolvedGroundLocation; }

	/** Replicated flight state for Blueprint rotor/airframe presentation. */
	UFUNCTION(BlueprintPure, Category = "Helicopter|Flight")
	bool IsSteeringFlightActive() const { return bSteeringMotion; }

	UFUNCTION(BlueprintPure, Category = "Helicopter|Flight")
	FVector GetSteeringFlightVelocity() const { return FVector(ReplicatedSteeringVelocity); }

	/** -1 left, +1 right. */
	UFUNCTION(BlueprintPure, Category = "Helicopter|Flight")
	float GetSteeringTurnInput() const { return ReplicatedSteeringTurnInput; }

	/** -1 nose down, +1 nose up. Includes acceleration/braking and climb/descent. */
	UFUNCTION(BlueprintPure, Category = "Helicopter|Flight")
	float GetSteeringPitchInput() const { return ReplicatedSteeringPitchInput; }

	UPROPERTY(BlueprintAssignable, Category = "Helicopter|Events")
	FOBInsertionHelicopterPhaseChanged OnInsertionPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Helicopter|Events")
	FOBExtractionHelicopterPhaseChanged OnExtractionPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Helicopter|Events")
	FOBHelicopterPassengerEvent OnPassengerDeployed;

	UPROPERTY(BlueprintAssignable, Category = "Helicopter|Events")
	FOBHelicopterEvent OnAllPassengersDeployed;

	UPROPERTY(BlueprintAssignable, Category = "Helicopter|Events")
	FOBHelicopterEvent OnExtractionBoardingReady;

	UPROPERTY(BlueprintAssignable, Category = "Helicopter|Events")
	FOBHelicopterEvent OnExtractionDepartureCompleted;

protected:
	UFUNCTION()
	void OnRep_Mission();

	UFUNCTION()
	void OnRep_InsertionPhase();

	UFUNCTION()
	void OnRep_ExtractionPhase();

	UFUNCTION()
	void OnRep_DoorsOpen();

	UFUNCTION()
	void OnRep_PassengerStates();

	void SetInsertionPhase(EOBInsertionPhase NewPhase);
	void SetExtractionPhase(EOBExtractionCallPhase NewPhase);
	void SetDoorsOpen(bool bOpen);
	void BeginTransformMotion(const FTransform& Target, float Duration, uint8 CompletionTask);
	void BeginRouteMotion(AOBHelicopterRoute* Route, float Duration, bool bLoop, uint8 CompletionTask);
	void BeginSteeringMotion(const FTransform& Target, float DesiredDuration, uint8 CompletionTask);
	void TickMotion(float DeltaSeconds);
	void TickSteeringMotion(float DeltaSeconds);
	/**
	 * 전방 경로에서 요구되는 최소 고도. 장애물이 없으면 아주 작은 값을 돌려준다.
	 * @param OutDistanceToObstacle 제약을 만든 지점까지의 수평 거리. 상승 속도 계산에 쓴다.
	 */
	float ComputeObstacleFloorZ(const FVector& From, const FVector& Direction, float Reach, float& OutDistanceToObstacle) const;
	void TickOrbit(float DeltaSeconds);
	void HandleMotionCompleted(uint8 CompletionTask);
	void CompleteInsertionScan();
	void StartInsertionRappel();
	void TickRappels(float DeltaSeconds);
	void StartNextRappel();
	void FinishRappel(int32 ActiveIndex);
	void OpenExtractionBoarding();
	void FinishInsertionDeparture();
	void FinishExtractionDeparture();
	void SetPassengerTransitState(
		AController* Controller,
		EOBPlayerInsertionTransitPhase Phase,
		AActor* ViewTarget);
	bool FinalizePassenger(
		AController* Controller,
		APawn* Pawn,
		const FVector* DeploymentLocation,
		bool bNotifyDeployment);
	void NotifyAllPassengersDeployed();
	void AddReplicatedSeatedPassenger(APawn* Pawn, int32 SeatIndex);
	void SetReplicatedPassengerRappelling(APawn* Pawn, int32 RopeIndex,
		const FVector& RopeStart, const FVector& RopeEnd);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Mission Changed"))
	void BP_OnMissionChanged(EOBHelicopterMission NewMission);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Insertion Phase Changed"))
	void BP_OnInsertionPhaseChanged(EOBInsertionPhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Extraction Phase Changed"))
	void BP_OnExtractionPhaseChanged(EOBExtractionCallPhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Doors Changed"))
	void BP_OnDoorsChanged(bool bOpen);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Rappel Line Changed"))
	void BP_OnRappelLineChanged(int32 RopeIndex, bool bActive, FVector RopeStart, FVector RopeEnd);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Passenger Seated"))
	void BP_OnPassengerSeated(AController* Passenger, int32 SeatIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Passenger Rappel Started"))
	void BP_OnPassengerRappelStarted(AController* Passenger, int32 RopeIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation", meta = (DisplayName = "On Passenger Landed"))
	void BP_OnPassengerLanded(AController* Passenger);

	/** Pawn-based passenger events are available on every relevant client. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation|Network", meta = (DisplayName = "On Replicated Passenger Seated"))
	void BP_OnReplicatedPassengerSeated(APawn* PassengerPawn, int32 SeatIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation|Network", meta = (DisplayName = "On Replicated Passenger Rappel Started"))
	void BP_OnReplicatedPassengerRappelStarted(
		APawn* PassengerPawn, int32 RopeIndex, FVector RopeStart, FVector RopeEnd);

	UFUNCTION(BlueprintImplementableEvent, Category = "Helicopter|Presentation|Network", meta = (DisplayName = "On Replicated Passenger Landed"))
	void BP_OnReplicatedPassengerLanded(APawn* PassengerPawn);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Attach the vendor helicopter mesh and visual components below this anchor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CabinCamera;

	/** Uses the local controller rotation while this helicopter is its ViewTarget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Camera")
	bool bEnablePassengerFreeLook = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LeftRappelAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RightRappelAnchor;

	/** Twelve editable native anchors. Move them onto the cabin seats in the Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<USceneComponent>> PassengerSeats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> RotorAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> GroundDustComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSourceComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Assets")
	TObjectPtr<USoundBase> RotorLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Assets")
	TObjectPtr<UNiagaraSystem> GroundDustSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Orbit", meta = (ClampMin = "1000"))
	float DefaultOrbitRadius = 60000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Orbit", meta = (ClampMin = "500"))
	float DefaultOrbitHeight = 10000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Orbit", meta = (ClampMin = "100"))
	float OrbitSpeed = 3500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Insertion", meta = (ClampMin = "0.1"))
	float InsertionApproachSeconds = 12.f;

	/** Uses acceleration, braking, turn-rate and an approach gate instead of a straight transform lerp. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering")
	bool bUseSteeringInsertionApproach = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "100"))
	float SteeringMinApproachSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "100"))
	float SteeringMaxApproachSpeed = 12000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "10"))
	float SteeringAcceleration = 2800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "10"))
	float SteeringDeceleration = 3500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float SteeringMaxTurnRateDegrees = 28.f;

	/** Predicts current momentum this many seconds ahead when choosing the turn direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float SteeringLookAheadSeconds = 1.35f;

	/** Distance behind the scan hold transform used to establish a stable final approach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "0"))
	float SteeringFinalLegDistance = 9000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "100"))
	float SteeringGateAcceptanceRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "50"))
	float SteeringArrivalRadius = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "10"))
	float SteeringMaxVerticalSpeed = 2800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float SteeringMaxBankAngle = 18.f;
	
	/** 뱅크 각 계산의 기준 각오차. 이 각도에서 최대 뱅크가 된다. 작을수록 예민하다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float SteeringBankReferenceAngle = 60.f;

	/** Maximum nose-up/down attitude produced by longitudinal and vertical steering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float SteeringMaxPitchAngle = 14.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SteeringAccelerationPitchWeight = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SteeringVerticalPitchWeight = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "0.1"))
	float SteeringAttitudeInterpSpeed = 3.5f;

	/** Safety fallback only; normal completion is based on arrival distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering", meta = (ClampMin = "1.0"))
	float SteeringTimeoutMultiplier = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Steering|Debug")
	bool bDrawSteeringDebug = false;
	
	/** 비행 경로 전방의 지형·건물 윗면을 확인해 그 위로 넘어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Obstacle")
	bool bAvoidObstacles = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Obstacle", meta = (ClampMin = "500", Units = "cm"))
	float ObstacleLookAheadDistance = 12000.f;

	/** 장애물 윗면에서 이만큼 더 띄운다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Obstacle", meta = (ClampMin = "100", Units = "cm"))
	float ObstacleClearance = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Flight|Obstacle", meta = (ClampMin = "1", ClampMax = "12"))
	int32 ObstacleProbeCount = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Insertion")
	FVector ScanningHoldOffset = FVector(-3500.f, 0.f, 600.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Insertion", meta = (ClampMin = "0"))
	float ScanDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Insertion", meta = (ClampMin = "0.1"))
	float HoverTransitionSeconds = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Insertion", meta = (ClampMin = "0"))
	float DoorOpenDelay = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Rappel", meta = (ClampMin = "50"))
	float RappelSpeed = 650.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Rappel", meta = (ClampMin = "0"))
	float RappelStaggerSeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Rappel", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxSimultaneousRappels = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Rappel", meta = (ClampMin = "50"))
	float LandingSlotSpacing = 180.f;

	/** Keep normal rappel endpoints inside the scanner's validated footprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Rappel", meta = (ClampMin = "0"))
	float RappelLandingFormationRadius = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Insertion", meta = (ClampMin = "0.1"))
	float InsertionDepartureSeconds = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Insertion")
	FVector InsertionDepartureOffset = FVector(25000.f, 0.f, 8000.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Extraction", meta = (ClampMin = "0"))
	float ExtractionLandingSettleSeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Helicopter|Extraction")
	FVector ExtractionDepartureOffset = FVector(25000.f, 0.f, 8000.f);

private:
	struct FActiveRappel
	{
		TWeakObjectPtr<AController> Controller;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Elapsed = 0.f;
		float Duration = 1.f;
		int32 RopeIndex = 0;
	};

	UPROPERTY(ReplicatedUsing = OnRep_Mission)
	EOBHelicopterMission Mission = EOBHelicopterMission::None;

	UPROPERTY(ReplicatedUsing = OnRep_InsertionPhase)
	EOBInsertionPhase InsertionPhase = EOBInsertionPhase::None;

	UPROPERTY(ReplicatedUsing = OnRep_ExtractionPhase)
	EOBExtractionCallPhase ExtractionPhase = EOBExtractionCallPhase::Ready;

	UPROPERTY(Replicated)
	uint8 TeamId = 0;

	UPROPERTY(Replicated)
	FVector_NetQuantize ResolvedGroundLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_DoorsOpen)
	bool bDoorsOpen = false;

	UPROPERTY(Replicated)
	bool bSteeringMotion = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 ReplicatedSteeringVelocity = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float ReplicatedSteeringTurnInput = 0.f;

	UPROPERTY(Replicated)
	float ReplicatedSteeringPitchInput = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_PassengerStates)
	TArray<FOBHelicopterPassengerNetState> ReplicatedPassengerStates;

	UPROPERTY(Transient)
	TArray<FOBHelicopterPassengerNetState> PresentedPassengerStates;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AController>> PassengerControllers;

	UPROPERTY(Transient)
	TArray<int32> PassengerSeatIndices;

	UPROPERTY(Transient)
	TObjectPtr<AOBExtractionZone> ExtractionZone;

	TWeakObjectPtr<AOBHelicopterRoute> ActiveRoute;
	FTransform MotionStartTransform = FTransform::Identity;
	FTransform MotionTargetTransform = FTransform::Identity;
	float MotionElapsed = 0.f;
	float MotionDuration = 1.f;
	float RouteProgress = 0.f;
	float OrbitAngleRadians = 0.f;
	FVector ProceduralOrbitCenter = FVector::ZeroVector;
	uint8 MotionCompletionTask = 0;
	bool bMotionActive = false;
	bool bRouteMotion = false;
	bool bLoopRoute = false;
	bool bSteeringFinalLeg = false;
	FVector SteeringVelocity = FVector::ZeroVector;
	FVector SteeringApproachGate = FVector::ZeroVector;
	float SteeringCruiseSpeed = 0.f;
	float SteeringPreviousDistance = TNumericLimits<float>::Max();
	float SteeringPreviousGateDistance = TNumericLimits<float>::Max();
	float SteeringLastLogTime = -1000.f;

	FOBLandingZoneResult ActiveLandingZone;
	TArray<TWeakObjectPtr<AController>> RappelQueue;
	TArray<FActiveRappel> ActiveRappels;
	float RappelStartAccumulator = 0.f;
	int32 RappelSequenceIndex = 0;
	int32 RappelSequenceTotal = 0;
	bool bAllPassengersDeployedNotified = false;

	FTimerHandle SequenceTimer;
};
