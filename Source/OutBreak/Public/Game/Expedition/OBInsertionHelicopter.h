#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Expedition/OBHelicopterTypes.h"
#include "OBInsertionHelicopter.generated.h"

class AController;
class AOBExtractionZone;
class AOBHelicopterRoute;
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
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeInsertion(uint8 InTeamId, AOBHelicopterRoute* OrbitRoute, const FVector& OrbitCenter);
	void BeginInsertionApproach(const FOBLandingZoneResult& LandingZone);

	void InitializeExtraction(AOBExtractionZone* InExtractionZone);
	void BeginExtractionApproach(AOBHelicopterRoute* ApproachRoute, const FTransform& LandingTransform, float TotalApproachSeconds);
	void BeginExtractionDeparture(AOBHelicopterRoute* ExitRoute, float DepartureSeconds);

	UFUNCTION(BlueprintCallable, Category = "Helicopter|Passengers")
	bool SeatPassenger(AController* Controller);

	UFUNCTION(BlueprintCallable, Category = "Helicopter|Passengers")
	void ReleaseAllPassengers(const FVector& GroundCenter);

	UFUNCTION(BlueprintPure, Category = "Helicopter|Passengers")
	int32 GetSeatCapacity() const { return PassengerSeats.Num(); }

	UFUNCTION(BlueprintPure, Category = "Helicopter|Passengers")
	int32 GetPassengerCount() const { return PassengerControllers.Num(); }

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

	void SetInsertionPhase(EOBInsertionPhase NewPhase);
	void SetExtractionPhase(EOBExtractionCallPhase NewPhase);
	void SetDoorsOpen(bool bOpen);
	void BeginTransformMotion(const FTransform& Target, float Duration, uint8 CompletionTask);
	void BeginRouteMotion(AOBHelicopterRoute* Route, float Duration, bool bLoop, uint8 CompletionTask);
	void TickMotion(float DeltaSeconds);
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
	void SetPassengerTransitState(AController* Controller, bool bInTransit, AActor* ViewTarget);

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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Attach the vendor helicopter mesh and visual components below this anchor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CabinCamera;

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

	FOBLandingZoneResult ActiveLandingZone;
	TArray<TWeakObjectPtr<AController>> RappelQueue;
	TArray<FActiveRappel> ActiveRappels;
	float RappelStartAccumulator = 0.f;
	int32 RappelSequenceIndex = 0;

	FTimerHandle SequenceTimer;
};
