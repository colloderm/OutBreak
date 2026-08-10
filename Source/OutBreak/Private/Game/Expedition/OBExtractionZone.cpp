#include "Game/Expedition/OBExtractionZone.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Game/Expedition/OBExtractionSite.h"
#include "Game/Expedition/OBHelicopterRoute.h"
#include "Game/Expedition/OBInsertionHelicopter.h"
#include "Game/Expedition/OBSignalFlare.h"
#include "Game/GameMode/OBExpeditionGameMode.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Player/State/OBPlayerStateBase.h"
#include "TimerManager.h"

AOBExtractionZone::AOBExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	// Calls and countdowns must exist even before this area is streamed around a player.
	bIsSpatiallyLoaded = false;
	// Public call state drives map/HUD countdowns even when the site is far away.
	// Personal zones still apply the team filter in IsNetRelevantFor.
	bAlwaysRelevant = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("CallTrigger"));
	SetRootComponent(Trigger);
	Trigger->InitSphereRadius(300.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	BoardingTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("BoardingTrigger"));
	BoardingTrigger->SetupAttachment(Trigger);
	BoardingTrigger->InitSphereRadius(500.f);
	BoardingTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	BoardingTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardingTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoardingTrigger->SetGenerateOverlapEvents(true);
	BoardingTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LandingAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("LandingAnchor"));
	LandingAnchor->SetupAttachment(Trigger);

	FlareLaunchAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("FlareLaunchAnchor"));
	FlareLaunchAnchor->SetupAttachment(Trigger);
	FlareLaunchAnchor->SetRelativeLocation(FVector(0.f, 0.f, 100.f));

	SignalFlareClass = AOBSignalFlare::StaticClass();
}

void AOBExtractionZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOBExtractionZone, CallState);
	DOREPLIFETIME(AOBExtractionZone, OwningTeamId);
}

void AOBExtractionZone::ConfigureAsPersonal(uint8 InTeamId)
{
	OwningTeamId = InTeamId;
	ExtractType = EOBExtractionType::Personal;
	bOnlyRelevantToOwner = false;
}

void AOBExtractionZone::ConfigureExtractionSite(AOBExtractionSite* Site)
{
	if (!Site)
	{
		return;
	}
	LandingAnchor->SetWorldTransform(Site->GetLandingTransform());
	FlareLaunchAnchor->SetWorldTransform(Site->GetFlareTransform());
	ApproachRoute = Site->GetApproachRoute();
	ExitRoute = Site->GetExitRoute();
}

bool AOBExtractionZone::IsNetRelevantFor(
	const AActor* RealViewer,
	const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	if (OwningTeamId == 0)
	{
		return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	}

	const AController* Controller = Cast<AController>(RealViewer);
	if (!Controller)
	{
		if (const APawn* Pawn = Cast<APawn>(ViewTarget))
		{
			Controller = Pawn->GetController();
		}
	}
	const AOBPlayerStateBase* PS = Controller ? Controller->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	return PS && PS->GetTeamId() == OwningTeamId;
}

void AOBExtractionZone::SetActiveWindow(int32 InStartSec, int32 InEndSec)
{
	ActiveStartSec = InStartSec;
	ActiveEndSec = InEndSec;
}

void AOBExtractionZone::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AOBExtractionZone::OnCallTriggerBeginOverlap);
		BoardingTrigger->OnComponentBeginOverlap.AddDynamic(this, &AOBExtractionZone::OnBoardingTriggerBeginOverlap);
		BoardingTrigger->OnComponentEndOverlap.AddDynamic(this, &AOBExtractionZone::OnBoardingTriggerEndOverlap);
	}

	CheckActiveState();
	GetWorldTimerManager().SetTimer(ActiveCheckTimer, this, &AOBExtractionZone::CheckActiveState, 1.f, true);
}

void AOBExtractionZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	const float Now = GetServerTime();
	switch (CallState.Phase)
	{
	case EOBExtractionCallPhase::FlareLaunched:
		if (Now >= CallState.CallStartedServerTime + 1.f)
		{
			SetCallPhase(EOBExtractionCallPhase::Waiting);
		}
		break;
	case EOBExtractionCallPhase::Waiting:
		if (Now >= CallState.ArrivalServerTime - InboundLeadTime)
		{
			SpawnExtractionHelicopter();
		}
		break;
	case EOBExtractionCallPhase::Boarding:
		ProcessBoarding(DeltaSeconds);
		if (Now >= CallState.BoardingEndsServerTime)
		{
			StartDeparture();
		}
		break;
	case EOBExtractionCallPhase::Completed:
	case EOBExtractionCallPhase::Aborted:
		if (bReusable && Now >= CooldownEndsServerTime)
		{
			SetCallPhase(EOBExtractionCallPhase::Cooldown);
			CooldownEndsServerTime = Now + CooldownSeconds;
		}
		break;
	case EOBExtractionCallPhase::Cooldown:
		if (Now >= CooldownEndsServerTime)
		{
			ResetForReuse();
		}
		break;
	default:
		break;
	}
}

void AOBExtractionZone::OnCallTriggerBeginOverlap(
	UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority() || CallState.Phase != EOBExtractionCallPhase::Ready || !IsActiveNow())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(OtherActor);
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (CanPlayerExtract(Controller))
	{
		StartExtractionCall(Controller);
	}
}

void AOBExtractionZone::OnBoardingTriggerBeginOverlap(
	UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority() || CallState.Phase != EOBExtractionCallPhase::Boarding)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(OtherActor);
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (CanPlayerExtract(Controller) && !BoardedControllers.Contains(Controller))
	{
		BoardingProgress.FindOrAdd(Controller) = 0.f;
	}
}

void AOBExtractionZone::OnBoardingTriggerEndOverlap(
	UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}
	BoardingProgress.Remove(Controller);
	if (AOBPlayerStateBase* PS = Controller->GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetExtractionProgress(0.f, false);
	}
}

void AOBExtractionZone::StartExtractionCall(AController* CallingController)
{
	AOBPlayerStateBase* PS = CallingController ? CallingController->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	if (!PS || CallState.Phase != EOBExtractionCallPhase::Ready)
	{
		return;
	}

	const float Now = GetServerTime();
	CallState = FOBExtractionCallState();
	CallState.CallingTeamId = PS->GetTeamId();
	CallState.CallStartedServerTime = Now;
	CallState.ArrivalServerTime = Now + HelicopterCallDelay;
	CallState.Phase = EOBExtractionCallPhase::FlareLaunched;
	ForceNetUpdate();
	BP_OnCallPhaseChanged(CallState.Phase);

	if (SignalFlareClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActiveFlare = GetWorld()->SpawnActor<AOBSignalFlare>(SignalFlareClass,
			FlareLaunchAnchor->GetComponentTransform(), Params);
	}
	BP_OnFlareLaunched(ActiveFlare);
	SetActorTickEnabled(true);
}

void AOBExtractionZone::SpawnExtractionHelicopter()
{
	if (CallState.Helicopter)
	{
		return;
	}

	TSubclassOf<AOBInsertionHelicopter> HelicopterClass = ExtractionHelicopterClass;
	if (!HelicopterClass)
	{
		if (const AOBExpeditionGameMode* GM = GetWorld()->GetAuthGameMode<AOBExpeditionGameMode>())
		{
			HelicopterClass = GM->GetDefaultExtractionHelicopterClass();
		}
	}
	if (!HelicopterClass)
	{
		AbortExtraction(TEXT("No extraction helicopter class is configured."));
		return;
	}

	const FTransform LandingTransform = LandingAnchor->GetComponentTransform();
	FTransform SpawnTransform = LandingTransform;
	if (ApproachRoute && ApproachRoute->GetRouteLength() > 1.f)
	{
		SpawnTransform = ApproachRoute->GetRouteTransform(0.f);
	}
	else
	{
		SpawnTransform.AddToTranslation(SpawnTransform.TransformVectorNoScale(HelicopterSpawnOffset));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AOBInsertionHelicopter* Helicopter = GetWorld()->SpawnActor<AOBInsertionHelicopter>(
		HelicopterClass, SpawnTransform, Params);
	if (!Helicopter)
	{
		AbortExtraction(TEXT("Failed to spawn extraction helicopter."));
		return;
	}

	Helicopter->OnExtractionPhaseChanged.AddDynamic(this, &AOBExtractionZone::HandleHelicopterPhaseChanged);
	Helicopter->OnExtractionBoardingReady.AddDynamic(this, &AOBExtractionZone::HandleHelicopterBoardingReady);
	Helicopter->OnExtractionDepartureCompleted.AddDynamic(this, &AOBExtractionZone::HandleHelicopterDepartureCompleted);
	Helicopter->InitializeExtraction(this);
	CallState.Helicopter = Helicopter;
	SetCallPhase(EOBExtractionCallPhase::Inbound);
	BP_OnHelicopterSpawned(Helicopter);

	const float RemainingApproachTime = FMath::Max(0.1f, CallState.ArrivalServerTime - GetServerTime());
	Helicopter->BeginExtractionApproach(ApproachRoute, LandingTransform, RemainingApproachTime);
}

void AOBExtractionZone::HandleHelicopterPhaseChanged(
	AOBInsertionHelicopter*, EOBExtractionCallPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}
	if (NewPhase == EOBExtractionCallPhase::Inbound
		|| NewPhase == EOBExtractionCallPhase::Landing
		|| NewPhase == EOBExtractionCallPhase::Boarding
		|| NewPhase == EOBExtractionCallPhase::Departing)
	{
		SetCallPhase(NewPhase);
	}
}

void AOBExtractionZone::HandleHelicopterBoardingReady(AOBInsertionHelicopter*)
{
	OpenBoardingWindow();
}

void AOBExtractionZone::OpenBoardingWindow()
{
	if (!HasAuthority())
	{
		return;
	}
	CallState.BoardingEndsServerTime = GetServerTime() + BoardingWindowSeconds;
	SetCallPhase(EOBExtractionCallPhase::Boarding);
	BoardingTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	TArray<AActor*> OverlappingActors;
	BoardingTrigger->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		AController* Controller = Pawn ? Pawn->GetController() : nullptr;
		if (CanPlayerExtract(Controller) && !BoardedControllers.Contains(Controller))
		{
			BoardingProgress.FindOrAdd(Controller) = 0.f;
		}
	}
}

void AOBExtractionZone::ProcessBoarding(float DeltaSeconds)
{
	TArray<AController*> Finished;
	for (TPair<TObjectPtr<AController>, float>& Pair : BoardingProgress)
	{
		AController* Controller = Pair.Key;
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		AOBPlayerStateBase* PS = Controller ? Controller->GetPlayerState<AOBPlayerStateBase>() : nullptr;
		if (!Pawn || !PS || !BoardingTrigger->IsOverlappingActor(Pawn) || !CanPlayerExtract(Controller))
		{
			Pair.Value = 0.f;
			if (PS)
			{
				PS->SetExtractionProgress(0.f, false);
			}
			continue;
		}

		Pair.Value += DeltaSeconds;
		PS->SetExtractionProgress(FMath::Clamp(Pair.Value / HoldTime, 0.f, 1.f), true);
		if (Pair.Value >= HoldTime)
		{
			Finished.Add(Controller);
		}
	}

	for (AController* Controller : Finished)
	{
		BoardPassenger(Controller);
	}
}

void AOBExtractionZone::BoardPassenger(AController* Controller)
{
	if (!Controller || !CallState.Helicopter || BoardedControllers.Contains(Controller))
	{
		return;
	}
	if (!CallState.Helicopter->SeatPassenger(Controller))
	{
		return;
	}

	BoardedControllers.Add(Controller);
	BoardingProgress.Remove(Controller);
	CallState.BoardedCount = BoardedControllers.Num();
	ForceNetUpdate();
	if (AOBPlayerStateBase* PS = Controller->GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetExtractionProgress(0.f, false);
	}
	BP_OnPassengerBoarded(Controller);
}

void AOBExtractionZone::StartDeparture()
{
	if (!HasAuthority() || CallState.Phase == EOBExtractionCallPhase::Departing)
	{
		return;
	}

	BoardingTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (const TPair<TObjectPtr<AController>, float>& Pair : BoardingProgress)
	{
		if (AOBPlayerStateBase* PS = Pair.Key ? Pair.Key->GetPlayerState<AOBPlayerStateBase>() : nullptr)
		{
			PS->SetExtractionProgress(0.f, false);
		}
	}
	BoardingProgress.Reset();
	SetCallPhase(EOBExtractionCallPhase::Departing);

	if (CallState.Helicopter)
	{
		CallState.Helicopter->BeginExtractionDeparture(ExitRoute, DepartureSeconds);
	}
	else
	{
		AbortExtraction(TEXT("Extraction helicopter disappeared before departure."));
	}
}

void AOBExtractionZone::HandleHelicopterDepartureCompleted(AOBInsertionHelicopter*)
{
	FinishExtractionFlight();
}

void AOBExtractionZone::FinishExtractionFlight()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<TObjectPtr<AController>> SettledControllers = BoardedControllers;
	BoardedControllers.Reset();
	if (AOBExpeditionGameMode* GM = GetWorld()->GetAuthGameMode<AOBExpeditionGameMode>())
	{
		for (AController* Controller : SettledControllers)
		{
			GM->NotifyPlayerExtracted(Controller);
		}
	}

	SetCallPhase(EOBExtractionCallPhase::Completed);
	CooldownEndsServerTime = GetServerTime() + 2.f;
	if (!bReusable)
	{
		SetActorTickEnabled(false);
	}
}

void AOBExtractionZone::AbortExtraction(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("[Extraction] %s aborted: %s"), *GetName(), *Reason);
	if (CallState.Helicopter)
	{
		CallState.Helicopter->ReleaseAllPassengers(LandingAnchor->GetComponentLocation());
		CallState.Helicopter->Destroy();
	}
	BoardedControllers.Reset();
	BoardingProgress.Reset();
	BoardingTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCallPhase(EOBExtractionCallPhase::Aborted);
	CooldownEndsServerTime = GetServerTime() + 2.f;
}

void AOBExtractionZone::ResetForReuse()
{
	CallState = FOBExtractionCallState();
	ActiveFlare = nullptr;
	BoardedControllers.Reset();
	BoardingProgress.Reset();
	BoardingTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForceNetUpdate();
	BP_OnCallPhaseChanged(CallState.Phase);
	SetActorTickEnabled(false);
}

void AOBExtractionZone::SetCallPhase(EOBExtractionCallPhase NewPhase)
{
	if (CallState.Phase == NewPhase)
	{
		return;
	}
	CallState.Phase = NewPhase;
	ForceNetUpdate();
	BP_OnCallPhaseChanged(NewPhase);
}

void AOBExtractionZone::OnRep_CallState()
{
	BP_OnCallPhaseChanged(CallState.Phase);
}

bool AOBExtractionZone::CanPlayerExtract(AController* Controller) const
{
	const AOBPlayerStateBase* PS = Controller ? Controller->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	if (!PS || PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive)
	{
		return false;
	}
	if (OwningTeamId != 0 && PS->GetTeamId() != OwningTeamId)
	{
		return false;
	}
	if (ExtractType == EOBExtractionType::Public && !bPublicAllowsAllTeams
		&& CallState.CallingTeamId != 0 && PS->GetTeamId() != CallState.CallingTeamId)
	{
		return false;
	}
	return true;
}

bool AOBExtractionZone::IsActiveNow() const
{
	const AOBExpeditionGameState* GS = GetExpeditionGameState();
	if (!GS)
	{
		return true;
	}
	const int32 Elapsed = GS->GetElapsedSeconds();
	return Elapsed >= ActiveStartSec && (ActiveEndSec <= 0 || Elapsed <= ActiveEndSec);
}

void AOBExtractionZone::CheckActiveState()
{
	const bool bNow = IsActiveNow();
	if (bNow != bLastActiveState)
	{
		bLastActiveState = bNow;
		OnExtractionActiveChanged(bNow);
	}
	const AOBExpeditionGameState* GS = GetExpeditionGameState();
	const bool bWindowHasEnded = GS && ActiveEndSec > 0 && GS->GetElapsedSeconds() > ActiveEndSec;
	if (HasAuthority() && bWindowHasEnded && CallState.Phase == EOBExtractionCallPhase::Ready)
	{
		SetCallPhase(EOBExtractionCallPhase::Expired);
	}
}

float AOBExtractionZone::GetServerTime() const
{
	if (const AOBExpeditionGameState* GS = GetExpeditionGameState())
	{
		return GS->GetServerWorldTimeSeconds();
	}
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

float AOBExtractionZone::GetArrivalSecondsRemaining() const
{
	return FMath::Max(0.f, CallState.ArrivalServerTime - GetServerTime());
}

float AOBExtractionZone::GetBoardingSecondsRemaining() const
{
	return FMath::Max(0.f, CallState.BoardingEndsServerTime - GetServerTime());
}

AOBExpeditionGameState* AOBExtractionZone::GetExpeditionGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AOBExpeditionGameState>() : nullptr;
}
