#include "Game/Expedition/OBInsertionHelicopter.h"

#include "Camera/CameraComponent.h"
#include "Character/OBCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Game/Expedition/OBExtractionZone.h"
#include "Game/Expedition/OBHelicopterRoute.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "Player/Controller/OBPlayerController.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

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

void AOBInsertionHelicopter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOBInsertionHelicopter, Mission);
	DOREPLIFETIME(AOBInsertionHelicopter, InsertionPhase);
	DOREPLIFETIME(AOBInsertionHelicopter, ExtractionPhase);
	DOREPLIFETIME(AOBInsertionHelicopter, TeamId);
	DOREPLIFETIME(AOBInsertionHelicopter, ResolvedGroundLocation);
	DOREPLIFETIME(AOBInsertionHelicopter, bDoorsOpen);
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
	BeginTransformMotion(ScanTransform, InsertionApproachSeconds, OBHelicopterTasks::InsertionApproach);
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
	SetPassengerTransitState(Controller, true, this);
	BP_OnPassengerSeated(Controller, FreeSeat);
	return true;
}

void AOBInsertionHelicopter::ReleaseAllPassengers(const FVector& GroundCenter)
{
	if (!HasAuthority())
	{
		return;
	}

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
}

void AOBInsertionHelicopter::SetInsertionPhase(EOBInsertionPhase NewPhase)
{
	if (InsertionPhase == NewPhase)
	{
		return;
	}
	InsertionPhase = NewPhase;
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
}

void AOBInsertionHelicopter::TickMotion(float DeltaSeconds)
{
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
	}

	BP_OnRappelLineChanged(Rappel.RopeIndex, false, Rappel.Start, Rappel.End);
	BP_OnPassengerLanded(Controller);
	OnPassengerDeployed.Broadcast(this, Controller);

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
