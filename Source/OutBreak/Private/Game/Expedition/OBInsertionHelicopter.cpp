#include "Game/Expedition/OBInsertionHelicopter.h"

#include "Camera/CameraComponent.h"
#include "Character/OBCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "DrawDebugHelpers.h"
#include "Game/Expedition/OBExtractionZone.h"
#include "Game/Expedition/OBHelicopterRoute.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBHelicopterInsertion, Log, All);

namespace OBHelicopterTasks
{
	static constexpr uint8 None = 0;
	static constexpr uint8 InsertionApproach = 1;
	static constexpr uint8 InsertionHover = 2;
	static constexpr uint8 InsertionDeparture = 3;
	static constexpr uint8 ExtractionApproach = 4;
	static constexpr uint8 ExtractionDeparture = 5;
}

AOBInsertionHelicopter::AOBInsertionHelicopter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(10.f);
	SetReplicatingMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(SceneRoot);

	CabinCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CabinCamera"));
	CabinCamera->SetupAttachment(VisualRoot);

	LeftRappelAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("Rappel_Left"));
	LeftRappelAnchor->SetupAttachment(VisualRoot);
	LeftRappelAnchor->SetRelativeLocation(FVector(0.f, -150.f, -100.f));

	RightRappelAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("Rappel_Right"));
	RightRappelAnchor->SetupAttachment(VisualRoot);
	RightRappelAnchor->SetRelativeLocation(FVector(0.f, 150.f, -100.f));

	PassengerSeats.Reserve(12);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const FName SeatName(*FString::Printf(TEXT("Seat_%02d"), Index));
		USceneComponent* Seat = CreateDefaultSubobject<USceneComponent>(SeatName);
		Seat->SetupAttachment(VisualRoot);
		Seat->SetRelativeLocation(FVector(-100.f * (Index / 2), Index % 2 == 0 ? -100.f : 100.f, 0.f));
		PassengerSeats.Add(Seat);
	}

	RotorAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("RotorAudio"));
	RotorAudioComponent->SetupAttachment(VisualRoot);
	RotorAudioComponent->bAutoActivate = false;

	GroundDustComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("GroundDust"));
	GroundDustComponent->SetupAttachment(VisualRoot);
	GroundDustComponent->bAutoActivate = false;

	StreamingSourceComponent = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("StreamingSource"));
}

void AOBInsertionHelicopter::BeginPlay()
{
	Super::BeginPlay();

	if (RotorLoopSound)
	{
		RotorAudioComponent->SetSound(RotorLoopSound);
		RotorAudioComponent->Play();
	}
	if (GroundDustSystem)
	{
		GroundDustComponent->SetAsset(GroundDustSystem);
	}
	GroundDustComponent->Deactivate();
	StreamingSourceComponent->EnableStreamingSource();
}

void AOBInsertionHelicopter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const bool bWorldTeardown = EndPlayReason == EEndPlayReason::Quit
		|| EndPlayReason == EEndPlayReason::EndPlayInEditor;
	if (HasAuthority() && !PassengerControllers.IsEmpty() && !bWorldTeardown)
	{
		UE_LOG(LogOBHelicopterInsertion, Error,
			TEXT("[InsertionSafety] Helicopter EndPlay with seated passengers Helicopter=%s Team=%d Phase=%d Passengers=%d Reason=%d ValidatedGround=%s"),
			*GetName(), TeamId, static_cast<int32>(InsertionPhase), PassengerControllers.Num(),
			static_cast<int32>(EndPlayReason), ActiveLandingZone.bValid ? TEXT("true") : TEXT("false"));
		if (ActiveLandingZone.bValid)
		{
			ReleaseAllPassengers(ActiveLandingZone.GroundLocation);
		}
		else
		{
			// There is no validated ground coordinate. Never teleport to the world
			// origin; detach in place and at least return camera/input ownership.
			for (AController* Controller : PassengerControllers)
			{
				APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
				if (!Pawn)
				{
					continue;
				}
				Pawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				Pawn->SetActorEnableCollision(true);
				if (ACharacter* Character = Cast<ACharacter>(Pawn))
				{
					Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				}
				SetPassengerTransitState(Controller, false, Pawn);
			}
			PassengerControllers.Reset();
			PassengerSeatIndices.Reset();
			RappelQueue.Reset();
			ActiveRappels.Reset();
		}
	}
	else if (!PassengerControllers.IsEmpty() && bWorldTeardown)
	{
		UE_LOG(LogOBHelicopterInsertion, Log,
			TEXT("[InsertionSafety] World teardown with seated passengers Helicopter=%s Team=%d Phase=%d Passengers=%d Reason=%d"),
			*GetName(), TeamId, static_cast<int32>(InsertionPhase), PassengerControllers.Num(),
			static_cast<int32>(EndPlayReason));
	}
	Super::EndPlay(EndPlayReason);
}

void AOBInsertionHelicopter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOBInsertionHelicopter, Mission);
	DOREPLIFETIME(AOBInsertionHelicopter, InsertionPhase);
	DOREPLIFETIME(AOBInsertionHelicopter, ExtractionPhase);
	DOREPLIFETIME(AOBInsertionHelicopter, TeamId);
	DOREPLIFETIME(AOBInsertionHelicopter, ResolvedGroundLocation);
	DOREPLIFETIME(AOBInsertionHelicopter, bDoorsOpen);
	DOREPLIFETIME(AOBInsertionHelicopter, bSteeringMotion);
	DOREPLIFETIME(AOBInsertionHelicopter, ReplicatedSteeringVelocity);
	DOREPLIFETIME(AOBInsertionHelicopter, ReplicatedSteeringTurnInput);
	DOREPLIFETIME(AOBInsertionHelicopter, ReplicatedSteeringPitchInput);
	DOREPLIFETIME(AOBInsertionHelicopter, ReplicatedPassengerStates);
}

bool AOBInsertionHelicopter::IsNetRelevantFor(
	const AActor* RealViewer,
	const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	// Insertion helicopters can orbit far outside the normal actor cull radius.
	// Keep the assigned team's aircraft relevant without broadcasting every
	// team's insertion route and passenger state to the entire server.
	if (Mission == EOBHelicopterMission::Insertion && TeamId != 0)
	{
		const AController* ViewingController = Cast<AController>(RealViewer);
		if (!ViewingController)
		{
			if (const APawn* ViewingPawn = Cast<APawn>(ViewTarget))
			{
				ViewingController = ViewingPawn->GetController();
			}
		}
		const AOBPlayerStateBase* ViewerState = ViewingController
			? ViewingController->GetPlayerState<AOBPlayerStateBase>()
			: nullptr;
		if (ViewerState && ViewerState->GetTeamId() == TeamId)
		{
			return true;
		}
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void AOBInsertionHelicopter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	if (InsertionPhase == EOBInsertionPhase::Orbiting && !bMotionActive)
	{
		TickOrbit(DeltaSeconds);
	}
	if (bMotionActive)
	{
		TickMotion(DeltaSeconds);
	}
	if (InsertionPhase == EOBInsertionPhase::Rappelling)
	{
		TickRappels(DeltaSeconds);
	}
}

void AOBInsertionHelicopter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (!CabinCamera)
	{
		Super::CalcCamera(DeltaTime, OutResult);
		return;
	}

	CabinCamera->GetCameraView(DeltaTime, OutResult);
	if (!bEnablePassengerFreeLook || !GetWorld())
	{
		return;
	}

	APlayerController* ViewingController = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Candidate = It->Get();
		if (Candidate && Candidate->IsLocalController() && Candidate->GetViewTarget() == this)
		{
			ViewingController = Candidate;
			break;
		}
	}
	if (!ViewingController)
	{
		return;
	}

	const FRotator BaseRotation = CabinCamera->GetComponentRotation();
	const FRotator RelativeRotation = (ViewingController->GetControlRotation() - BaseRotation).GetNormalized();
	const float Pitch = FMath::Clamp(
		RelativeRotation.Pitch,
		FMath::Min(PassengerFreeLookMinPitch, PassengerFreeLookMaxPitch),
		FMath::Max(PassengerFreeLookMinPitch, PassengerFreeLookMaxPitch));
	const float Yaw = FMath::Clamp(
		RelativeRotation.Yaw,
		-FMath::Abs(PassengerFreeLookYawLimit),
		FMath::Abs(PassengerFreeLookYawLimit));
	OutResult.Rotation = FRotator(
		BaseRotation.Pitch + Pitch,
		BaseRotation.Yaw + Yaw,
		BaseRotation.Roll).GetNormalized();
}

FRotator AOBInsertionHelicopter::GetCabinViewRotation() const
{
	return CabinCamera ? CabinCamera->GetComponentRotation() : GetActorRotation();
}

void AOBInsertionHelicopter::InitializeInsertion(
	uint8 InTeamId,
	AOBHelicopterRoute* OrbitRoute,
	const FVector& OrbitCenter)
{
	if (!HasAuthority())
	{
		return;
	}

	Mission = EOBHelicopterMission::Insertion;
	TeamId = InTeamId;
	ExtractionZone = nullptr;
	ActiveRoute = OrbitRoute;
	RouteProgress = 0.f;
	ProceduralOrbitCenter = OrbitCenter;

	if (OrbitRoute)
	{
		SetActorTransform(OrbitRoute->GetRouteTransform(0.f));
	}
	else
	{
		FHitResult GroundHit;
		const FVector TraceStart(OrbitCenter.X, OrbitCenter.Y, 100000.f);
		const FVector TraceEnd(OrbitCenter.X, OrbitCenter.Y, -100000.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(OBHelicopterOrbitGround), false, this);
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			ProceduralOrbitCenter.Z = GroundHit.ImpactPoint.Z + DefaultOrbitHeight;
		}
		else
		{
			ProceduralOrbitCenter.Z += DefaultOrbitHeight;
		}
		SetActorLocation(ProceduralOrbitCenter + FVector(DefaultOrbitRadius, 0.f, 0.f));
		SetActorRotation(FRotator(0.f, 90.f, 0.f));
	}

	ForceNetUpdate();
	BP_OnMissionChanged(Mission);
	SetInsertionPhase(EOBInsertionPhase::Orbiting);
}

void AOBInsertionHelicopter::InitializeExtraction(AOBExtractionZone* InExtractionZone)
{
	if (!HasAuthority())
	{
		return;
	}

	Mission = EOBHelicopterMission::Extraction;
	ExtractionZone = InExtractionZone;
	ForceNetUpdate();
	BP_OnMissionChanged(Mission);
	SetExtractionPhase(EOBExtractionCallPhase::Inbound);
}

void AOBInsertionHelicopter::BeginInsertionApproach(const FOBLandingZoneResult& LandingZone)
{
	if (!HasAuthority() || Mission != EOBHelicopterMission::Insertion || !LandingZone.bValid)
	{
		return;
	}

	ActiveLandingZone = LandingZone;
	ResolvedGroundLocation = LandingZone.GroundLocation;
	ActiveRoute.Reset();
	bMotionActive = false;
	SetInsertionPhase(EOBInsertionPhase::Approaching);

	FTransform ScanTransform = LandingZone.HoverTransform;
	ScanTransform.AddToTranslation(ScanTransform.TransformVectorNoScale(ScanningHoldOffset));
	const FVector Direction = LandingZone.HoverTransform.GetLocation() - ScanTransform.GetLocation();
	if (!Direction.IsNearlyZero())
	{
		ScanTransform.SetRotation(Direction.Rotation().Quaternion());
	}
	if (bUseSteeringInsertionApproach)
	{
		BeginSteeringMotion(ScanTransform, InsertionApproachSeconds, OBHelicopterTasks::InsertionApproach);
	}
	else
	{
		BeginTransformMotion(ScanTransform, InsertionApproachSeconds, OBHelicopterTasks::InsertionApproach);
	}
}

void AOBInsertionHelicopter::BeginExtractionApproach(
	AOBHelicopterRoute* ApproachRoute,
	const FTransform& LandingTransform,
	float TotalApproachSeconds)
{
	if (!HasAuthority() || Mission != EOBHelicopterMission::Extraction)
	{
		return;
	}

	MotionTargetTransform = LandingTransform;
	SetExtractionPhase(EOBExtractionCallPhase::Inbound);
	const float TravelSeconds = FMath::Max(0.1f, TotalApproachSeconds - ExtractionLandingSettleSeconds);
	if (ApproachRoute && ApproachRoute->GetRouteLength() > 1.f)
	{
		SetActorTransform(ApproachRoute->GetRouteTransform(0.f));
		BeginRouteMotion(ApproachRoute, TravelSeconds, false,
			OBHelicopterTasks::ExtractionApproach);
	}
	else
	{
		BeginTransformMotion(LandingTransform, TravelSeconds,
			OBHelicopterTasks::ExtractionApproach);
	}
}

void AOBInsertionHelicopter::BeginExtractionDeparture(AOBHelicopterRoute* ExitRoute, float DepartureSeconds)
{
	if (!HasAuthority() || Mission != EOBHelicopterMission::Extraction)
	{
		return;
	}

	SetDoorsOpen(false);
	GroundDustComponent->Deactivate();
	SetExtractionPhase(EOBExtractionCallPhase::Departing);
	if (ExitRoute && ExitRoute->GetRouteLength() > 1.f)
	{
		SetActorTransform(ExitRoute->GetRouteTransform(0.f));
		BeginRouteMotion(ExitRoute, FMath::Max(0.1f, DepartureSeconds), false,
			OBHelicopterTasks::ExtractionDeparture);
	}
	else
	{
		FTransform ExitTransform = GetActorTransform();
		ExitTransform.AddToTranslation(ExitTransform.TransformVectorNoScale(ExtractionDepartureOffset));
		BeginTransformMotion(ExitTransform, FMath::Max(0.1f, DepartureSeconds),
			OBHelicopterTasks::ExtractionDeparture);
	}
}

FTransform AOBInsertionHelicopter::GetSeatTransform(int32 SeatIndex) const
{
	return PassengerSeats.IsValidIndex(SeatIndex) && PassengerSeats[SeatIndex]
		? PassengerSeats[SeatIndex]->GetComponentTransform()
		: GetActorTransform();
}

bool AOBInsertionHelicopter::SeatPassenger(AController* Controller)
{
	if (!HasAuthority() || !Controller || !Controller->GetPawn())
	{
		return false;
	}

	if (PassengerControllers.Contains(Controller))
	{
		return true;
	}

	int32 FreeSeat = INDEX_NONE;
	for (int32 SeatIndex = 0; SeatIndex < PassengerSeats.Num(); ++SeatIndex)
	{
		if (!PassengerSeatIndices.Contains(SeatIndex))
		{
			FreeSeat = SeatIndex;
			break;
		}
	}
	if (FreeSeat == INDEX_NONE)
	{
		return false;
	}

	APawn* Pawn = Controller->GetPawn();
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->GetCharacterMovement()->DisableMovement();
		Character->StopJumping();
	}
	Pawn->SetActorEnableCollision(false);
	Pawn->AttachToComponent(PassengerSeats[FreeSeat], FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	PassengerControllers.Add(Controller);
	PassengerSeatIndices.Add(FreeSeat);
	AddReplicatedSeatedPassenger(Pawn, FreeSeat);
	SetPassengerTransitState(Controller, true, this);
	UE_LOG(LogOBHelicopterInsertion, Log,
		TEXT("[Insertion] Passenger seated Helicopter=%s Team=%d Controller=%s Pawn=%s Seat=%d Count=%d"),
		*GetName(), TeamId, *Controller->GetName(), *GetNameSafe(Pawn), FreeSeat, PassengerControllers.Num());
	BP_OnPassengerSeated(Controller, FreeSeat);
	return true;
}

void AOBInsertionHelicopter::ReleaseAllPassengers(const FVector& GroundCenter)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogOBHelicopterInsertion, Warning,
		TEXT("[InsertionSafety] ReleaseAllPassengers Helicopter=%s Team=%d Phase=%d Count=%d Ground=%s"),
		*GetName(), TeamId, static_cast<int32>(InsertionPhase), PassengerControllers.Num(),
		*GroundCenter.ToCompactString());
	for (int32 Index = 0; Index < PassengerControllers.Num(); ++Index)
	{
		AController* Controller = PassengerControllers[Index];
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		const float Angle = PassengerControllers.Num() > 0
			? 2.f * UE_PI * static_cast<float>(Index) / static_cast<float>(PassengerControllers.Num()) : 0.f;
		const FVector Offset(FMath::Cos(Angle) * LandingSlotSpacing, FMath::Sin(Angle) * LandingSlotSpacing, 0.f);
		float CapsuleHalfHeight = 90.f;
		if (const ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}

		Pawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Pawn->SetActorLocation(GroundCenter + Offset + FVector(0.f, 0.f, CapsuleHalfHeight + 5.f), false, nullptr,
			ETeleportType::TeleportPhysics);
		Pawn->SetActorEnableCollision(true);
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		SetPassengerTransitState(Controller, false, Pawn);
	}

	PassengerControllers.Reset();
	PassengerSeatIndices.Reset();
	RappelQueue.Reset();
	ActiveRappels.Reset();
	ReplicatedPassengerStates.Reset();
	ForceNetUpdate();
}

void AOBInsertionHelicopter::SetInsertionPhase(EOBInsertionPhase NewPhase)
{
	if (InsertionPhase == NewPhase)
	{
		return;
	}
	const EOBInsertionPhase PreviousPhase = InsertionPhase;
	InsertionPhase = NewPhase;
	UE_LOG(LogOBHelicopterInsertion, Log,
		TEXT("[Insertion] Helicopter phase changed Helicopter=%s Team=%d Previous=%d New=%d Location=%s Passengers=%d"),
		*GetName(), TeamId, static_cast<int32>(PreviousPhase), static_cast<int32>(NewPhase),
		*GetActorLocation().ToCompactString(), PassengerControllers.Num());
	ForceNetUpdate();
	BP_OnInsertionPhaseChanged(NewPhase);
	OnInsertionPhaseChanged.Broadcast(this, NewPhase);
}

void AOBInsertionHelicopter::SetExtractionPhase(EOBExtractionCallPhase NewPhase)
{
	if (ExtractionPhase == NewPhase)
	{
		return;
	}
	ExtractionPhase = NewPhase;
	ForceNetUpdate();
	BP_OnExtractionPhaseChanged(NewPhase);
	OnExtractionPhaseChanged.Broadcast(this, NewPhase);
}

void AOBInsertionHelicopter::SetDoorsOpen(bool bOpen)
{
	if (bDoorsOpen == bOpen)
	{
		return;
	}
	bDoorsOpen = bOpen;
	ForceNetUpdate();
	BP_OnDoorsChanged(bOpen);
}

void AOBInsertionHelicopter::BeginTransformMotion(const FTransform& Target, float Duration, uint8 CompletionTask)
{
	MotionStartTransform = GetActorTransform();
	MotionTargetTransform = Target;
	MotionElapsed = 0.f;
	MotionDuration = FMath::Max(0.05f, Duration);
	MotionCompletionTask = CompletionTask;
	bMotionActive = true;
	bRouteMotion = false;
	bLoopRoute = false;
	bSteeringMotion = false;
	SteeringVelocity = FVector::ZeroVector;
	ReplicatedSteeringVelocity = FVector::ZeroVector;
	ReplicatedSteeringTurnInput = 0.f;
	ReplicatedSteeringPitchInput = 0.f;
}

void AOBInsertionHelicopter::BeginRouteMotion(
	AOBHelicopterRoute* Route,
	float Duration,
	bool bLoop,
	uint8 CompletionTask)
{
	ActiveRoute = Route;
	RouteProgress = 0.f;
	MotionElapsed = 0.f;
	MotionDuration = FMath::Max(0.05f, Duration);
	MotionCompletionTask = CompletionTask;
	bMotionActive = Route != nullptr;
	bRouteMotion = true;
	bLoopRoute = bLoop;
	bSteeringMotion = false;
	SteeringVelocity = FVector::ZeroVector;
	ReplicatedSteeringVelocity = FVector::ZeroVector;
	ReplicatedSteeringTurnInput = 0.f;
	ReplicatedSteeringPitchInput = 0.f;
}

void AOBInsertionHelicopter::BeginSteeringMotion(
	const FTransform& Target,
	float DesiredDuration,
	uint8 CompletionTask)
{
	MotionStartTransform = GetActorTransform();
	MotionTargetTransform = Target;
	MotionElapsed = 0.f;
	MotionDuration = FMath::Max(0.1f, DesiredDuration);
	MotionCompletionTask = CompletionTask;
	bMotionActive = true;
	bRouteMotion = false;
	bLoopRoute = false;
	bSteeringMotion = true;
	bSteeringFinalLeg = false;
	SteeringLastLogTime = -1000.f;

	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = Target.GetLocation();
	FVector FinalForward = Target.GetRotation().GetForwardVector().GetSafeNormal2D();
	if (FinalForward.IsNearlyZero())
	{
		FinalForward = (TargetLocation - CurrentLocation).GetSafeNormal2D();
	}
	SteeringApproachGate = TargetLocation - FinalForward * FMath::Max(0.f, SteeringFinalLegDistance);

	const float CurrentTargetDistance2D = FVector::Dist2D(CurrentLocation, TargetLocation);
	bSteeringFinalLeg = SteeringFinalLegDistance <= KINDA_SMALL_NUMBER
		|| CurrentTargetDistance2D <= SteeringFinalLegDistance * 1.25f;

	const float EstimatedPathLength = bSteeringFinalLeg
		? FVector::Distance(CurrentLocation, TargetLocation)
		: FVector::Distance(CurrentLocation, SteeringApproachGate)
			+ FVector::Distance(SteeringApproachGate, TargetLocation);
	const float MinSpeed = FMath::Max(100.f, FMath::Min(SteeringMinApproachSpeed, SteeringMaxApproachSpeed));
	const float MaxSpeed = FMath::Max(MinSpeed, SteeringMaxApproachSpeed);
	SteeringCruiseSpeed = FMath::Clamp(EstimatedPathLength / MotionDuration, MinSpeed, MaxSpeed);

	const float InitialSpeed = FMath::Clamp(FMath::Max(OrbitSpeed, MinSpeed), MinSpeed, SteeringCruiseSpeed);
	FVector InitialForward = GetActorForwardVector().GetSafeNormal2D();
	if (InitialForward.IsNearlyZero())
	{
		InitialForward = (TargetLocation - CurrentLocation).GetSafeNormal2D();
	}
	SteeringVelocity = InitialForward * InitialSpeed;
	ReplicatedSteeringVelocity = SteeringVelocity;
	ReplicatedSteeringTurnInput = 0.f;
	ReplicatedSteeringPitchInput = 0.f;
	SteeringPreviousDistance = FVector::Distance(CurrentLocation, TargetLocation);
	SteeringPreviousGateDistance = FVector::Dist2D(CurrentLocation, SteeringApproachGate);
	ForceNetUpdate();

	UE_LOG(LogOBHelicopterInsertion, Display,
		TEXT("[HelicopterSteering] Begin Helicopter=%s Start=%s Target=%s Gate=%s FinalLeg=%s InitialSpeed=%.0f CruiseSpeed=%.0f TurnRate=%.1f LookAhead=%.2f DurationTarget=%.1f"),
		*GetName(), *CurrentLocation.ToCompactString(), *TargetLocation.ToCompactString(),
		*SteeringApproachGate.ToCompactString(), bSteeringFinalLeg ? TEXT("true") : TEXT("false"),
		InitialSpeed, SteeringCruiseSpeed, SteeringMaxTurnRateDegrees,
		SteeringLookAheadSeconds, MotionDuration);
}

void AOBInsertionHelicopter::TickMotion(float DeltaSeconds)
{
	if (bSteeringMotion)
	{
		TickSteeringMotion(DeltaSeconds);
		return;
	}

	MotionElapsed += DeltaSeconds;
	float Alpha = FMath::Clamp(MotionElapsed / MotionDuration, 0.f, 1.f);

	if (bRouteMotion && ActiveRoute.IsValid())
	{
		RouteProgress = bLoopRoute ? FMath::Fmod(Alpha, 1.f) : Alpha;
		SetActorTransform(ActiveRoute->GetRouteTransform(RouteProgress), false, nullptr, ETeleportType::None);
	}
	else
	{
		const float SmoothAlpha = FMath::SmoothStep(0.f, 1.f, Alpha);
		const FVector Location = FMath::Lerp(MotionStartTransform.GetLocation(), MotionTargetTransform.GetLocation(), SmoothAlpha);
		const FQuat Rotation = FQuat::Slerp(MotionStartTransform.GetRotation(), MotionTargetTransform.GetRotation(), SmoothAlpha).GetNormalized();
		SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::None);
	}

	if (Alpha >= 1.f && !bLoopRoute)
	{
		const uint8 CompletedTask = MotionCompletionTask;
		bMotionActive = false;
		MotionCompletionTask = OBHelicopterTasks::None;
		HandleMotionCompleted(CompletedTask);
	}
}

void AOBInsertionHelicopter::TickSteeringMotion(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	MotionElapsed += DeltaSeconds;
	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = MotionTargetTransform.GetLocation();

	if (!bSteeringFinalLeg)
	{
		const float GateDistance2D = FVector::Dist2D(CurrentLocation, SteeringApproachGate);
		const float GateRadius = FMath::Max(100.f, SteeringGateAcceptanceRadius);
		// A helicopter moving at cruise speed must start curving toward the final
		// hover point before physically crossing the gate. Waiting for the small
		// gate radius can put the target behind the airframe and create an orbit.
		const float PredictiveTransitionRadius = FMath::Max(
			GateRadius,
			FMath::Min(
				FMath::Max(GateRadius, SteeringFinalLegDistance),
				SteeringVelocity.Size2D() * FMath::Max(0.5f, SteeringLookAheadSeconds)));
		const bool bPassedClosestGatePoint = GateDistance2D > SteeringPreviousGateDistance
			&& SteeringPreviousGateDistance <= GateRadius * 1.5f;
		if (GateDistance2D <= PredictiveTransitionRadius || bPassedClosestGatePoint)
		{
			bSteeringFinalLeg = true;
			UE_LOG(LogOBHelicopterInsertion, Display,
				TEXT("[HelicopterSteering] Final leg entered Helicopter=%s Location=%s GateDistance=%.0f TransitionRadius=%.0f Elapsed=%.2f"),
				*GetName(), *CurrentLocation.ToCompactString(), GateDistance2D,
				PredictiveTransitionRadius, MotionElapsed);
		}
		SteeringPreviousGateDistance = GateDistance2D;
	}

	const FVector SteeringTarget = bSteeringFinalLeg ? TargetLocation : SteeringApproachGate;
	const float DistanceToSteeringTarget2D = FVector::Dist2D(CurrentLocation, SteeringTarget);
	const float DistanceToFinalTarget = FVector::Distance(CurrentLocation, TargetLocation);
	const float CurrentHorizontalSpeed = SteeringVelocity.Size2D();
	const float PredictionTime = FMath::Min(
		FMath::Max(0.f, SteeringLookAheadSeconds),
		DistanceToSteeringTarget2D / FMath::Max(100.f, CurrentHorizontalSpeed) * 0.65f);
	const FVector PredictedLocation = CurrentLocation + FVector(
		SteeringVelocity.X,
		SteeringVelocity.Y,
		0.f) * PredictionTime;
	FVector DesiredDirection = (SteeringTarget - PredictedLocation).GetSafeNormal2D();
	if (DesiredDirection.IsNearlyZero())
	{
		DesiredDirection = (SteeringTarget - CurrentLocation).GetSafeNormal2D();
	}

	const float CurrentYaw = CurrentHorizontalSpeed > 10.f
		? SteeringVelocity.Rotation().Yaw
		: GetActorRotation().Yaw;
	const float DesiredYaw = DesiredDirection.IsNearlyZero() ? CurrentYaw : DesiredDirection.Rotation().Yaw;
	const float UnclampedYawDelta = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
	const float MaxYawStep = FMath::Max(1.f, SteeringMaxTurnRateDegrees) * DeltaSeconds;
	const float AppliedYawDelta = FMath::Clamp(UnclampedYawDelta, -MaxYawStep, MaxYawStep);
	const float NewYaw = FRotator::NormalizeAxis(CurrentYaw + AppliedYawDelta);
	const float TurnInput = MaxYawStep > UE_SMALL_NUMBER
		? FMath::Clamp(AppliedYawDelta / MaxYawStep, -1.f, 1.f)
		: 0.f;

	float DesiredHorizontalSpeed = SteeringCruiseSpeed;
	if (bSteeringFinalLeg)
	{
		const float BrakingDistance = FMath::Max(0.f, DistanceToFinalTarget - SteeringArrivalRadius * 0.25f);
		DesiredHorizontalSpeed = FMath::Min(
			SteeringCruiseSpeed,
			FMath::Sqrt(2.f * FMath::Max(10.f, SteeringDeceleration) * BrakingDistance));
	}
	// Rotorcraft can keep yawing while translating very slowly. A low final-leg
	// factor lets the simulated turn radius shrink below the arrival radius
	// instead of orbiting the hover point forever.
	const float MinimumTurnSpeedFactor = bSteeringFinalLeg ? 0.06f : 0.55f;
	const float TurnSlowdown = FMath::Lerp(
		1.f,
		MinimumTurnSpeedFactor,
		FMath::Clamp(FMath::Abs(UnclampedYawDelta) / 120.f, 0.f, 1.f));
	DesiredHorizontalSpeed *= TurnSlowdown;
	if (!bSteeringFinalLeg && DistanceToFinalTarget > SteeringArrivalRadius * 2.f)
	{
		DesiredHorizontalSpeed = FMath::Max(
			FMath::Min(SteeringMinApproachSpeed, SteeringCruiseSpeed),
			DesiredHorizontalSpeed);
	}

	const float SpeedChangeRate = DesiredHorizontalSpeed >= CurrentHorizontalSpeed
		? FMath::Max(10.f, SteeringAcceleration)
		: FMath::Max(10.f, SteeringDeceleration);
	const float NewHorizontalSpeed = FMath::FInterpConstantTo(
		CurrentHorizontalSpeed, DesiredHorizontalSpeed, DeltaSeconds, SpeedChangeRate);

	const float EstimatedArrivalSeconds = FMath::Max(
		0.5f,
		DistanceToSteeringTarget2D / FMath::Max(300.f, NewHorizontalSpeed));
	const float DesiredVerticalSpeed = FMath::Clamp(
		(SteeringTarget.Z - CurrentLocation.Z) / EstimatedArrivalSeconds,
		-FMath::Max(10.f, SteeringMaxVerticalSpeed),
		FMath::Max(10.f, SteeringMaxVerticalSpeed));
	const float NewVerticalSpeed = FMath::FInterpConstantTo(
		SteeringVelocity.Z,
		DesiredVerticalSpeed,
		DeltaSeconds,
		FMath::Max(10.f, SteeringAcceleration));

	const FVector HorizontalDirection = FRotator(0.f, NewYaw, 0.f).Vector();
	SteeringVelocity = HorizontalDirection * NewHorizontalSpeed + FVector::UpVector * NewVerticalSpeed;
	const FVector NewLocation = CurrentLocation + SteeringVelocity * DeltaSeconds;

	const float SpeedDelta = NewHorizontalSpeed - CurrentHorizontalSpeed;
	const float LongitudinalRate = SpeedDelta >= 0.f
		? FMath::Max(10.f, SteeringAcceleration)
		: FMath::Max(10.f, SteeringDeceleration);
	const float LongitudinalInput = FMath::Clamp(
		SpeedDelta / FMath::Max(UE_SMALL_NUMBER, LongitudinalRate * DeltaSeconds),
		-1.f,
		1.f);
	const float VerticalInput = FMath::Clamp(
		NewVerticalSpeed / FMath::Max(10.f, SteeringMaxVerticalSpeed),
		-1.f,
		1.f);
	const float PitchInput = FMath::Clamp(
		VerticalInput * SteeringVerticalPitchWeight
			- LongitudinalInput * SteeringAccelerationPitchWeight,
		-1.f,
		1.f);
	const float DesiredPitch = PitchInput * FMath::Max(0.f, SteeringMaxPitchAngle);
	// UE/vendor airframe roll convention in this project banks into a positive-Yaw turn.
	const float DesiredBank = TurnInput * FMath::Max(0.f, SteeringMaxBankAngle);
	const FRotator CurrentRotation = GetActorRotation();
	const float NewPitch = FMath::FInterpTo(
		CurrentRotation.Pitch, DesiredPitch, DeltaSeconds, FMath::Max(0.1f, SteeringAttitudeInterpSpeed));
	const float NewBank = FMath::FInterpTo(
		CurrentRotation.Roll, DesiredBank, DeltaSeconds, FMath::Max(0.1f, SteeringAttitudeInterpSpeed));
	SetActorLocationAndRotation(
		NewLocation,
		FRotator(NewPitch, NewYaw, NewBank),
		false,
		nullptr,
		ETeleportType::None);

	ReplicatedSteeringVelocity = SteeringVelocity;
	ReplicatedSteeringTurnInput = TurnInput;
	ReplicatedSteeringPitchInput = PitchInput;

#if ENABLE_DRAW_DEBUG
	if (bDrawSteeringDebug && GetWorld())
	{
		DrawDebugLine(GetWorld(), CurrentLocation, PredictedLocation, FColor::Cyan, false, 0.f, 0, 8.f);
		DrawDebugDirectionalArrow(GetWorld(), PredictedLocation, SteeringTarget, 500.f,
			bSteeringFinalLeg ? FColor::Green : FColor::Yellow, false, 0.f, 0, 8.f);
		DrawDebugSphere(GetWorld(), SteeringApproachGate, SteeringGateAcceptanceRadius, 16,
			FColor::Yellow, false, 0.f, 0, 5.f);
		DrawDebugSphere(GetWorld(), TargetLocation, SteeringArrivalRadius, 16,
			FColor::Green, false, 0.f, 0, 5.f);
	}
#endif

	if (MotionElapsed - SteeringLastLogTime >= 1.f)
	{
		SteeringLastLogTime = MotionElapsed;
		UE_LOG(LogOBHelicopterInsertion, Display,
			TEXT("[HelicopterSteering] Tick Helicopter=%s FinalLeg=%s Location=%s Target=%s Speed=%.0f DesiredSpeed=%.0f Vertical=%.0f YawError=%.1f Turn=%.2f Pitch=%.2f Distance=%.0f Elapsed=%.1f"),
			*GetName(), bSteeringFinalLeg ? TEXT("true") : TEXT("false"),
			*CurrentLocation.ToCompactString(), *SteeringTarget.ToCompactString(),
			NewHorizontalSpeed, DesiredHorizontalSpeed, NewVerticalSpeed,
			UnclampedYawDelta, TurnInput, PitchInput, DistanceToFinalTarget, MotionElapsed);
	}

	const float NewDistanceToTarget = FVector::Distance(NewLocation, TargetLocation);
	const bool bCrossedTarget = bSteeringFinalLeg
		&& NewDistanceToTarget > SteeringPreviousDistance
		&& SteeringPreviousDistance <= SteeringArrivalRadius * 1.5f;
	const bool bArrived = bSteeringFinalLeg
		&& (NewDistanceToTarget <= FMath::Max(50.f, SteeringArrivalRadius) || bCrossedTarget);
	const float HardTimeout = FMath::Max(10.f, MotionDuration * FMath::Max(1.f, SteeringTimeoutMultiplier));
	const bool bTimedOut = MotionElapsed >= HardTimeout;
	SteeringPreviousDistance = NewDistanceToTarget;

	if (bArrived || bTimedOut)
	{
		if (bTimedOut && !bArrived)
		{
			UE_LOG(LogOBHelicopterInsertion, Error,
				TEXT("[HelicopterSteering] Hard timeout; snapping to target Helicopter=%s Distance=%.0f Elapsed=%.1f Timeout=%.1f"),
				*GetName(), NewDistanceToTarget, MotionElapsed, HardTimeout);
		}
		else
		{
			UE_LOG(LogOBHelicopterInsertion, Display,
				TEXT("[HelicopterSteering] Arrived Helicopter=%s Distance=%.0f Elapsed=%.2f FinalSpeed=%.0f"),
				*GetName(), NewDistanceToTarget, MotionElapsed, SteeringVelocity.Size());
		}

		SetActorTransform(MotionTargetTransform, false, nullptr, ETeleportType::None);
		const uint8 CompletedTask = MotionCompletionTask;
		bMotionActive = false;
		bSteeringMotion = false;
		SteeringVelocity = FVector::ZeroVector;
		ReplicatedSteeringVelocity = FVector::ZeroVector;
		ReplicatedSteeringTurnInput = 0.f;
		ReplicatedSteeringPitchInput = 0.f;
		MotionCompletionTask = OBHelicopterTasks::None;
		ForceNetUpdate();
		HandleMotionCompleted(CompletedTask);
	}
}

void AOBInsertionHelicopter::TickOrbit(float DeltaSeconds)
{
	if (ActiveRoute.IsValid() && ActiveRoute->GetRouteLength() > 1.f)
	{
		RouteProgress = FMath::Fmod(RouteProgress + DeltaSeconds * OrbitSpeed / ActiveRoute->GetRouteLength(), 1.f);
		SetActorTransform(ActiveRoute->GetRouteTransform(RouteProgress), false, nullptr, ETeleportType::None);
		return;
	}

	OrbitAngleRadians = FMath::Fmod(
		OrbitAngleRadians + DeltaSeconds * OrbitSpeed / FMath::Max(100.f, DefaultOrbitRadius),
		2.f * UE_PI);
	const FVector Location = ProceduralOrbitCenter + FVector(
		FMath::Cos(OrbitAngleRadians) * DefaultOrbitRadius,
		FMath::Sin(OrbitAngleRadians) * DefaultOrbitRadius,
		0.f);
	const FVector Tangent(-FMath::Sin(OrbitAngleRadians), FMath::Cos(OrbitAngleRadians), 0.f);
	SetActorLocationAndRotation(Location, Tangent.Rotation(), false, nullptr, ETeleportType::None);
}

void AOBInsertionHelicopter::HandleMotionCompleted(uint8 CompletionTask)
{
	switch (CompletionTask)
	{
	case OBHelicopterTasks::InsertionApproach:
		SetInsertionPhase(EOBInsertionPhase::Scanning);
		GetWorldTimerManager().SetTimer(SequenceTimer, this, &AOBInsertionHelicopter::CompleteInsertionScan,
			FMath::Max(0.01f, ScanDuration), false);
		break;
	case OBHelicopterTasks::InsertionHover:
		SetInsertionPhase(EOBInsertionPhase::Hovering);
		SetDoorsOpen(true);
		GroundDustComponent->Activate(true);
		GetWorldTimerManager().SetTimer(SequenceTimer, this, &AOBInsertionHelicopter::StartInsertionRappel,
			FMath::Max(0.01f, DoorOpenDelay), false);
		break;
	case OBHelicopterTasks::InsertionDeparture:
		FinishInsertionDeparture();
		break;
	case OBHelicopterTasks::ExtractionApproach:
		SetActorTransform(MotionTargetTransform);
		SetExtractionPhase(EOBExtractionCallPhase::Landing);
		GroundDustComponent->Activate(true);
		GetWorldTimerManager().SetTimer(SequenceTimer, this, &AOBInsertionHelicopter::OpenExtractionBoarding,
			FMath::Max(0.01f, ExtractionLandingSettleSeconds), false);
		break;
	case OBHelicopterTasks::ExtractionDeparture:
		FinishExtractionDeparture();
		break;
	default:
		break;
	}
}

void AOBInsertionHelicopter::CompleteInsertionScan()
{
	if (!HasAuthority() || !ActiveLandingZone.bValid)
	{
		return;
	}
	BeginTransformMotion(ActiveLandingZone.HoverTransform, HoverTransitionSeconds,
		OBHelicopterTasks::InsertionHover);
}

void AOBInsertionHelicopter::StartInsertionRappel()
{
	if (!HasAuthority())
	{
		return;
	}

	SetInsertionPhase(EOBInsertionPhase::Rappelling);
	RappelQueue.Reset();
	for (AController* Controller : PassengerControllers)
	{
		RappelQueue.Add(Controller);
	}
	ActiveRappels.Reset();
	RappelStartAccumulator = RappelStaggerSeconds;
	RappelSequenceIndex = 0;

	if (RappelQueue.IsEmpty())
	{
		OnAllPassengersDeployed.Broadcast(this);
		SetInsertionPhase(EOBInsertionPhase::Departing);
		SetDoorsOpen(false);
		GroundDustComponent->Deactivate();
		FTransform ExitTransform = GetActorTransform();
		ExitTransform.AddToTranslation(ExitTransform.TransformVectorNoScale(InsertionDepartureOffset));
		BeginTransformMotion(ExitTransform, InsertionDepartureSeconds, OBHelicopterTasks::InsertionDeparture);
	}
}

void AOBInsertionHelicopter::TickRappels(float DeltaSeconds)
{
	RappelStartAccumulator += DeltaSeconds;
	if (!RappelQueue.IsEmpty() && ActiveRappels.Num() < MaxSimultaneousRappels
		&& (ActiveRappels.IsEmpty() || RappelStartAccumulator >= RappelStaggerSeconds))
	{
		StartNextRappel();
		RappelStartAccumulator = 0.f;
	}

	for (int32 Index = ActiveRappels.Num() - 1; Index >= 0; --Index)
	{
		FActiveRappel& Rappel = ActiveRappels[Index];
		AController* Controller = Rappel.Controller.Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn)
		{
			ActiveRappels.RemoveAtSwap(Index);
			continue;
		}

		Rappel.Elapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(Rappel.Elapsed / Rappel.Duration, 0.f, 1.f);
		Pawn->SetActorLocation(FMath::Lerp(Rappel.Start, Rappel.End, Alpha), false, nullptr, ETeleportType::None);
		if (Alpha >= 1.f)
		{
			FinishRappel(Index);
		}
	}

	if (RappelQueue.IsEmpty() && ActiveRappels.IsEmpty() && PassengerControllers.IsEmpty())
	{
		OnAllPassengersDeployed.Broadcast(this);
		SetInsertionPhase(EOBInsertionPhase::Departing);
		SetDoorsOpen(false);
		GroundDustComponent->Deactivate();
		FTransform ExitTransform = GetActorTransform();
		ExitTransform.AddToTranslation(ExitTransform.TransformVectorNoScale(InsertionDepartureOffset));
		BeginTransformMotion(ExitTransform, InsertionDepartureSeconds, OBHelicopterTasks::InsertionDeparture);
	}
}

void AOBInsertionHelicopter::StartNextRappel()
{
	if (RappelQueue.IsEmpty())
	{
		return;
	}

	AController* Controller = RappelQueue[0].Get();
	RappelQueue.RemoveAt(0);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const int32 RopeIndex = RappelSequenceIndex++ % 2;
	USceneComponent* RopeAnchor = RopeIndex == 0 ? LeftRappelAnchor : RightRappelAnchor;
	const FVector Start = RopeAnchor ? RopeAnchor->GetComponentLocation() : GetActorLocation();
	const int32 LandingIndex = RappelSequenceIndex - 1;
	const int32 Row = LandingIndex / 2;
	const float Side = RopeIndex == 0 ? -1.f : 1.f;
	const FVector HorizontalOffset(Row * LandingSlotSpacing, Side * LandingSlotSpacing, 0.f);

	float CapsuleHalfHeight = 90.f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const FVector End = FVector(ResolvedGroundLocation) + HorizontalOffset + FVector(0.f, 0.f, CapsuleHalfHeight + 5.f);

	Pawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Pawn->SetReplicateMovement(true);
	Pawn->SetActorEnableCollision(false);
	Pawn->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
	SetPassengerTransitState(Controller, true, Pawn);

	FActiveRappel& Rappel = ActiveRappels.AddDefaulted_GetRef();
	Rappel.Controller = Controller;
	Rappel.Start = Start;
	Rappel.End = End;
	Rappel.Duration = FMath::Max(0.1f, FVector::Distance(Start, End) / FMath::Max(50.f, RappelSpeed));
	Rappel.RopeIndex = RopeIndex;
	SetReplicatedPassengerRappelling(Pawn, RopeIndex, Start, End);
	UE_LOG(LogOBHelicopterInsertion, Log,
		TEXT("[Insertion] Rappel started Helicopter=%s Team=%d Controller=%s Pawn=%s Rope=%d Start=%s End=%s Duration=%.2f Queue=%d Active=%d"),
		*GetName(), TeamId, *GetNameSafe(Controller), *GetNameSafe(Pawn), RopeIndex,
		*Start.ToCompactString(), *End.ToCompactString(), Rappel.Duration, RappelQueue.Num(), ActiveRappels.Num());

	BP_OnRappelLineChanged(RopeIndex, true, Start, End);
	BP_OnPassengerRappelStarted(Controller, RopeIndex);
}

void AOBInsertionHelicopter::FinishRappel(int32 ActiveIndex)
{
	if (!ActiveRappels.IsValidIndex(ActiveIndex))
	{
		return;
	}

	const FActiveRappel Rappel = ActiveRappels[ActiveIndex];
	AController* Controller = Rappel.Controller.Get();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (Pawn)
	{
		Pawn->SetActorLocation(Rappel.End, false, nullptr, ETeleportType::TeleportPhysics);
		Pawn->SetActorEnableCollision(true);
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			if (AOBCharacterBase* OBCharacter = Cast<AOBCharacterBase>(Character))
			{
				OBCharacter->HoldUntilGrounded();
			}
		}
		SetPassengerTransitState(Controller, false, Pawn);
		UE_LOG(LogOBHelicopterInsertion, Log,
			TEXT("[InsertionInput] Rappel restore Controller=%s Pawn=%s Ground=%s MovementMode=%d Collision=%s"),
			*GetNameSafe(Controller), *Pawn->GetName(), *Rappel.End.ToCompactString(),
			Cast<ACharacter>(Pawn) ? static_cast<int32>(Cast<ACharacter>(Pawn)->GetCharacterMovement()->MovementMode) : -1,
			Pawn->GetActorEnableCollision() ? TEXT("true") : TEXT("false"));
	}

	BP_OnRappelLineChanged(Rappel.RopeIndex, false, Rappel.Start, Rappel.End);
	BP_OnPassengerLanded(Controller);
	OnPassengerDeployed.Broadcast(this, Controller);
	RemoveReplicatedPassenger(Pawn);

	const int32 PassengerIndex = PassengerControllers.IndexOfByKey(Controller);
	if (PassengerIndex != INDEX_NONE)
	{
		PassengerControllers.RemoveAt(PassengerIndex);
		PassengerSeatIndices.RemoveAt(PassengerIndex);
	}
	ActiveRappels.RemoveAt(ActiveIndex);
}

void AOBInsertionHelicopter::OpenExtractionBoarding()
{
	if (!HasAuthority())
	{
		return;
	}
	SetDoorsOpen(true);
	SetExtractionPhase(EOBExtractionCallPhase::Boarding);
	OnExtractionBoardingReady.Broadcast(this);
}

void AOBInsertionHelicopter::FinishInsertionDeparture()
{
	SetInsertionPhase(EOBInsertionPhase::Completed);
	SetLifeSpan(2.f);
}

void AOBInsertionHelicopter::FinishExtractionDeparture()
{
	SetExtractionPhase(EOBExtractionCallPhase::Completed);
	OnExtractionDepartureCompleted.Broadcast(this);
	SetLifeSpan(3.f);
}

void AOBInsertionHelicopter::SetPassengerTransitState(AController* Controller, bool bInTransit, AActor* ViewTarget)
{
	if (AOBPlayerController* PlayerController = Cast<AOBPlayerController>(Controller))
	{
		PlayerController->SetHelicopterTransitView(ViewTarget, bInTransit);
	}
}

void AOBInsertionHelicopter::AddReplicatedSeatedPassenger(APawn* Pawn, int32 SeatIndex)
{
	if (!HasAuthority() || !Pawn)
	{
		return;
	}

	FOBHelicopterPassengerNetState* Existing = ReplicatedPassengerStates.FindByPredicate(
		[Pawn](const FOBHelicopterPassengerNetState& State) { return State.Pawn == Pawn; });
	if (!Existing)
	{
		Existing = &ReplicatedPassengerStates.AddDefaulted_GetRef();
	}
	Existing->Pawn = Pawn;
	Existing->SeatIndex = SeatIndex;
	Existing->Phase = EOBHelicopterPassengerPhase::Seated;
	Existing->RopeIndex = INDEX_NONE;
	Existing->RopeStart = FVector::ZeroVector;
	Existing->RopeEnd = FVector::ZeroVector;
	ForceNetUpdate();
}

void AOBInsertionHelicopter::SetReplicatedPassengerRappelling(
	APawn* Pawn,
	int32 RopeIndex,
	const FVector& RopeStart,
	const FVector& RopeEnd)
{
	if (!HasAuthority() || !Pawn)
	{
		return;
	}

	FOBHelicopterPassengerNetState* Existing = ReplicatedPassengerStates.FindByPredicate(
		[Pawn](const FOBHelicopterPassengerNetState& State) { return State.Pawn == Pawn; });
	if (!Existing)
	{
		Existing = &ReplicatedPassengerStates.AddDefaulted_GetRef();
		Existing->Pawn = Pawn;
	}
	Existing->Phase = EOBHelicopterPassengerPhase::Rappelling;
	Existing->RopeIndex = RopeIndex;
	Existing->RopeStart = RopeStart;
	Existing->RopeEnd = RopeEnd;
	ForceNetUpdate();
}

void AOBInsertionHelicopter::RemoveReplicatedPassenger(APawn* Pawn)
{
	if (!HasAuthority() || !Pawn)
	{
		return;
	}
	ReplicatedPassengerStates.RemoveAll(
		[Pawn](const FOBHelicopterPassengerNetState& State) { return State.Pawn == Pawn; });
	ForceNetUpdate();
}

void AOBInsertionHelicopter::OnRep_Mission()
{
	BP_OnMissionChanged(Mission);
}

void AOBInsertionHelicopter::OnRep_InsertionPhase()
{
	if (InsertionPhase == EOBInsertionPhase::Hovering || InsertionPhase == EOBInsertionPhase::Rappelling)
	{
		GroundDustComponent->Activate(true);
	}
	else if (InsertionPhase == EOBInsertionPhase::Departing || InsertionPhase == EOBInsertionPhase::Completed)
	{
		GroundDustComponent->Deactivate();
	}
	BP_OnInsertionPhaseChanged(InsertionPhase);
}

void AOBInsertionHelicopter::OnRep_ExtractionPhase()
{
	if (ExtractionPhase == EOBExtractionCallPhase::Landing || ExtractionPhase == EOBExtractionCallPhase::Boarding)
	{
		GroundDustComponent->Activate(true);
	}
	else if (ExtractionPhase == EOBExtractionCallPhase::Departing || ExtractionPhase == EOBExtractionCallPhase::Completed)
	{
		GroundDustComponent->Deactivate();
	}
	BP_OnExtractionPhaseChanged(ExtractionPhase);
}

void AOBInsertionHelicopter::OnRep_DoorsOpen()
{
	BP_OnDoorsChanged(bDoorsOpen);
}

void AOBInsertionHelicopter::OnRep_PassengerStates()
{
	auto FindPresented = [this](const APawn* Pawn) -> const FOBHelicopterPassengerNetState*
	{
		return PresentedPassengerStates.FindByPredicate(
			[Pawn](const FOBHelicopterPassengerNetState& State) { return State.Pawn == Pawn; });
	};

	for (const FOBHelicopterPassengerNetState& Previous : PresentedPassengerStates)
	{
		const bool bStillPresent = ReplicatedPassengerStates.ContainsByPredicate(
			[Pawn = Previous.Pawn](const FOBHelicopterPassengerNetState& State) { return State.Pawn == Pawn; });
		if (!bStillPresent && Previous.Pawn)
		{
			if (Previous.Phase == EOBHelicopterPassengerPhase::Rappelling)
			{
				BP_OnRappelLineChanged(Previous.RopeIndex, false,
					FVector(Previous.RopeStart), FVector(Previous.RopeEnd));
			}
			BP_OnReplicatedPassengerLanded(Previous.Pawn);
			if (AController* LocalController = Previous.Pawn->GetController();
				LocalController && LocalController->IsLocalController())
			{
				BP_OnPassengerLanded(LocalController);
			}
		}
	}

	for (const FOBHelicopterPassengerNetState& Current : ReplicatedPassengerStates)
	{
		if (!Current.Pawn)
		{
			continue;
		}
		const FOBHelicopterPassengerNetState* Previous = FindPresented(Current.Pawn);
		if (!Previous && Current.Phase == EOBHelicopterPassengerPhase::Seated)
		{
			BP_OnReplicatedPassengerSeated(Current.Pawn, Current.SeatIndex);
			if (AController* LocalController = Current.Pawn->GetController();
				LocalController && LocalController->IsLocalController())
			{
				BP_OnPassengerSeated(LocalController, Current.SeatIndex);
			}
		}

		const bool bRappelChanged = Current.Phase == EOBHelicopterPassengerPhase::Rappelling
			&& (!Previous
				|| Previous->Phase != EOBHelicopterPassengerPhase::Rappelling
				|| Previous->RopeIndex != Current.RopeIndex
				|| Previous->RopeStart != Current.RopeStart
				|| Previous->RopeEnd != Current.RopeEnd);
		if (bRappelChanged)
		{
			BP_OnRappelLineChanged(Current.RopeIndex, true,
				FVector(Current.RopeStart), FVector(Current.RopeEnd));
			BP_OnReplicatedPassengerRappelStarted(Current.Pawn, Current.RopeIndex,
				FVector(Current.RopeStart), FVector(Current.RopeEnd));
			if (AController* LocalController = Current.Pawn->GetController();
				LocalController && LocalController->IsLocalController())
			{
				BP_OnPassengerRappelStarted(LocalController, Current.RopeIndex);
			}
		}
	}

	PresentedPassengerStates = ReplicatedPassengerStates;
	UE_LOG(LogOBHelicopterInsertion, Verbose,
		TEXT("[InsertionNet] Passenger state applied Client Helicopter=%s Team=%d Count=%d"),
		*GetName(), TeamId, ReplicatedPassengerStates.Num());
}
