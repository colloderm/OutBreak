#include "Game/Expedition/OBExtractionZone.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Game/Expedition/OBExtractionSite.h"
#include "Game/Expedition/OBHelicopterRoute.h"
#include "Game/Expedition/OBHelicopterSystemSettings.h"
#include "Game/Expedition/OBInsertionHelicopter.h"
#include "Game/Expedition/OBSignalFlare.h"
#include "Game/GameMode/OBExpeditionGameMode.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"
#include "TimerManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarOBExtractionDebug(
		TEXT("ob.Extraction.Debug"),
		0,
		TEXT("Extraction zone debug visualization: 0=off, 1=triggers/state, 2=triggers/state/anchors/routes."),
		ECVF_Cheat);

	TAutoConsoleVariable<float> CVarOBExtractionDebugDistance(
		TEXT("ob.Extraction.DebugDistance"),
		30000.f,
		TEXT("Maximum 2D distance in cm from the local pawn for extraction debug drawing. <=0 draws all zones."),
		ECVF_Cheat);

	FString ExtractionPhaseName(EOBExtractionCallPhase Phase)
	{
		if (const UEnum* Enum = StaticEnum<EOBExtractionCallPhase>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Phase));
		}
		return TEXT("Unknown");
	}

	FString ExtractionTypeName(EOBExtractionType Type)
	{
		if (const UEnum* Enum = StaticEnum<EOBExtractionType>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Type));
		}
		return TEXT("Unknown");
	}
}

AOBExtractionZone::AOBExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	// Calls and countdowns must exist even before this area is streamed around a player.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
	// Public call state drives map/HUD countdowns even when the site is far away.
	// Personal zones still apply the team filter in IsNetRelevantFor.
	bAlwaysRelevant = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
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

bool AOBExtractionZone::IsRuntimeAssignedPersonalZone() const
{
	return ExtractType == EOBExtractionType::Personal
		&& OwningTeamId != 0;
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

	const FString ConfigurationIssue = ExtractType == EOBExtractionType::Personal && OwningTeamId == 0
		? TEXT("UNASSIGNED_PERSONAL_DIRECT_PLACEMENT")
		: TEXT("None");
	UE_LOG(LogTemp, Log,
		TEXT("[ExtractionDebug] BeginPlay Zone=%s Type=%s Team=%d RuntimeAssigned=%s Location=%s Phase=%s ActiveWindow=%d..%d CallTriggerRadius=%.1f CallCollision=%s PawnResponse=%d GenerateOverlap=%s BoardingRadius=%.1f ConfigIssue=%s"),
		*GetPathName(), *ExtractionTypeName(ExtractType), OwningTeamId,
		IsRuntimeAssignedPersonalZone() ? TEXT("true") : TEXT("false"),
		*GetActorLocation().ToCompactString(), *ExtractionPhaseName(CallState.Phase),
		ActiveStartSec, ActiveEndSec, Trigger ? Trigger->GetScaledSphereRadius() : 0.f,
		Trigger ? *UEnum::GetValueAsString(Trigger->GetCollisionEnabled()) : TEXT("Missing"),
		Trigger ? static_cast<int32>(Trigger->GetCollisionResponseToChannel(ECC_Pawn)) : -1,
		Trigger && Trigger->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),
		BoardingTrigger ? BoardingTrigger->GetScaledSphereRadius() : 0.f,
		*ConfigurationIssue);

	if (ConfigurationIssue != TEXT("None"))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ExtractionDebug] %s is a Personal extraction zone placed directly in the level, so no team was assigned. Place an OBExtractionSite/PersonalExtract marker instead; the GameMode will spawn and configure BP_ExtractionZone_Personal at runtime."),
			*GetPathName());
	}
	if (Trigger && Trigger->GetScaledSphereRadius() < 100.f)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[ExtractionDebug] %s CallTrigger radius is only %.1f cm. A standing character's capsule center is normally higher than this, so BeginOverlap may never fire even at the exact marker. Restore the BP CallTrigger sphere radius to at least the native 300 cm default."),
			*GetPathName(), Trigger->GetScaledSphereRadius());
	}

	CheckActiveState();
	GetWorldTimerManager().SetTimer(ActiveCheckTimer, this, &AOBExtractionZone::CheckActiveState, 1.f, true);
}

void AOBExtractionZone::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 네이티브 서브오브젝트 이름이 "Trigger" -> "CallTrigger"로 바뀌면서
	// BP에 남은 옛 오버라이드가 생성자의 InitSphereRadius를 덮고 있다.
	// 생성자가 아니라 여기서 쓰면 직렬화 값보다 뒤에 실행되므로 항상 이긴다.
	if (Trigger)
	{
		Trigger->SetSphereRadius(CallTriggerRadius, /*bUpdateOverlaps=*/true);
	}
	if (BoardingTrigger)
	{
		BoardingTrigger->SetSphereRadius(BoardingTriggerRadius, /*bUpdateOverlaps=*/true);
	}
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
		if (Now >= CallState.ArrivalServerTime - GetEffectiveInboundLeadTime())
		{
			SpawnExtractionHelicopter();
		}
		break;
	case EOBExtractionCallPhase::Boarding:
		ProcessBoarding(DeltaSeconds);
		UpdateEarlyDepartureCountdown();
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
	if (!HasAuthority())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(OtherActor);
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	FString BlockReason;
	if (CallState.Phase != EOBExtractionCallPhase::Ready)
	{
		BlockReason = FString::Printf(TEXT("Phase=%s"), *ExtractionPhaseName(CallState.Phase));
	}
	else if (!IsActiveNow())
	{
		BlockReason = FString::Printf(TEXT("InactiveWindow=%d..%d"), ActiveStartSec, ActiveEndSec);
	}
	else
	{
		BlockReason = GetPlayerExtractionBlockReason(Controller);
	}

	if (BlockReason.IsEmpty())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[ExtractionDebug] Call trigger accepted. Zone=%s Pawn=%s Controller=%s PlayerTeam=%d ZoneTeam=%d Type=%s"),
			*GetName(), *GetNameSafe(Pawn), *GetNameSafe(Controller),
			Controller && Controller->GetPlayerState<AOBPlayerStateBase>()
				? Controller->GetPlayerState<AOBPlayerStateBase>()->GetTeamId() : 0,
			OwningTeamId, *ExtractionTypeName(ExtractType));

		if (AOBPlayerController* CallingPlayer = Cast<AOBPlayerController>(Controller))
		{
			if (const UOBHelicopterSystemSettings* Settings = GetDefault<UOBHelicopterSystemSettings>();
				Settings && !Settings->ExtractionCallRadioSound.IsNull())
			{
				if (USoundBase* RadioSound = Settings->ExtractionCallRadioSound.LoadSynchronous())
				{
					CallingPlayer->Client_PlayExtractionRadio(
						RadioSound,
						FMath::Max(0.f, Settings->ExtractionCallRadioVolume));
				}
			}
		}
		StartExtractionCall(Controller);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ExtractionDebug] Call trigger rejected. Zone=%s OtherActor=%s Pawn=%s Controller=%s Type=%s ZoneTeam=%d Phase=%s Active=%s Reason=%s"),
			*GetName(), *GetNameSafe(OtherActor), *GetNameSafe(Pawn), *GetNameSafe(Controller),
			*ExtractionTypeName(ExtractType), OwningTeamId, *ExtractionPhaseName(CallState.Phase),
			IsActiveNow() ? TEXT("true") : TEXT("false"), *BlockReason);
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
	TSubclassOf<AOBSignalFlare> ResolvedFlareClass = SignalFlareClass;
	// Native class is the legacy default; let the project-wide BP replace it too.
	if (!ResolvedFlareClass || ResolvedFlareClass == AOBSignalFlare::StaticClass())
	{
		if (const UOBHelicopterSystemSettings* Settings = GetDefault<UOBHelicopterSystemSettings>();
			Settings && !Settings->ExtractionSignalFlareClass.IsNull())
		{
			ResolvedFlareClass = Settings->ExtractionSignalFlareClass.LoadSynchronous();
			if (!ResolvedFlareClass)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[ExtractionDebug] Failed to load the project signal flare class. Zone=%s Class=%s"),
					*GetName(), *Settings->ExtractionSignalFlareClass.ToString());
			}
		}
	}
	if (!ResolvedFlareClass)
	{
		ResolvedFlareClass = AOBSignalFlare::StaticClass();
	}

	UE_LOG(LogTemp, Log,
		TEXT("[ExtractionDebug] Call started. Zone=%s CallingTeam=%d FlareClass=%s HelicopterDelay=%.1f ArrivalServerTime=%.2f"),
		*GetName(), CallState.CallingTeamId, *GetNameSafe(ResolvedFlareClass),
		HelicopterCallDelay, CallState.ArrivalServerTime);

	if (ResolvedFlareClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActiveFlare = GetWorld()->SpawnActor<AOBSignalFlare>(ResolvedFlareClass,
			FlareLaunchAnchor->GetComponentTransform(), Params);
	}
	UE_LOG(LogTemp, Log, TEXT("[ExtractionDebug] Flare spawn result. Zone=%s Flare=%s Anchor=%s"),
		*GetName(), *GetNameSafe(ActiveFlare),
		FlareLaunchAnchor ? *FlareLaunchAnchor->GetComponentLocation().ToCompactString() : TEXT("Missing"));
	BP_OnFlareLaunched(ActiveFlare);
	SetActorTickEnabled(true);
}

float AOBExtractionZone::GetEffectiveInboundLeadTime() const
{
	return FMath::Clamp(InboundLeadTime, 0.1f, FMath::Max(0.1f, HelicopterCallDelay));
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
	StandardBoardingEndsServerTime = GetServerTime() + BoardingWindowSeconds;
	CallState.BoardingEndsServerTime = StandardBoardingEndsServerTime;
	bEarlyDepartureScheduled = false;
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

bool AOBExtractionZone::AreAllCallingTeamMembersBoarded() const
{
	if (CallState.CallingTeamId == 0 || BoardedControllers.IsEmpty())
	{
		return false;
	}

	const AOBExpeditionGameState* GS = GetExpeditionGameState();
	if (!GS)
	{
		return false;
	}

	int32 ActiveTeamMemberCount = 0;
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		const AOBPlayerStateBase* TeamMember = Cast<AOBPlayerStateBase>(PlayerState);
		if (!TeamMember || TeamMember->GetTeamId() != CallState.CallingTeamId)
		{
			continue;
		}

		const EOBPlayerExpeditionStatus Status = TeamMember->GetExpeditionStatus();
		if (Status != EOBPlayerExpeditionStatus::Alive && Status != EOBPlayerExpeditionStatus::Downed)
		{
			continue;
		}

		AController* TeamController = TeamMember->GetOwningController();
		if (!IsValid(TeamController))
		{
			// A disconnected player's inactive PlayerState must not hold the helicopter forever.
			continue;
		}

		++ActiveTeamMemberCount;
		if (!BoardedControllers.Contains(TeamController))
		{
			return false;
		}
	}

	return ActiveTeamMemberCount > 0;
}

void AOBExtractionZone::UpdateEarlyDepartureCountdown()
{
	const bool bAllTeamMembersBoarded = AreAllCallingTeamMembersBoarded();
	if (bAllTeamMembersBoarded == bEarlyDepartureScheduled)
	{
		return;
	}

	if (bAllTeamMembersBoarded)
	{
		const UOBHelicopterSystemSettings* Settings = GetDefault<UOBHelicopterSystemSettings>();
		const float ConfiguredDelay = Settings ? Settings->AllTeamBoardedDepartureDelay : 3.f;
		const float DepartureDelay = FMath::Clamp(ConfiguredDelay, 2.f, 3.f);
		CallState.BoardingEndsServerTime = FMath::Min(
			StandardBoardingEndsServerTime,
			GetServerTime() + DepartureDelay);
		bEarlyDepartureScheduled = true;
		ForceNetUpdate();
		UE_LOG(LogTemp, Log,
			TEXT("[Extraction] All calling-team members boarded. Zone=%s Team=%d Boarded=%d DepartingIn=%.1fs"),
			*GetName(), CallState.CallingTeamId, BoardedControllers.Num(),
			FMath::Max(0.f, CallState.BoardingEndsServerTime - GetServerTime()));
		return;
	}

	// A late join or a revived teammate can invalidate the shortened countdown.
	CallState.BoardingEndsServerTime = StandardBoardingEndsServerTime;
	bEarlyDepartureScheduled = false;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log,
		TEXT("[Extraction] Early departure cancelled because the calling team is no longer fully boarded. Zone=%s Team=%d"),
		*GetName(), CallState.CallingTeamId);
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
	bEarlyDepartureScheduled = false;
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
	StandardBoardingEndsServerTime = 0.f;
	bEarlyDepartureScheduled = false;
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
	StandardBoardingEndsServerTime = 0.f;
	bEarlyDepartureScheduled = false;
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
	return GetPlayerExtractionBlockReason(Controller).IsEmpty();
}

FString AOBExtractionZone::GetPlayerExtractionBlockReason(AController* Controller) const
{
	const AOBPlayerStateBase* PS = Controller ? Controller->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	if (!Controller)
	{
		return TEXT("NoController");
	}
	if (!PS)
	{
		return TEXT("NoOBPlayerState");
	}
	if (PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive)
	{
		return FString::Printf(TEXT("PlayerStatus=%s"),
			*UEnum::GetValueAsString(PS->GetExpeditionStatus()));
	}
	if (OwningTeamId != 0 && PS->GetTeamId() != OwningTeamId)
	{
		return FString::Printf(TEXT("TeamMismatch Player=%d Zone=%d"), PS->GetTeamId(), OwningTeamId);
	}
	if (ExtractType == EOBExtractionType::Public && !bPublicAllowsAllTeams
		&& CallState.CallingTeamId != 0 && PS->GetTeamId() != CallState.CallingTeamId)
	{
		return FString::Printf(TEXT("PublicCallOwnedByTeam=%d Player=%d"),
			CallState.CallingTeamId, PS->GetTeamId());
	}
	return FString();
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
	DrawDebugStatus();
}

void AOBExtractionZone::DrawDebugStatus() const
{
#if ENABLE_DRAW_DEBUG
	const int32 DebugMode = CVarOBExtractionDebug.GetValueOnGameThread();
	if (!bDrawDebugVisualization && DebugMode <= 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || !Trigger || !BoardingTrigger)
	{
		return;
	}

	APlayerController* LocalPC = World->GetFirstPlayerController();
	APawn* LocalPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
	const float MaxDistance = CVarOBExtractionDebugDistance.GetValueOnGameThread();
	if (LocalPawn && MaxDistance > 0.f
		&& FVector::DistSquared2D(LocalPawn->GetActorLocation(), GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return;
	}

	const bool bInvalidPersonalPlacement = ExtractType == EOBExtractionType::Personal && OwningTeamId == 0;
	const bool bActive = IsActiveNow();
	const FString PlayerBlock = GetPlayerExtractionBlockReason(LocalPC);
	FColor StateColor = FColor::Yellow;
	if (bInvalidPersonalPlacement)
	{
		StateColor = FColor::Red;
	}
	else if (!bActive || CallState.Phase == EOBExtractionCallPhase::Expired)
	{
		StateColor = FColor(255, 128, 0);
	}
	else if (CallState.Phase != EOBExtractionCallPhase::Ready)
	{
		StateColor = FColor::Magenta;
	}
	else if (PlayerBlock.IsEmpty())
	{
		StateColor = FColor::Green;
	}

	const FVector Center = Trigger->GetComponentLocation();
	DrawDebugSphere(World, Center, Trigger->GetScaledSphereRadius(), 24, StateColor,
		false, 1.1f, 0, 5.f);
	DrawDebugSphere(World, BoardingTrigger->GetComponentLocation(), BoardingTrigger->GetScaledSphereRadius(),
		24, FColor::Cyan, false, 1.1f, 0, 2.f);
	DrawDebugLine(World, Center, Center + FVector(0.f, 0.f, 250.f), StateColor,
		false, 1.1f, 0, 4.f);

	const FString Assignment = bInvalidPersonalPlacement
		? TEXT("INVALID: direct Personal BP / Team 0")
		: (ExtractType == EOBExtractionType::Personal ? TEXT("runtime team assignment") : TEXT("level public zone"));
	const FString Eligibility = PlayerBlock.IsEmpty() ? TEXT("ELIGIBLE") : PlayerBlock;
	const FString Text = FString::Printf(
		TEXT("%s\nType=%s Team=%d (%s)\nPhase=%s Active=%s Window=%d..%d\nLocal=%s\nCallRadius=%.0f BoardingRadius=%.0f"),
		*GetName(), *ExtractionTypeName(ExtractType), OwningTeamId, *Assignment,
		*ExtractionPhaseName(CallState.Phase), bActive ? TEXT("true") : TEXT("false"),
		ActiveStartSec, ActiveEndSec, *Eligibility,
		Trigger->GetScaledSphereRadius(), BoardingTrigger->GetScaledSphereRadius());
	DrawDebugString(World, Center + FVector(0.f, 0.f, 280.f), Text, nullptr,
		StateColor, 1.1f, true, 1.f);

	if (DebugMode >= 2 || bDrawDebugVisualization)
	{
		const FVector LandingLocation = LandingAnchor ? LandingAnchor->GetComponentLocation() : Center;
		const FVector FlareLocation = FlareLaunchAnchor ? FlareLaunchAnchor->GetComponentLocation() : Center;
		DrawDebugCoordinateSystem(World, LandingLocation,
			LandingAnchor ? LandingAnchor->GetComponentRotation() : FRotator::ZeroRotator,
			200.f, false, 1.1f, 0, 3.f);
		DrawDebugPoint(World, FlareLocation, 20.f, FColor::Orange, false, 1.1f, 0);
		DrawDebugLine(World, FlareLocation, FlareLocation + FVector(0.f, 0.f, 500.f),
			FColor::Orange, false, 1.1f, 0, 3.f);

		auto DrawRoute = [World](const AOBHelicopterRoute* Route, const FColor& Color)
		{
			if (!Route || Route->GetRouteLength() <= 1.f)
			{
				return;
			}
			FVector Previous = Route->GetRouteTransform(0.f).GetLocation();
			constexpr int32 Segments = 24;
			for (int32 Index = 1; Index <= Segments; ++Index)
			{
				const FVector Current = Route->GetRouteTransform(static_cast<float>(Index) / Segments).GetLocation();
				DrawDebugLine(World, Previous, Current, Color, false, 1.1f, 0, 3.f);
				Previous = Current;
			}
		};
		DrawRoute(ApproachRoute, FColor::Blue);
		DrawRoute(ExitRoute, FColor::Purple);
	}
#endif
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
