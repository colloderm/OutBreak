// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameMode/OBExpeditionGameMode.h"

#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Engine/TargetPoint.h"
#include "Game/Expedition/OBExpeditionMapCatalog.h"
#include "Game/Expedition/OBExpeditionMapData.h"
#include "Game/Expedition/OBExpeditionSpawnZone.h"
#include "Game/Expedition/OBExtractionZone.h"
#include "Game/Expedition/OBExtractionSite.h"
#include "Game/Expedition/OBHelicopterRoute.h"
#include "Game/Expedition/OBHelicopterInsertionAreaVolume.h"
#include "Game/Expedition/OBInsertionHelicopter.h"
#include "Game/Expedition/OBHelicopterSpawnLog.h"
#include "Game/Expedition/OBInsertionTargetStreamingProxy.h"
#include "Game/Expedition/OBLandingZoneScannerComponent.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Player/State/OBPlayerStateBase.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"
#include "Character/OBCharacterBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Player/Controller/OBPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBInsertion, Log, All);

AOBExpeditionGameMode::AOBExpeditionGameMode()
{
	GameStateClass = AOBExpeditionGameState::StaticClass();
	InsertionHelicopterClass = AOBInsertionHelicopter::StaticClass();
	LandingZoneScanner = CreateDefaultSubobject<UOBLandingZoneScannerComponent>(TEXT("LandingZoneScanner"));
}

void AOBExpeditionGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// The Blueprint Class Default is authoritative unless a server/test travel URL
	// explicitly supplies ?HelicopterInsertion=. This also makes both branches
	// independently testable without resaving BP_ExpeditionGameMode.
	const FString Override = UGameplayStatics::ParseOption(Options, TEXT("HelicopterInsertion"));
	if (!Override.IsEmpty())
	{
		bEnableHelicopterInsertion = !(Override.Equals(TEXT("0"), ESearchCase::IgnoreCase)
			|| Override.Equals(TEXT("false"), ESearchCase::IgnoreCase)
			|| Override.Equals(TEXT("off"), ESearchCase::IgnoreCase)
			|| Override.Equals(TEXT("no"), ESearchCase::IgnoreCase));
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Log,
			TEXT("[SpawnMode] URL override HelicopterInsertion=%s RawValue=%s Map=%s"),
			bEnableHelicopterInsertion ? TEXT("true") : TEXT("false"), *Override, *MapName);
	}
}

void AOBExpeditionGameMode::PreInitializeComponents()
{
	OB_HELICOPTER_SPAWN_LOG(LogTemp, Log, TEXT("[SpawnMode] HelicopterInsertion=%s GameMode=%s"),
		bEnableHelicopterInsertion ? TEXT("true") : TEXT("false"), *GetName());
	if (bEnableHelicopterInsertion && !InsertionHelicopterClass)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Warning,
			TEXT("[Insertion] No helicopter class was configured; using the native logic-only helicopter."));
		InsertionHelicopterClass = AOBInsertionHelicopter::StaticClass();
	}

	// GameState는 이 모드의 동작 전제(페이즈/세션타이머/결과창)라 BP 설정을 신뢰하지 않는다.
	// BP 서브클래스에 옛 값이 직렬화돼 있으면 C++ 생성자 기본값이 조용히 무시되고,
	// 그러면 SetPhase가 통째로 안 돌아 결과창도 타이머도 죽는다(원인 찾기 매우 어려움).
	if (!GameStateClass || !GameStateClass->IsChildOf(AOBExpeditionGameState::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Expedition] GameStateClass가 %s 였다 → OBExpeditionGameState로 강제 교체. "
				 "BP_ExpeditionGameMode의 Class Defaults > Classes > GameState Class 도 고칠 것."),
			GameStateClass ? *GameStateClass->GetName() : TEXT("null"));

		GameStateClass = AOBExpeditionGameState::StaticClass();
	}
	
	Super::PreInitializeComponents(); // 여기서 GameState가 실제로 스폰된다
	
	// 루팅 시드는 어떤 액터의 BeginPlay보다도 먼저 정해져야 한다.
	// StartPlay에서 정하면 Super::StartPlay()가 이미 전 액터의 BeginPlay를 돌린 뒤라
	// 컨테이너들이 시드 0으로 굴려 매 세션 같은 결과가 나온다.
	if (AOBExpeditionGameState* GS = Cast<AOBExpeditionGameState>(GameState))
	{
		int32 Seed = FMath::Rand();
		if (Seed == 0) Seed = 1;   // 0은 "아직 안 정해짐" 표시로 남겨둔다

		GS->SetLootSeed(Seed);
		UE_LOG(LogTemp, Log, TEXT("[Loot] 세션 루팅 시드 = %d"), Seed);
	}
}

void AOBExpeditionGameMode::StartPlay()
{
	Super::StartPlay();
	
	// 레벨 배치 공용 탈출구 수집. MapData가 필요 없으니 먼저 해도 된다.
	CollectPublicExtractsForMap();

	ActiveMapData = ResolveMapData();

	// [순서 주의] 반드시 ResolveMapData 뒤여야 한다. 앞이면 null이 실린다.
	if (AOBExpeditionGameState* GS = GetGameState<AOBExpeditionGameState>())
	{
		GS->SetMapData(ActiveMapData);
	}

	// 지도용 팀원 위치 갱신. 초당 1회면 도보 속도에 충분하다.
	GetWorldTimerManager().SetTimer(
		TeammateMapTimer, this, &AOBExpeditionGameMode::UpdateTeammateMapLocations, 1.f, true);
	
	ActiveMapData = ResolveMapData();
	
	// [데디 정원 강제] GameSession은 InitGame 단계에서 이미 생성됨.
	// MaxPlayers는 원격 클라 접속 시 PreLogin의 AtCapacity() 판정에 사용됨.
	if (GameSession)
	{
		GameSession->MaxPlayers = ResolveMaxPlayers();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Expedition] GameSession 없음 → 정원 제한 미적용"));
	}
	
	if (bEnableHelicopterInsertion && InsertionHelicopterClass)
	{
		BeginInsertionPhase();

		// StartPlay 이전에 로그인된 로컬/PIE 플레이어는 HandleStartingNewPlayer에서
		// 기존 PlayerStart 스폰을 보류했다. 삽입 페이즈와 Route 수집이 끝난 지금 태운다.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PlayerController = It->Get();
			if (PlayerController && !PendingInsertionControllers.Contains(PlayerController))
			{
				RegisterPlayerForInsertion(PlayerController);
			}
		}
	}
	else if (bEnableHelicopterInsertion)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
			TEXT("[Insertion] Insertion phase cannot start because no helicopter class exists; legacy spawn is blocked."));
	}
	else
	{
		// HandleStartingNewPlayer can run from Super::StartPlay before map setup is
		// ready. Mark the normal branch ready, start the session, then spawn every
		// controller whose PlayerStart/SpawnZone path was deferred.
		bInsertionHasStarted = false;
		bInsertionHasCompleted = true;
		StartExpedition();
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Log,
			TEXT("[SpawnMode] Normal Unreal spawn active. Helicopter creation and insertion presentation are disabled."));

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PlayerController = It->Get();
			if (PlayerController && !PlayerController->GetPawn())
			{
				Super::HandleStartingNewPlayer_Implementation(PlayerController);
				if (PlayerController->GetPawn())
				{
					OB_HELICOPTER_SPAWN_LOG(LogTemp, Log,
						TEXT("[SpawnMode] Normal spawn completed. Player=%s Pawn=%s Location=%s"),
						*PlayerController->GetName(), *PlayerController->GetPawn()->GetName(),
						*PlayerController->GetPawn()->GetActorLocation().ToCompactString());
				}
				else
				{
					OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
						TEXT("[SpawnMode] Normal spawn failed to create a Pawn. Player=%s"),
						*PlayerController->GetName());
				}
			}
		}
	}
}

void AOBExpeditionGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!bEnableHelicopterInsertion)
	{
		if (!bInsertionHasCompleted)
		{
			OB_HELICOPTER_SPAWN_LOG(LogTemp, Log,
				TEXT("[SpawnMode] Normal spawn deferred until GameMode StartPlay initialization. Player=%s"),
				*GetNameSafe(NewPlayer));
			return;
		}

		OB_HELICOPTER_SPAWN_LOG(LogTemp, Log,
			TEXT("[SpawnMode] Starting player through normal Unreal spawn. Player=%s"),
			*GetNameSafe(NewPlayer));
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		if (NewPlayer && NewPlayer->GetPawn())
		{
			OB_HELICOPTER_SPAWN_LOG(LogTemp, Log,
				TEXT("[SpawnMode] Normal late/player spawn completed. Player=%s Pawn=%s Location=%s"),
				*NewPlayer->GetName(), *NewPlayer->GetPawn()->GetName(),
				*NewPlayer->GetPawn()->GetActorLocation().ToCompactString());
		}
		else
		{
			OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
				TEXT("[SpawnMode] Normal late/player spawn failed to create a Pawn. Player=%s"),
				*GetNameSafe(NewPlayer));
		}
		return;
	}

	AOBExpeditionGameState* GS = GetExpeditionGameState();
	if (!GS)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
			TEXT("[Insertion] Player %s remains unspawned because ExpeditionGameState is unavailable; legacy spawn is blocked."),
			*GetNameSafe(NewPlayer));
		return;
	}

	if (GS->GetPhase() == EOBExpeditionPhase::Insertion)
	{
		RegisterPlayerForInsertion(NewPlayer);
		return;
	}

	// 로컬 PIE/OpenLevel에서는 기존 플레이어 시작 처리가 StartPlay보다 먼저 올 수 있다.
	// 이때 Super를 호출하면 PlayerStart/SpawnZone에 먼저 스폰되어 헬기 삽입을 우회한다.
	// StartPlay가 삽입 페이즈를 연 뒤 현재 컨트롤러들을 일괄 등록한다.
	if (!bInsertionHasStarted && !bInsertionHasCompleted)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Log, TEXT("[Insertion] %s spawn deferred until insertion phase initialization."),
			NewPlayer ? *NewPlayer->GetName() : TEXT("UnknownPlayer"));
		return;
	}

	if (GS->GetPhase() == EOBExpeditionPhase::InProgress && bAllowLateJoinAtResolvedInsertionPoint && NewPlayer)
	{
		const AOBPlayerStateBase* PS = NewPlayer->GetPlayerState<AOBPlayerStateBase>();
		FOBTeamInsertionState State;
		if (PS && GS->GetTeamInsertionState(PS->GetTeamId(), State) && State.bHasResolvedLocation)
		{
			const FTransform SpawnTransform(FRotator::ZeroRotator,
				FVector(State.ResolvedGroundLocation) + FVector(0.f, 0.f, 150.f));
			NewPlayer->SetInitialLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
			RestartPlayerAtTransform(NewPlayer, SpawnTransform);
			AssignPersonalExtractsFor(NewPlayer, State.ResolvedGroundLocation);
			if (AOBCharacterBase* Character = Cast<AOBCharacterBase>(NewPlayer->GetPawn()))
			{
				Character->HoldUntilGrounded();
			}
			return;
		}
	}

	OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
		TEXT("[Insertion] Player %s has no valid helicopter/resolved insertion path; legacy spawn is blocked."),
		*GetNameSafe(NewPlayer));
}

void AOBExpeditionGameMode::BeginInsertionPhase()
{
	if (!bEnableHelicopterInsertion)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[SpawnMode] BeginInsertionPhase ignored because helicopter insertion is disabled."));
		return;
	}

	AOBExpeditionGameState* GS = GetExpeditionGameState();
	if (!GS)
	{
		return;
	}

	const int32 Length = ResolveSessionLength();
	GS->SetSessionLength(Length);
	GS->SetTimeRemaining(Length);
	GS->SetFinalMinute(false);
	GS->SetPhase(EOBExpeditionPhase::Insertion);
	bInsertionHasStarted = true;
	bInsertionHasCompleted = false;
	bExpeditionEnded = false;
	CollectHelicopterRoutes();
	int32 EnabledInsertionAreaVolumes = 0;
	for (TActorIterator<AOBHelicopterInsertionAreaVolume> It(GetWorld()); It; ++It)
	{
		if (It->bAllowInsertion)
		{
			++EnabledInsertionAreaVolumes;
		}
	}
	if (LandingZoneScanner && LandingZoneScanner->RequiresInsertionAreaVolume()
		&& EnabledInsertionAreaVolumes == 0)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Error,
			TEXT("[InsertionArea] No enabled AOBHelicopterInsertionAreaVolume is loaded. All insertion targets will be rejected. Place an allow volume and disable Is Spatially Loaded."));
	}
	else
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
			TEXT("[InsertionArea] Enabled allow volumes=%d Required=%s"),
			EnabledInsertionAreaVolumes,
			LandingZoneScanner && LandingZoneScanner->RequiresInsertionAreaVolume() ? TEXT("true") : TEXT("false"));
	}
	GetWorldTimerManager().SetTimer(
		InsertionWatchdogTimer, this, &AOBExpeditionGameMode::TickInsertionWatchdog, 1.f, true);
	OB_HELICOPTER_SPAWN_LOG(LogTemp, Log, TEXT("[Insertion] Phase started. HelicopterClass=%s Routes=%d"),
		InsertionHelicopterClass ? *InsertionHelicopterClass->GetName() : TEXT("None"),
		AvailableInsertionRoutes.Num());
}

void AOBExpeditionGameMode::CollectHelicopterRoutes()
{
	AvailableInsertionRoutes.Reset();
	TeamInsertionRoutes.Reset();
	for (TActorIterator<AOBHelicopterRoute> It(GetWorld()); It; ++It)
	{
		if (It->GetPurpose() == EOBHelicopterRoutePurpose::InsertionOrbit)
		{
			AvailableInsertionRoutes.Add(*It);
		}
	}
	OB_HELICOPTER_SPAWN_LOG(LogTemp, Log, TEXT("[Insertion] InsertionOrbit routes found = %d"),
		AvailableInsertionRoutes.Num());
}

AOBHelicopterRoute* AOBExpeditionGameMode::GetOrAssignInsertionRoute(uint8 TeamId)
{
	if (const TObjectPtr<AOBHelicopterRoute>* Existing = TeamInsertionRoutes.Find(TeamId))
	{
		return *Existing;
	}

	int32 SelectedIndex = AvailableInsertionRoutes.IndexOfByPredicate(
		[TeamId](const AOBHelicopterRoute* Route) { return Route && Route->GetTeamSlot() == TeamId; });
	if (SelectedIndex == INDEX_NONE)
	{
		SelectedIndex = AvailableInsertionRoutes.IndexOfByPredicate(
			[](const AOBHelicopterRoute* Route) { return Route && Route->GetTeamSlot() == 0; });
	}
	if (SelectedIndex == INDEX_NONE)
	{
		return nullptr;
	}

	AOBHelicopterRoute* Route = AvailableInsertionRoutes[SelectedIndex];
	AvailableInsertionRoutes.RemoveAt(SelectedIndex);
	TeamInsertionRoutes.Add(TeamId, Route);
	return Route;
}

AOBInsertionHelicopter* AOBExpeditionGameMode::GetOrCreateInsertionHelicopter(uint8 TeamId)
{
	if (!bEnableHelicopterInsertion)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[SpawnMode] Helicopter spawn rejected because helicopter insertion is disabled. Team=%d"), TeamId);
		return nullptr;
	}

	if (TObjectPtr<AOBInsertionHelicopter>* Existing = TeamInsertionHelicopters.Find(TeamId))
	{
		return *Existing;
	}
	if (!InsertionHelicopterClass || TeamId == 0)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error, TEXT("[Insertion] Helicopter creation rejected. Class=%s TeamId=%d"),
			InsertionHelicopterClass ? *InsertionHelicopterClass->GetName() : TEXT("None"), TeamId);
		return nullptr;
	}

	AOBHelicopterRoute* Route = GetOrAssignInsertionRoute(TeamId);
	FVector OrbitCenter = FVector::ZeroVector;
	if (ActiveMapData)
	{
		OrbitCenter.X = ActiveMapData->WorldMapCenter.X;
		OrbitCenter.Y = ActiveMapData->WorldMapCenter.Y;
	}
	FTransform SpawnTransform = Route ? Route->GetRouteTransform(0.f) : FTransform(FRotator::ZeroRotator, OrbitCenter);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AOBInsertionHelicopter* Helicopter = GetWorld()->SpawnActor<AOBInsertionHelicopter>(
		InsertionHelicopterClass, SpawnTransform, SpawnParameters);
	if (!Helicopter)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
			TEXT("[Insertion] Failed to spawn configured helicopter class %s for Team %d; legacy spawn is blocked."),
			*GetNameSafe(InsertionHelicopterClass), TeamId);
		return nullptr;
	}

	Helicopter->OnInsertionPhaseChanged.AddDynamic(this, &AOBExpeditionGameMode::HandleInsertionHelicopterPhaseChanged);
	Helicopter->OnPassengerDeployed.AddDynamic(this, &AOBExpeditionGameMode::HandleInsertionPassengerDeployed);
	Helicopter->OnAllPassengersDeployed.AddDynamic(this, &AOBExpeditionGameMode::HandleAllInsertionPassengersDeployed);
	Helicopter->InitializeInsertion(TeamId, Route, OrbitCenter);
	TeamInsertionHelicopters.Add(TeamId, Helicopter);

	FOBTeamInsertionState& State = TeamInsertionRuntimeStates.FindOrAdd(TeamId);
	State.TeamId = TeamId;
	State.Helicopter = Helicopter;
	State.Phase = EOBInsertionPhase::Orbiting;
	State.StateStartedServerTime = GetExpeditionGameState()->GetServerWorldTimeSeconds();
	State.SelectionDeadlineServerTime = State.StateStartedServerTime + InsertionSelectionTimeout;
	GetExpeditionGameState()->SetTeamInsertionState(State);

	FTimerHandle& SelectionTimer = InsertionSelectionTimers.FindOrAdd(TeamId);
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &AOBExpeditionGameMode::AutoSelectInsertionPoint, TeamId);
	GetWorldTimerManager().SetTimer(SelectionTimer, Delegate, InsertionSelectionTimeout, false);
	return Helicopter;
}

void AOBExpeditionGameMode::RegisterPlayerForInsertion(APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}
	if (!bEnableHelicopterInsertion)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[SpawnMode] RegisterPlayerForInsertion redirected to normal spawn. Player=%s"),
			*NewPlayer->GetName());
		if (!NewPlayer->GetPawn() && bInsertionHasCompleted)
		{
			Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(InsertionCompletionTimer);
	PendingInsertionControllers.AddUnique(NewPlayer);
	AOBPlayerStateBase* PS = NewPlayer->GetPlayerState<AOBPlayerStateBase>();
	if (!PS)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
			TEXT("[Insertion] %s has no PlayerState and remains pending; legacy spawn is blocked."),
			*NewPlayer->GetName());
		return;
	}
	uint8 TeamId = PS->GetTeamId();
	if (PS && TeamId == 0)
	{
		const FString* PartyCode = PartyCodeByController.Find(NewPlayer);
		TeamId = ResolveTeamForCode(PartyCode ? *PartyCode : FString());
		PS->SetTeamId(TeamId);
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Warning, TEXT("[Insertion] %s had TeamId 0; assigned TeamId %d before seating."),
			*NewPlayer->GetName(), TeamId);
	}
	AOBInsertionHelicopter* Helicopter = GetOrCreateInsertionHelicopter(TeamId);
	FOBTeamInsertionState* ExistingState = TeamInsertionRuntimeStates.Find(TeamId);
	const bool bRappelAlreadyStarted = Helicopter
		&& (Helicopter->GetInsertionPhase() == EOBInsertionPhase::Rappelling
			|| Helicopter->GetInsertionPhase() == EOBInsertionPhase::Departing
			|| Helicopter->GetInsertionPhase() == EOBInsertionPhase::Completed);
	if (bRappelAlreadyStarted && ExistingState && ExistingState->bHasResolvedLocation
		&& bAllowLateJoinAtResolvedInsertionPoint)
	{
		const FTransform SpawnTransform(FRotator::ZeroRotator,
			FVector(ExistingState->ResolvedGroundLocation) + FVector(0.f, 0.f, 150.f));
		NewPlayer->SetInitialLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
		RestartPlayerAtTransform(NewPlayer, SpawnTransform);
		if (!NewPlayer->GetPawn())
		{
			OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
				TEXT("[Insertion] Late insertion Pawn spawn failed for %s; player remains pending."),
				*NewPlayer->GetName());
			return;
		}
		AssignPersonalExtractsFor(NewPlayer, ExistingState->ResolvedGroundLocation);
		if (AOBCharacterBase* Character = Cast<AOBCharacterBase>(NewPlayer->GetPawn()))
		{
			Character->HoldUntilGrounded();
		}
		PendingInsertionControllers.Remove(NewPlayer);
		TryCompleteInsertion();
		return;
	}
	if (!Helicopter || Helicopter->GetPassengerCount() >= Helicopter->GetSeatCapacity())
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error, TEXT("[Insertion] Unable to seat player %s for Team %d; player remains pending and legacy spawn is blocked."),
			*NewPlayer->GetName(), TeamId);
		return;
	}

	if (!SpawnAndSeatInsertionPawn(NewPlayer, Helicopter))
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error, TEXT("[Insertion] Pawn creation or seat attachment failed for %s."), *NewPlayer->GetName());
		return;
	}

	FOBTeamInsertionState& State = TeamInsertionRuntimeStates.FindOrAdd(TeamId);
	State.TeamId = TeamId;
	State.Helicopter = Helicopter;
	State.PassengerCount = Helicopter->GetPassengerCount();
	GetExpeditionGameState()->SetTeamInsertionState(State);
	if (AOBPlayerController* OBPlayerController = Cast<AOBPlayerController>(NewPlayer))
	{
		OBPlayerController->Client_BeginInsertionPresentation(
			Helicopter, State.SelectionDeadlineServerTime, PS->IsPartyLeader());
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
			TEXT("[InsertionInput] Presentation requested PC=%s Team=%d Leader=%s Helicopter=%s Deadline=%.2f"),
			*NewPlayer->GetName(), TeamId, PS->IsPartyLeader() ? TEXT("true") : TEXT("false"),
			*Helicopter->GetName(), State.SelectionDeadlineServerTime);
	}
}

bool AOBExpeditionGameMode::SpawnAndSeatInsertionPawn(
	APlayerController* NewPlayer,
	AOBInsertionHelicopter* Helicopter)
{
	if (!NewPlayer || !Helicopter || Helicopter->GetPassengerCount() >= Helicopter->GetSeatCapacity())
	{
		return false;
	}

	const FTransform SeatTransform = Helicopter->GetSeatTransform(Helicopter->GetPassengerCount());
	NewPlayer->SetInitialLocationAndRotation(SeatTransform.GetLocation(), SeatTransform.Rotator());

	APawn* Pawn = NewPlayer->GetPawn();
	const bool bCreatedPawn = !IsValid(Pawn);
	if (bCreatedPawn)
	{
		UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
		if (!PawnClass)
		{
			OB_HELICOPTER_SPAWN_LOG(LogTemp, Error, TEXT("[Insertion] No default Pawn class exists for %s."), *NewPlayer->GetName());
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Instigator = GetInstigator();
		SpawnParameters.ObjectFlags |= RF_Transient;
		// Cabin seats intentionally overlap the helicopter. The normal restart path
		// can reject this as Bad Size, so insertion owns an explicit AlwaysSpawn path.
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pawn = GetWorld()->SpawnActor<APawn>(PawnClass, SeatTransform, SpawnParameters);
		if (!Pawn)
		{
			OB_HELICOPTER_SPAWN_LOG(LogTemp, Error, TEXT("[Insertion] AlwaysSpawn failed for Pawn class %s."), *GetNameSafe(PawnClass));
			return false;
		}

		Pawn->SetActorEnableCollision(false);
		NewPlayer->SetPawn(Pawn);
		FinishRestartPlayer(NewPlayer, SeatTransform.Rotator());
		Pawn = NewPlayer->GetPawn();
	}

	if (!IsValid(Pawn) || !Helicopter->SeatPassenger(NewPlayer))
	{
		if (bCreatedPawn && IsValid(Pawn))
		{
			NewPlayer->UnPossess();
			Pawn->Destroy();
		}
		return false;
	}

	OB_HELICOPTER_SPAWN_LOG(LogTemp, Log, TEXT("[Insertion] %s seated in %s at %s."),
		*NewPlayer->GetName(), *Helicopter->GetName(), *SeatTransform.GetLocation().ToCompactString());
	return true;
}

void AOBExpeditionGameMode::RequestInsertionPoint(
	AOBPlayerController* RequestingPlayer,
	const FVector2D& WorldXY)
{
	if (!bEnableHelicopterInsertion)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[SpawnMode] Insertion target request rejected because helicopter insertion is disabled. PC=%s"),
			*GetNameSafe(RequestingPlayer));
		if (RequestingPlayer)
		{
			RequestingPlayer->Client_InsertionPointResult(
				false, FVector::ZeroVector, TEXT("Helicopter insertion is disabled for this game mode."));
		}
		return;
	}

	AOBExpeditionGameState* GS = GetExpeditionGameState();
	AOBPlayerStateBase* PS = RequestingPlayer ? RequestingPlayer->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
		TEXT("[InsertionTarget] Request PC=%s Team=%d Leader=%s WorldXY=%s ExpeditionPhase=%d"),
		*GetNameSafe(RequestingPlayer), PS ? PS->GetTeamId() : 0,
		PS && PS->IsPartyLeader() ? TEXT("true") : TEXT("false"), *WorldXY.ToString(),
		GS ? static_cast<int32>(GS->GetPhase()) : -1);
	if (!GS || !PS || GS->GetPhase() != EOBExpeditionPhase::Insertion)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[InsertionTarget] Request rejected Reason=SelectionClosed PC=%s GS=%s PS=%s"),
			*GetNameSafe(RequestingPlayer), *GetNameSafe(GS), *GetNameSafe(PS));
		if (RequestingPlayer)
		{
			RequestingPlayer->Client_InsertionPointResult(false, FVector::ZeroVector, TEXT("Insertion selection is closed."));
		}
		return;
	}
	if (!PS->IsPartyLeader())
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[InsertionTarget] Request rejected Reason=NotLeader PC=%s Team=%d"),
			*RequestingPlayer->GetName(), PS->GetTeamId());
		RequestingPlayer->Client_InsertionPointResult(false, FVector::ZeroVector, TEXT("Only the party leader can select the insertion point."));
		return;
	}
	if (!FMath::IsFinite(WorldXY.X) || !FMath::IsFinite(WorldXY.Y)
		|| !ActiveMapData || !ActiveMapData->ContainsWorldXY(WorldXY))
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[InsertionTarget] Request rejected Reason=OutsideMap PC=%s XY=%s MapData=%s"),
			*RequestingPlayer->GetName(), *WorldXY.ToString(), *GetNameSafe(ActiveMapData));
		RequestingPlayer->Client_InsertionPointResult(false, FVector::ZeroVector, TEXT("The selected point is outside the playable map."));
		return;
	}

	const FOBTeamInsertionState* State = TeamInsertionRuntimeStates.Find(PS->GetTeamId());
	if (!State || (State->Phase != EOBInsertionPhase::Orbiting && State->Phase != EOBInsertionPhase::WaitingForTarget))
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[InsertionTarget] Request rejected Reason=AlreadyCommitted PC=%s Team=%d State=%s Phase=%d"),
			*RequestingPlayer->GetName(), PS->GetTeamId(), State ? TEXT("valid") : TEXT("missing"),
			State ? static_cast<int32>(State->Phase) : -1);
		RequestingPlayer->Client_InsertionPointResult(false, FVector::ZeroVector, TEXT("This helicopter has already committed to an insertion point."));
		return;
	}

	TArray<FVector> Candidates;
	Candidates.Add(FVector(WorldXY.X, WorldXY.Y, 0.f));
	BeginInsertionTargetResolution(PS->GetTeamId(), Candidates, RequestingPlayer, false);
}

bool AOBExpeditionGameMode::ResolveAndBeginInsertion(
	uint8 TeamId,
	const FVector& RequestedLocation,
	AOBPlayerController* FeedbackPlayer)
{
	TObjectPtr<AOBInsertionHelicopter>* FoundHelicopter = TeamInsertionHelicopters.Find(TeamId);
	AOBInsertionHelicopter* Helicopter = FoundHelicopter ? FoundHelicopter->Get() : nullptr;
	FOBTeamInsertionState* State = TeamInsertionRuntimeStates.Find(TeamId);
	FOBLandingZoneResult LandingZone;
	if (!Helicopter || !State || !LandingZoneScanner)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Error,
			TEXT("[InsertionTarget] Validation could not start Team=%d Helicopter=%s State=%s Scanner=%s"),
			TeamId, *GetNameSafe(Helicopter), State ? TEXT("valid") : TEXT("missing"),
			*GetNameSafe(LandingZoneScanner));
		if (FeedbackPlayer)
		{
			FeedbackPlayer->Client_InsertionPointResult(false, FVector::ZeroVector,
				TEXT("Insertion validation is temporarily unavailable."));
		}
		return false;
	}
	if (!LandingZoneScanner->FindSafeLandingZone(RequestedLocation, LandingZone))
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[InsertionTarget] Validation failed Team=%d Requested=%s Reason=%s Candidates=%d"),
			TeamId, *RequestedLocation.ToCompactString(), LexToString(LandingZone.Failure),
			LandingZone.CandidatesEvaluated);
		if (FeedbackPlayer)
		{
			FeedbackPlayer->Client_InsertionPointResult(false, FVector::ZeroVector,
				TEXT("No safe landing zone was found near that point."));
		}
		return false;
	}

	State->bHasRequestedLocation = true;
	State->RequestedLocation = RequestedLocation;
	State->bHasResolvedLocation = true;
	State->ResolvedGroundLocation = LandingZone.GroundLocation;
	State->Phase = EOBInsertionPhase::Approaching;
	State->StateStartedServerTime = GetExpeditionGameState()->GetServerWorldTimeSeconds();
	GetExpeditionGameState()->SetTeamInsertionState(*State);

	if (FTimerHandle* Timer = InsertionSelectionTimers.Find(TeamId))
	{
		GetWorldTimerManager().ClearTimer(*Timer);
	}
	Helicopter->BeginInsertionApproach(LandingZone);
	OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
		TEXT("[InsertionTarget] Validation accepted Team=%d Requested=%s Ground=%s Hover=%s Candidates=%d Score=%.1f"),
		TeamId, *RequestedLocation.ToCompactString(),
		*FVector(LandingZone.GroundLocation).ToCompactString(),
		*LandingZone.HoverTransform.GetLocation().ToCompactString(),
		LandingZone.CandidatesEvaluated, LandingZone.Score);
	if (FeedbackPlayer)
	{
		FeedbackPlayer->Client_InsertionPointResult(true, LandingZone.GroundLocation, TEXT("Insertion point confirmed."));
	}
	return true;
}

void AOBExpeditionGameMode::AutoSelectInsertionPoint(uint8 TeamId)
{
	FOBTeamInsertionState* State = TeamInsertionRuntimeStates.Find(TeamId);
	if (!State || (State->Phase != EOBInsertionPhase::Orbiting && State->Phase != EOBInsertionPhase::WaitingForTarget))
	{
		return;
	}

	TArray<FVector> Candidates;
	if (ActiveMapData)
	{
		for (const FVector2D& Candidate : ActiveMapData->DefaultInsertionCandidates)
		{
			if (ActiveMapData->ContainsWorldXY(Candidate))
			{
				Candidates.AddUnique(FVector(Candidate.X, Candidate.Y, 0.f));
			}
		}

		const FVector2D Center = ActiveMapData->WorldMapCenter;
		Candidates.AddUnique(FVector(Center.X, Center.Y, 0.f));
		const float RingRadius = FMath::Min(ActiveMapData->WorldMapSize.X, ActiveMapData->WorldMapSize.Y) * 0.25f;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = 2.f * UE_PI * static_cast<float>(Index) / 8.f;
			const FVector2D Candidate(
				Center.X + FMath::Cos(Angle) * RingRadius,
				Center.Y + FMath::Sin(Angle) * RingRadius);
			if (ActiveMapData->ContainsWorldXY(Candidate))
			{
				Candidates.AddUnique(FVector(Candidate.X, Candidate.Y, 0.f));
			}
		}
	}
	else if (State->Helicopter)
	{
		FVector Candidate = State->Helicopter->GetActorLocation();
		Candidate.Z = 0.f;
		Candidates.Add(Candidate);
	}

	if (Candidates.IsEmpty())
	{
		FinishInsertionTargetFailure(TeamId, TEXT("No automatic insertion candidates are configured."));
		return;
	}

	OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
		TEXT("[InsertionTarget] Automatic candidate queue Team=%d Candidates=%d"), TeamId, Candidates.Num());
	BeginInsertionTargetResolution(TeamId, Candidates, nullptr, true);
}

void AOBExpeditionGameMode::BeginInsertionTargetResolution(
	uint8 TeamId,
	const TArray<FVector>& Candidates,
	AOBPlayerController* FeedbackPlayer,
	bool bAutomatic)
{
	if (Candidates.IsEmpty())
	{
		FinishInsertionTargetFailure(TeamId, TEXT("No insertion target candidates were supplied."));
		return;
	}

	if (FTimerHandle* SelectionTimer = InsertionSelectionTimers.Find(TeamId))
	{
		GetWorldTimerManager().ClearTimer(*SelectionTimer);
	}
	if (FTimerHandle* StreamingTimer = InsertionStreamingPollTimers.Find(TeamId))
	{
		GetWorldTimerManager().ClearTimer(*StreamingTimer);
	}

	FOBPendingInsertionTargetRequest& Request = PendingInsertionTargetRequests.FindOrAdd(TeamId);
	Request.Candidates = Candidates;
	Request.CandidateIndex = INDEX_NONE;
	Request.FeedbackPlayer = FeedbackPlayer;
	Request.bAutomatic = bAutomatic;
	Request.RequestStartedServerTime = GetExpeditionGameState()->GetServerWorldTimeSeconds();
	Request.CandidateStartedServerTime = 0.f;

	OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
		TEXT("[InsertionTarget] Resolution begin Team=%d Automatic=%s Candidates=%d FeedbackPC=%s"),
		TeamId, bAutomatic ? TEXT("true") : TEXT("false"), Candidates.Num(), *GetNameSafe(FeedbackPlayer));
	StartNextInsertionTargetCandidate(TeamId);
}

void AOBExpeditionGameMode::StartNextInsertionTargetCandidate(uint8 TeamId)
{
	FOBPendingInsertionTargetRequest* Request = PendingInsertionTargetRequests.Find(TeamId);
	FOBTeamInsertionState* State = TeamInsertionRuntimeStates.Find(TeamId);
	if (!Request || !State)
	{
		FinishInsertionTargetFailure(TeamId, TEXT("Insertion target state was lost."));
		return;
	}

	if (FTimerHandle* StreamingTimer = InsertionStreamingPollTimers.Find(TeamId))
	{
		GetWorldTimerManager().ClearTimer(*StreamingTimer);
	}

	++Request->CandidateIndex;
	if (!Request->Candidates.IsValidIndex(Request->CandidateIndex))
	{
		FinishInsertionTargetFailure(
			TeamId,
			Request->bAutomatic
				? TEXT("Automatic landing-zone search failed. Select another point on the map.")
				: TEXT("No safe landing zone was found near that point. Select another point."));
		return;
	}

	const FVector Candidate = Request->Candidates[Request->CandidateIndex];
	AOBInsertionTargetStreamingProxy* Proxy = GetOrCreateInsertionTargetStreamingProxy(TeamId);
	if (!Proxy)
	{
		FinishInsertionTargetFailure(TeamId, TEXT("The insertion streaming source could not be created."));
		return;
	}

	const float Radius = ActiveMapData ? ActiveMapData->InsertionStreamingRadius : 10000.f;
	Proxy->Configure(Candidate, Radius);
	Request->CandidateStartedServerTime = GetExpeditionGameState()->GetServerWorldTimeSeconds();

	State->bHasRequestedLocation = true;
	State->RequestedLocation = Candidate;
	State->bHasResolvedLocation = false;
	State->ResolvedGroundLocation = FVector::ZeroVector;
	State->Phase = EOBInsertionPhase::LoadingTarget;
	State->StateStartedServerTime = Request->CandidateStartedServerTime;
	GetExpeditionGameState()->SetTeamInsertionState(*State);

	OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
		TEXT("[InsertionTarget] Streaming begin Team=%d Candidate=%d/%d XY=%s Radius=%.0f Proxy=%s Automatic=%s"),
		TeamId, Request->CandidateIndex + 1, Request->Candidates.Num(), *Candidate.ToCompactString(), Radius,
		*Proxy->GetName(), Request->bAutomatic ? TEXT("true") : TEXT("false"));
	NotifyTeamInsertionPresentation(TeamId, EOBInsertionPhase::LoadingTarget,
		TEXT("Loading the selected insertion area..."), true);

	FTimerHandle& StreamingTimer = InsertionStreamingPollTimers.FindOrAdd(TeamId);
	FTimerDelegate PollDelegate = FTimerDelegate::CreateUObject(
		this, &AOBExpeditionGameMode::PollInsertionTargetStreaming, TeamId);
	GetWorldTimerManager().SetTimer(
		StreamingTimer, PollDelegate, FMath::Max(0.05f, InsertionStreamingPollInterval), true);
	PollInsertionTargetStreaming(TeamId);
}

void AOBExpeditionGameMode::PollInsertionTargetStreaming(uint8 TeamId)
{
	FOBPendingInsertionTargetRequest* Request = PendingInsertionTargetRequests.Find(TeamId);
	TObjectPtr<AOBInsertionTargetStreamingProxy>* FoundProxy = TeamInsertionTargetStreamingProxies.Find(TeamId);
	AOBInsertionTargetStreamingProxy* Proxy = FoundProxy ? FoundProxy->Get() : nullptr;
	if (!Request || !Proxy)
	{
		FinishInsertionTargetFailure(TeamId, TEXT("The insertion streaming request was interrupted."));
		return;
	}

	const float Now = GetExpeditionGameState()->GetServerWorldTimeSeconds();
	const float Elapsed = Now - Request->CandidateStartedServerTime;
	const float Timeout = ActiveMapData ? ActiveMapData->InsertionStreamingTimeout : 15.f;
	if (Proxy->IsTargetStreamingCompleted())
	{
		if (FTimerHandle* StreamingTimer = InsertionStreamingPollTimers.Find(TeamId))
		{
			GetWorldTimerManager().ClearTimer(*StreamingTimer);
		}
		if (FOBTeamInsertionState* State = TeamInsertionRuntimeStates.Find(TeamId))
		{
			State->Phase = EOBInsertionPhase::ValidatingTarget;
			State->StateStartedServerTime = Now;
			GetExpeditionGameState()->SetTeamInsertionState(*State);
		}
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
			TEXT("[InsertionTarget] Streaming complete Team=%d Candidate=%d Elapsed=%.2f Proxy=%s"),
			TeamId, Request->CandidateIndex + 1, Elapsed, *Proxy->GetName());
		NotifyTeamInsertionPresentation(TeamId, EOBInsertionPhase::ValidatingTarget,
			TEXT("Scanning the selected area for a safe rappel point..."), true);

		FTimerDelegate ValidateDelegate = FTimerDelegate::CreateUObject(
			this, &AOBExpeditionGameMode::ValidateCurrentInsertionTarget, TeamId);
		GetWorldTimerManager().SetTimerForNextTick(ValidateDelegate);
		return;
	}

	if (Elapsed >= Timeout)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[InsertionTarget] Streaming timeout Team=%d Candidate=%d Elapsed=%.2f Timeout=%.2f Automatic=%s"),
			TeamId, Request->CandidateIndex + 1, Elapsed, Timeout,
			Request->bAutomatic ? TEXT("true") : TEXT("false"));
		if (Request->bAutomatic)
		{
			StartNextInsertionTargetCandidate(TeamId);
		}
		else
		{
			FinishInsertionTargetFailure(TeamId,
				TEXT("The selected area did not finish loading. Select another point."));
		}
	}
}

void AOBExpeditionGameMode::ValidateCurrentInsertionTarget(uint8 TeamId)
{
	FOBPendingInsertionTargetRequest* Request = PendingInsertionTargetRequests.Find(TeamId);
	if (!Request || !Request->Candidates.IsValidIndex(Request->CandidateIndex))
	{
		FinishInsertionTargetFailure(TeamId, TEXT("The insertion candidate was lost before validation."));
		return;
	}

	const FVector Candidate = Request->Candidates[Request->CandidateIndex];
	AOBPlayerController* FeedbackPlayer = Request->FeedbackPlayer.Get();
	const bool bAutomatic = Request->bAutomatic;
	FVector RequestedGround;
	if (LandingZoneScanner && LandingZoneScanner->RequiresInsertionAreaVolume()
		&& LandingZoneScanner->FindGroundAtXY(FVector2D(Candidate.X, Candidate.Y), RequestedGround)
		&& !LandingZoneScanner->IsInsideInsertionArea(RequestedGround))
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
			TEXT("[InsertionArea] Requested point rejected Team=%d Candidate=%d/%d Requested=%s Ground=%s Reason=OutsideInsertionArea Automatic=%s"),
			TeamId, Request->CandidateIndex + 1, Request->Candidates.Num(),
			*Candidate.ToCompactString(), *RequestedGround.ToCompactString(),
			bAutomatic ? TEXT("true") : TEXT("false"));
		if (bAutomatic)
		{
			StartNextInsertionTargetCandidate(TeamId);
		}
		else
		{
			FinishInsertionTargetFailure(TeamId,
				TEXT("The selected point is outside the permitted helicopter insertion area."));
		}
		return;
	}
	if (ResolveAndBeginInsertion(TeamId, Candidate, nullptr))
	{
		if (FeedbackPlayer)
		{
			const FOBTeamInsertionState* State = TeamInsertionRuntimeStates.Find(TeamId);
			FeedbackPlayer->Client_InsertionPointResult(
				true,
				State ? FVector(State->ResolvedGroundLocation) : Candidate,
				TEXT("Insertion point confirmed."));
		}
		PendingInsertionTargetRequests.Remove(TeamId);
		NotifyTeamInsertionPresentation(TeamId, EOBInsertionPhase::Approaching,
			TEXT("Insertion point confirmed. Helicopter approaching."), false);
		return;
	}

	OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
		TEXT("[InsertionTarget] Candidate rejected Team=%d Candidate=%d/%d XY=%s Automatic=%s"),
		TeamId, Request->CandidateIndex + 1, Request->Candidates.Num(), *Candidate.ToCompactString(),
		bAutomatic ? TEXT("true") : TEXT("false"));
	if (bAutomatic)
	{
		StartNextInsertionTargetCandidate(TeamId);
	}
	else
	{
		FinishInsertionTargetFailure(TeamId,
			TEXT("No safe landing zone was found near that point. Select another point."));
	}
}

void AOBExpeditionGameMode::FinishInsertionTargetFailure(uint8 TeamId, const FString& Message)
{
	AOBPlayerController* FeedbackPlayer = nullptr;
	if (FOBPendingInsertionTargetRequest* Request = PendingInsertionTargetRequests.Find(TeamId))
	{
		FeedbackPlayer = Request->FeedbackPlayer.Get();
	}
	if (FTimerHandle* StreamingTimer = InsertionStreamingPollTimers.Find(TeamId))
	{
		GetWorldTimerManager().ClearTimer(*StreamingTimer);
	}
	PendingInsertionTargetRequests.Remove(TeamId);
	ReleaseInsertionTargetStreamingProxy(TeamId);

	if (FOBTeamInsertionState* State = TeamInsertionRuntimeStates.Find(TeamId))
	{
		State->Phase = EOBInsertionPhase::WaitingForTarget;
		State->bHasResolvedLocation = false;
		State->ResolvedGroundLocation = FVector::ZeroVector;
		State->StateStartedServerTime = GetExpeditionGameState()->GetServerWorldTimeSeconds();
		GetExpeditionGameState()->SetTeamInsertionState(*State);
	}
	if (FeedbackPlayer)
	{
		FeedbackPlayer->Client_InsertionPointResult(false, FVector::ZeroVector, Message);
	}
	NotifyTeamInsertionPresentation(TeamId, EOBInsertionPhase::WaitingForTarget, Message, true);
	OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Error,
		TEXT("[InsertionTarget] Resolution failed Team=%d Message=%s; passengers remain seated and map reopens."),
		TeamId, *Message);
}

AOBInsertionTargetStreamingProxy* AOBExpeditionGameMode::GetOrCreateInsertionTargetStreamingProxy(uint8 TeamId)
{
	if (TObjectPtr<AOBInsertionTargetStreamingProxy>* Existing = TeamInsertionTargetStreamingProxies.Find(TeamId))
	{
		if (IsValid(Existing->Get()))
		{
			return Existing->Get();
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AOBInsertionTargetStreamingProxy* Proxy = GetWorld()->SpawnActor<AOBInsertionTargetStreamingProxy>(
		AOBInsertionTargetStreamingProxy::StaticClass(), FTransform::Identity, SpawnParameters);
	if (Proxy)
	{
		TeamInsertionTargetStreamingProxies.Add(TeamId, Proxy);
	}
	else
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Error,
			TEXT("[InsertionTarget] Failed to spawn streaming proxy Team=%d"), TeamId);
	}
	return Proxy;
}

void AOBExpeditionGameMode::NotifyTeamInsertionPresentation(
	uint8 TeamId,
	EOBInsertionPhase Phase,
	const FString& Message,
	bool bForceMapOpen)
{
	if (!GameState)
	{
		return;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AOBPlayerStateBase* OBPlayerState = Cast<AOBPlayerStateBase>(PlayerState);
		if (OBPlayerState && OBPlayerState->GetTeamId() == TeamId)
		{
			if (AOBPlayerController* PC = Cast<AOBPlayerController>(OBPlayerState->GetOwningController()))
			{
				PC->Client_UpdateInsertionPresentation(Phase, Message, bForceMapOpen);
			}
		}
	}
}

void AOBExpeditionGameMode::ReleaseInsertionTargetStreamingProxy(uint8 TeamId)
{
	if (TObjectPtr<AOBInsertionTargetStreamingProxy>* Found = TeamInsertionTargetStreamingProxies.Find(TeamId))
	{
		if (AOBInsertionTargetStreamingProxy* Proxy = Found->Get())
		{
			Proxy->DeactivateStreaming();
			Proxy->Destroy();
		}
		TeamInsertionTargetStreamingProxies.Remove(TeamId);
	}
}

void AOBExpeditionGameMode::TickInsertionWatchdog()
{
	if (bInsertionHasCompleted || !bInsertionHasStarted)
	{
		return;
	}
	const float Now = GetExpeditionGameState()->GetServerWorldTimeSeconds();
	for (TPair<uint8, FOBTeamInsertionState>& Pair : TeamInsertionRuntimeStates)
	{
		const uint8 TeamId = Pair.Key;
		FOBTeamInsertionState& State = Pair.Value;
		AOBInsertionHelicopter* Helicopter = State.Helicopter;
		if (!IsValid(Helicopter))
		{
			OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Error,
				TEXT("[InsertionWatchdog] Missing helicopter Team=%d Phase=%d PendingPlayers=%d"),
				TeamId, static_cast<int32>(State.Phase), PendingInsertionControllers.Num());
			continue;
		}

		if (State.Phase == EOBInsertionPhase::WaitingForTarget)
		{
			float& LastWarning = LastInsertionWatchdogWarningTimes.FindOrAdd(TeamId);
			if (Now - LastWarning >= 10.f)
			{
				LastWarning = Now;
				OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Warning,
					TEXT("[InsertionWatchdog] Team=%d waiting for target for %.1fs; forcing insertion map open."),
					TeamId, Now - State.StateStartedServerTime);
				NotifyTeamInsertionPresentation(TeamId, State.Phase,
					TEXT("Select another insertion point on the map."), true);
			}
		}
		else if (State.Phase == EOBInsertionPhase::Rappelling
			&& Now - State.StateStartedServerTime >= MaxInsertionRappelSeconds)
		{
			if (!State.bHasResolvedLocation)
			{
				OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Error,
					TEXT("[InsertionWatchdog] Rappel timeout Team=%d but no validated ground exists; no forced drop performed."),
					TeamId);
				continue;
			}
			OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Error,
				TEXT("[InsertionWatchdog] Rappel timeout Team=%d Elapsed=%.1f; releasing at validated ground=%s"),
				TeamId, Now - State.StateStartedServerTime,
				*FVector(State.ResolvedGroundLocation).ToCompactString());
			Helicopter->ReleaseAllPassengers(State.ResolvedGroundLocation);
			for (int32 Index = PendingInsertionControllers.Num() - 1; Index >= 0; --Index)
			{
				AController* Controller = PendingInsertionControllers[Index];
				const AOBPlayerStateBase* PS = Controller ? Controller->GetPlayerState<AOBPlayerStateBase>() : nullptr;
				if (PS && PS->GetTeamId() == TeamId)
				{
					PendingInsertionControllers.RemoveAtSwap(Index);
				}
			}
			AssignPersonalExtractsForTeam(TeamId, State.ResolvedGroundLocation);
			ReleaseInsertionTargetStreamingProxy(TeamId);
			TryCompleteInsertion();
		}
	}
}

void AOBExpeditionGameMode::HandleInsertionHelicopterPhaseChanged(
	AOBInsertionHelicopter* Helicopter,
	EOBInsertionPhase NewPhase)
{
	if (Helicopter)
	{
		const uint8 TeamId = Helicopter->GetTeamId();
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertion, Log,
			TEXT("[Insertion] Helicopter phase Team=%d Helicopter=%s Phase=%d Passengers=%d"),
			TeamId, *Helicopter->GetName(), static_cast<int32>(NewPhase), Helicopter->GetPassengerCount());
		UpdateReplicatedInsertionState(TeamId, NewPhase);
		NotifyTeamInsertionPresentation(TeamId, NewPhase, FString(), false);
		if (NewPhase == EOBInsertionPhase::Departing || NewPhase == EOBInsertionPhase::Completed
			|| NewPhase == EOBInsertionPhase::Aborted)
		{
			ReleaseInsertionTargetStreamingProxy(TeamId);
		}
	}
}

void AOBExpeditionGameMode::HandleInsertionPassengerDeployed(
	AOBInsertionHelicopter* Helicopter,
	AController* Passenger)
{
	PendingInsertionControllers.Remove(Passenger);
	if (Helicopter)
	{
		FOBTeamInsertionState& State = TeamInsertionRuntimeStates.FindOrAdd(Helicopter->GetTeamId());
		++State.DeployedCount;
		State.PassengerCount = State.DeployedCount + Helicopter->GetPassengerCount();
		GetExpeditionGameState()->SetTeamInsertionState(State);
	}
}

void AOBExpeditionGameMode::HandleAllInsertionPassengersDeployed(AOBInsertionHelicopter* Helicopter)
{
	if (!Helicopter)
	{
		return;
	}
	const uint8 TeamId = Helicopter->GetTeamId();
	AssignPersonalExtractsForTeam(TeamId, Helicopter->GetResolvedGroundLocation());
	ReleaseInsertionTargetStreamingProxy(TeamId);
	TryCompleteInsertion();
}

void AOBExpeditionGameMode::AssignPersonalExtractsForTeam(uint8 TeamId, const FVector& InsertionOrigin)
{
	if (!GameState)
	{
		return;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AOBPlayerStateBase* OBPlayerState = Cast<AOBPlayerStateBase>(PlayerState);
		if (OBPlayerState && OBPlayerState->GetTeamId() == TeamId)
		{
			AssignPersonalExtractsFor(OBPlayerState->GetOwningController(), InsertionOrigin);
		}
	}
}

void AOBExpeditionGameMode::UpdateReplicatedInsertionState(uint8 TeamId, EOBInsertionPhase Phase)
{
	FOBTeamInsertionState& State = TeamInsertionRuntimeStates.FindOrAdd(TeamId);
	State.TeamId = TeamId;
	State.Phase = Phase;
	State.StateStartedServerTime = GetExpeditionGameState()->GetServerWorldTimeSeconds();
	if (TObjectPtr<AOBInsertionHelicopter>* Helicopter = TeamInsertionHelicopters.Find(TeamId))
	{
		State.Helicopter = Helicopter->Get();
		State.PassengerCount = State.DeployedCount + Helicopter->Get()->GetPassengerCount();
	}
	GetExpeditionGameState()->SetTeamInsertionState(State);
}

void AOBExpeditionGameMode::TryCompleteInsertion()
{
	if (!bInsertionHasStarted || bInsertionHasCompleted || !PendingInsertionControllers.IsEmpty())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(InsertionCompletionTimer);
	GetWorldTimerManager().SetTimer(InsertionCompletionTimer, this,
		&AOBExpeditionGameMode::CompleteInsertionAfterGracePeriod,
		FMath::Max(0.01f, InsertionCompletionGraceSeconds), false);
}

void AOBExpeditionGameMode::CompleteInsertionAfterGracePeriod()
{
	if (bInsertionHasCompleted || !PendingInsertionControllers.IsEmpty())
	{
		return;
	}
	bInsertionHasCompleted = true;
	GetWorldTimerManager().ClearTimer(InsertionWatchdogTimer);
	for (TPair<uint8, TObjectPtr<AOBInsertionTargetStreamingProxy>>& Pair : TeamInsertionTargetStreamingProxies)
	{
		if (Pair.Value)
		{
			Pair.Value->DeactivateStreaming();
			Pair.Value->Destroy();
		}
	}
	TeamInsertionTargetStreamingProxies.Reset();
	StartExpedition();
}

TSubclassOf<AOBInsertionHelicopter> AOBExpeditionGameMode::GetDefaultExtractionHelicopterClass() const
{
	return ExtractionHelicopterClass ? ExtractionHelicopterClass : InsertionHelicopterClass;
}

void AOBExpeditionGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	
	if (!C) return;
	
	// 진입 플레이어의 세션 초기화
	if (AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>())
	{		
		// 세션 시작 시점엔 모두 Alive. (로비에서 넘어온 상태가 남아있을 수 있어 명시적 리셋)
		PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Alive);
	}
}

//~ 세션 진행 ------------------------------------------------------------

void AOBExpeditionGameMode::CollectSpawnZones()
{
	AvailableZones.Reset();
	TeamZones.Reset();
	
	for (TActorIterator<AOBExpeditionSpawnZone> It(GetWorld()); It; ++It)
	{
		AvailableZones.Add(*It);
	}
	
	// Fisher-Yates 셔플 -> 파티별 랜덤 배정을 위해.
	for (int32 i = AvailableZones.Num() - 1; i > 0; --i)
	{
		AvailableZones.Swap(i, FMath::RandRange(0, i));
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Expedition] SpawnZones found = %d"), AvailableZones.Num());
}

AOBExpeditionSpawnZone* AOBExpeditionGameMode::GetOrAssignZoneForTeam(uint8 InTeamId)
{
	// 같은 파티(TeamId)는 항상 같은 존 -> 파티원이 함께 스폰
	if (TObjectPtr<AOBExpeditionSpawnZone>* Found = TeamZones.Find(InTeamId)) return *Found;
	
	// 새 팀 -> 미배정 존 하나 소비
	if (AvailableZones.Num() > 0)
	{
		AOBExpeditionSpawnZone* Zone = AvailableZones.Pop();
		TeamZones.Add(InTeamId, Zone);
		return Zone;
	}
	
	return nullptr; // 존 수 < 팀 수 -> 폴백(기본 PlayerStart)
}

AActor* AOBExpeditionGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (bEnableHelicopterInsertion && !bInsertionHasCompleted)
	{
		// Engine callers may still ask for a start while insertion initializes.
		// Do not collect or assign a legacy SpawnZone in that phase.
		return nullptr;
	}

	if (!bZonesCollected)   // ★ 첫 스폰 직전에 확실히 수집(존 배치 이후 시점 보장)
	{
		CollectSpawnZones();
		ValidateZoneSeparation();
		bZonesCollected = true;
	}
	
	if (Player)
	{
		if (AOBPlayerStateBase* PS = Player->GetPlayerState<AOBPlayerStateBase>())
		{
			if (AOBExpeditionSpawnZone* Zone = GetOrAssignZoneForTeam(PS->GetTeamId()))
			{
				UE_LOG(LogTemp, Log, TEXT("[Expedition] Team %d → Zone %s"), PS->GetTeamId(), *Zone->GetName());
				
				return Zone;
			}
		}
	}
	
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AOBExpeditionGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	if (bEnableHelicopterInsertion && !bInsertionHasCompleted)
	{
		OB_HELICOPTER_SPAWN_LOG(LogTemp, Error,
			TEXT("[Insertion] Blocked legacy RestartPlayerAtPlayerStart for %s (StartSpot=%s)."),
			*GetNameSafe(NewPlayer), *GetNameSafe(StartSpot));
		return;
	}

	// [개인 탈출] 스폰 지점(존)이 확정된 지금 시점에 스폰기준 배정.
	if (NewPlayer && StartSpot && (!bEnableHelicopterInsertion || bInsertionHasCompleted))
	{
		AssignPersonalExtractsFor(NewPlayer, StartSpot->GetActorLocation());
	}

	// 시작지점이 존이면 반경 내 랜덤(네비 투영) 트랜스폼으로 스폰 -> 파티원 산개
	if (AOBExpeditionSpawnZone* Zone = Cast<AOBExpeditionSpawnZone>(StartSpot))
	{
		const FTransform T = Zone->GetScatteredSpawnTransform();

		// 월드 파티션은 컨트롤러(뷰 타깃)를 스트리밍 소스로 쓴다. 폰이 없는 지금은
		// 컨트롤러 자신이 소스라, 먼저 옮겨야 스폰 지점 셀부터 로딩이 시작된다.
		// AController는 SetActorLocation을 숨겨놨으므로 전용 API를 쓴다.
		if (NewPlayer)
		{
			NewPlayer->SetInitialLocationAndRotation(T.GetLocation(), T.Rotator());
		}

		RestartPlayerAtTransform(NewPlayer, T);

		// 셀이 올라올 때까지 낙하 금지.
		if (NewPlayer)
		{
			if (AOBCharacterBase* Char = Cast<AOBCharacterBase>(NewPlayer->GetPawn()))
			{
				Char->HoldUntilGrounded();
			}
		}
		
		return;
	}
	
	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
}

void AOBExpeditionGameMode::Logout(AController* Exiting)
{
	const AOBPlayerStateBase* ExitingPlayerState =
		Exiting ? Exiting->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	const uint8 ExitingTeamId =
		ExitingPlayerState ? ExitingPlayerState->GetTeamId() : 0;

	PendingInsertionControllers.Remove(Exiting);
	Super::Logout(Exiting);

	if (ExitingTeamId != 0)
	{
		NormalizePartyLeaderForTeam(ExitingTeamId);
	}
	
	if (const AOBExpeditionGameState* GS = GetExpeditionGameState(); GS && GS->GetPhase() == EOBExpeditionPhase::Insertion)
	{
		TryCompleteInsertion();
	}
	else
	{
		CheckEndConditions();
	}
	
	// 개인 탈출구는 이제 팀 공유다. 한 명 나갔다고 파괴하면 남은 팀원의 탈출구가 사라진다.
	// 팀 전원이 나가도 세션 종료 시 레벨과 함께 정리되므로 개별 파괴는 하지 않는다.
	PartyCodeByController.Remove(Exiting);
}

void AOBExpeditionGameMode::HandlePartyLeaderClaim(
	AOBPlayerController* RequestingPlayer,
	const bool bRequestedLeader)
{
	if (!HasAuthority() || !IsValid(RequestingPlayer))
	{
		return;
	}

	AOBPlayerStateBase* RequestingPlayerState =
		RequestingPlayer->GetPlayerState<AOBPlayerStateBase>();
	if (!IsValid(RequestingPlayerState) ||
		RequestingPlayerState->GetTeamId() == 0)
	{
		return;
	}

	// The client value is only a claim. The first valid server PlayerArray
	// member remains leader so multiple clients cannot grant themselves target
	// selection permission.
	NormalizePartyLeaderForTeam(RequestingPlayerState->GetTeamId());

	OB_HELICOPTER_SPAWN_LOG(
		LogOBInsertion,
		Verbose,
		TEXT("[InsertionLeader] Claim PC=%s Team=%d Requested=%s Effective=%s"),
		*GetNameSafe(RequestingPlayer),
		RequestingPlayerState->GetTeamId(),
		bRequestedLeader ? TEXT("true") : TEXT("false"),
		RequestingPlayerState->IsPartyLeader() ? TEXT("true") : TEXT("false"));
}

void AOBExpeditionGameMode::NormalizePartyLeaderForTeam(const uint8 TeamId)
{
	if (!HasAuthority() || TeamId == 0 || !GameState)
	{
		return;
	}

	AOBPlayerStateBase* SelectedLeader = nullptr;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AOBPlayerStateBase* TeamMember = Cast<AOBPlayerStateBase>(PlayerState);
		if (IsValid(TeamMember) && TeamMember->GetTeamId() == TeamId)
		{
			SelectedLeader = TeamMember;
			break;
		}
	}

	if (!IsValid(SelectedLeader))
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AOBPlayerStateBase* TeamMember = Cast<AOBPlayerStateBase>(PlayerState);
		if (!IsValid(TeamMember) || TeamMember->GetTeamId() != TeamId)
		{
			continue;
		}

		TeamMember->SetPartyLeader(TeamMember == SelectedLeader);
		if (AOBPlayerController* TeamController =
			Cast<AOBPlayerController>(TeamMember->GetOwningController()))
		{
			TeamController->RefreshInsertionTransitSelectionPermission();
		}
	}
}

void AOBExpeditionGameMode::ValidateZoneSeparation() const
{
	// 3~5분 이격 검증(디자이너 배치 실수감지). 실패해도 게임은 진행(경고만)
	for (int32 i = 0; i < AvailableZones.Num(); ++i)
	{
		for (int32 k = i + 1; k < AvailableZones.Num(); ++k)
		{
			if (!AvailableZones[i] || !AvailableZones[k]) continue;
			const float D = FVector::Dist(AvailableZones[i]->GetActorLocation(), AvailableZones[k]->GetActorLocation());
			if (D < MinZoneSeparation)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Expedition] Spawn zones too close: %.0f < %.0f (파밍 이격 부족)"), D, MinZoneSeparation);
			}
		}
	}
}

void AOBExpeditionGameMode::StartExpedition()
{
	AOBExpeditionGameState* GS = GetExpeditionGameState();
	if (!GS) return;
	if (GS->GetPhase() == EOBExpeditionPhase::InProgress) return;
	
	const int32 Len = ResolveSessionLength();
	GS->SetSessionLength(Len);
	GS->SetTimeRemaining(Len);
	GS->SetFinalMinute(false);
	GS->SetPhase(EOBExpeditionPhase::InProgress);
	
	bExpeditionEnded = false;
	
	GetWorldTimerManager().SetTimer(
		SessionTimerHandle, this, &AOBExpeditionGameMode::TickSessionTimer, 1.f, /*bLoop=*/true);
}

void AOBExpeditionGameMode::TickSessionTimer()
{
	AOBExpeditionGameState* GS = GetExpeditionGameState();
	if (!GS) return;
	
	const int32 Remaining = GS->GetTimeRemaining() - 1;
	GS->SetTimeRemaining(Remaining); // 복제 -> 전 클라 HUD 갱신
	
	// 막바지 진입(경계에서 1회만 true 세팅됨. GameState 세터가 중복 무시
	if (Remaining <= FinalMinuteThreshold)
		GS->SetFinalMinute(true);
	
	// 시간 초과 -> 종료
	if (Remaining <= 0)
		EndExpedition(EOBExpeditionEndReason::TimedOut);
}

void AOBExpeditionGameMode::EndExpedition(EOBExpeditionEndReason Reason)
{
	if (bExpeditionEnded) return; // 중복 방지(시간초과와 전원종료가 겹칠 수 있음).
	bExpeditionEnded = true;

	// 타이머 정지.
	GetWorldTimerManager().ClearTimer(SessionTimerHandle);

	if (AOBExpeditionGameState* GS = GetExpeditionGameState())
	{
		// 종료 시점에 아직 탈출 못 한 인원(Alive/Downed)은 실패(Dead)로 확정
		TSet<uint8> TouchedTeams;
		if (GameState)
		{
			for (APlayerState* PS : GameState->PlayerArray)
			{
				if (AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS))
				{
					const EOBPlayerExpeditionStatus S = OBPS->GetExpeditionStatus();
					if (S == EOBPlayerExpeditionStatus::Alive || S == EOBPlayerExpeditionStatus::Downed)
					{
						OBPS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Dead);
						TouchedTeams.Add(OBPS->GetTeamId());
					}
				}
			}
		}
		
		// 시간초과로 죽은 인원도 사망화면을 받아야 한다(관전 정리 + ClientTeamWiped).
		for (const uint8 Team : TouchedTeams)
		{
			UpdateSpectatorsForTeam(Team);
		}
		
		GS->SetPhase(EOBExpeditionPhase::Ended);
	}
}

void AOBExpeditionGameMode::CheckEndConditions()
{
	if (bExpeditionEnded) return;

	// PlayerArray를 훑어 "아직 게임 중"인 플레이어가 있는지 확인.
	// - Alive 또는 Downed(다운은 아직 부활 여지) 이면 계속 진행.
	// - 전원이 Extracted/Dead면 종료.
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS))
		{
			const EOBPlayerExpeditionStatus S = OBPS->GetExpeditionStatus();
			if (S == EOBPlayerExpeditionStatus::Alive || S == EOBPlayerExpeditionStatus::Downed)
			{
				return; // 아직 진행 중인 플레이어 존재 → 종료 안 함.
			}
		}
	}

	EndExpedition(EOBExpeditionEndReason::AllResolved);
}

//~ 헬퍼 -----------------------------------------------------------------

AOBExpeditionGameState* AOBExpeditionGameMode::GetExpeditionGameState() const
{
	// GameState는 GetGameState<T>()로 안전 캐스팅.
	return GetGameState<AOBExpeditionGameState>();
}

int32 AOBExpeditionGameMode::ResolveMaxPlayers() const
{
	return ActiveMapData ? ActiveMapData->MaxSessionPlayers : 12;
}

int32 AOBExpeditionGameMode::ResolveSessionLength() const
{
	return ActiveMapData ? ActiveMapData->SessionLength : SessionLength; // 폴백 = inline
}

UOBExpeditionMapData* AOBExpeditionGameMode::ResolveMapData()
{
	if (MapData) return MapData; // 명시 지정이 있으면 우선(특수 케이스)
	
	if (MapCatalog)
	{
		// 현재 레벨 이름과 카탈로그의 각 MapData.Level 이름을매칭
		const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefix=*/true);
		for (UOBExpeditionMapData* M : MapCatalog->AvailableMaps)
		{
			if (!M || M->Level.IsNull()) continue;
			const FString ShortName = FPackageName::GetShortName(M->Level.ToSoftObjectPath().GetLongPackageName());
			if (ShortName.Equals(CurrentLevel, ESearchCase::IgnoreCase))
				return M;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("[Expedition] 카탈로그에서 현재 레벨(%s) MapData 미발견 → 폴백값 사용"), *CurrentLevel);
	}
	
	return nullptr;
}

void AOBExpeditionGameMode::CollectPersonalExtractPoints()
{
	PersonalExtractPoints.Reset();
	
	// 레벨에 배치된 TargetPoint 중 Actor Tag "PersonalExtract" 인 것만 수집
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("PersonalExtract")))
		{
			PersonalExtractPoints.Add(*It);
		}
	}
	for (TActorIterator<AOBExtractionSite> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("PersonalExtract")))
		{
			PersonalExtractPoints.AddUnique(*It);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Expedition] PersonalExtractPoints = %d"), PersonalExtractPoints.Num());
}

TArray<AActor*> AOBExpeditionGameMode::SelectPersonalMarkers(const FVector& SpawnOrigin, uint8 TeamId)
{
	TArray<AActor*> Result;
	if (PersonalExtractPoints.Num() == 0) return Result; // 탈출구 없으면 반환
	
	// 1. 먼거리 필터(직선 XY) 없으면 완화(전체)
	TArray<AActor*> Candidates;
	for (const TObjectPtr<AActor>& M : PersonalExtractPoints)
	{
		// 스폰 위치에서 충분히 먼 탈출구만 후보로 선택 (기준 거리 이상인 경우 탈출구 후보에 포함)
		if (M && FVector::Dist2D(SpawnOrigin, M->GetActorLocation()) >= MinSpawnDistance)
		{
			Candidates.Add(M);
		}
	}
	// 조건에 만족하는 후보가 없으면 전체를 후보로 사용
	if (Candidates.Num() == 0)
	{
		for (const TObjectPtr<AActor>& M : PersonalExtractPoints)
		{
			if (M)
			{
				Candidates.Add(M);
			}
		}
	}
	if (Candidates.Num() == 0) return Result;
	
	// 2. 먼 순 정렬 → 상위 FarPool(변동 여지).
	Candidates.Sort([&SpawnOrigin](const AActor& L, const AActor& R)
	{
		return FVector::Dist2D(SpawnOrigin, L.GetActorLocation()) > FVector::Dist2D(SpawnOrigin, R.GetActorLocation());
	});
	const int32 PoolN = FMath::Clamp(FarPoolSize, 1, Candidates.Num());
	TArray<AActor*> FarPool(Candidates.GetData(), PoolN);
	
	// 같은 팀이 이전에 사용한 중심 탈출구 제외
	TSet<TObjectPtr<AActor>>& Used = TeamUsedCenters.FindOrAdd(TeamId);
	
	// FarPool 중 팀 미사용 후보(소진 시 중복 허용 완화).
	auto BuildAvailable = [&](TArray<AActor*>& Out)
	{
		Out.Reset();
		for (AActor* M : FarPool)
			if (M && !Used.Contains(M)) Out.Add(M);
		if (Out.Num() == 0)
			for (AActor* M : FarPool) if (M) Out.Add(M);
	};

	// --- 1번 탈출구: 팀 미사용 후보 중 랜덤(세션 변동성) ---
	TArray<AActor*> Avail;
	BuildAvailable(Avail);
	if (Avail.Num() == 0) return Result;

	AActor* First = Avail[FMath::RandRange(0, Avail.Num() - 1)];
	Used.Add(First);
	Result.Add(First);
	
	// --- 2번 탈출구: 1번과 방향각이 충분히 벌어진 후보 중 랜덤 ---
	const FVector Dir1 = (First->GetActorLocation() - SpawnOrigin).GetSafeNormal2D();
	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(MinDirectionAngleDeg));
	
	BuildAvailable(Avail); // First가 Used에 반영된 상태로 다시 구성
	
	TArray<AActor*> Divergent;
	for (AActor* M : Avail)
	{
		if (M == First) continue;
		const FVector Dir2 = (M->GetActorLocation() - SpawnOrigin).GetSafeNormal2D();
		// dot <= cos(임계) ⟺ 두 방향의 각도 >= 임계
		if (FVector::DotProduct(Dir1, Dir2) <= CosThreshold)
			Divergent.Add(M);
	}

	// 방향 조건 만족이 없으면 방향 무시하고 남은 것 중 랜덤(완화)
	TArray<AActor*>& Pool2 = (Divergent.Num() > 0) ? Divergent : Avail;
	TArray<AActor*> SecondCands;
	for (AActor* M : Pool2)
	{
		if (M && M != First)
		{
			SecondCands.Add(M);
		}
	}
	
	if (SecondCands.Num() > 0)
	{
		AActor* Second = SecondCands[FMath::RandRange(0, SecondCands.Num() - 1)];
		Used.Add(Second);
		Result.Add(Second);
	}
	
	return Result; // 최대 2개
}

FString AOBExpeditionGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	
	// URL ?party=<코드> 파싱 → 팀 배정(GenericPlayerInitialization)에서 사용.
	const FString PartyCode = UGameplayStatics::ParseOption(Options, TEXT("party"));
	if (NewPlayerController)
	{
		PartyCodeByController.Add(NewPlayerController, PartyCode);
		
		// [중요] 팀 배정을 스폰(ChoosePlayerStart) 이전인 여기서 수행.
		if (AOBPlayerStateBase* PS = NewPlayerController->GetPlayerState<AOBPlayerStateBase>();
			PS && PS->GetTeamId() == 0)
		{
			PS->SetTeamId(ResolveTeamForCode(PartyCode));
		}
	}
	
	return Result;
}

uint8 AOBExpeditionGameMode::ResolveTeamForCode(const FString& PartyCode)
{
	FString EffectiveCode = PartyCode;
	if (EffectiveCode.IsEmpty() && bUseSharedTeam)
		EffectiveCode = TEXT("__SHARED__");

	if (!EffectiveCode.IsEmpty())
	{
		if (uint8* Existing = PartyTeams.Find(EffectiveCode))
			return *Existing;                       // 같은 코드 → 같은 팀
		const uint8 NewTeam = NextTeamId++;
		PartyTeams.Add(EffectiveCode, NewTeam);
		return NewTeam;                             // 새 코드 → 고유 팀
	}
	return NextTeamId++;                            // 솔로(협동 아님) → 고유 팀
}

bool AOBExpeditionGameMode::ShouldEnterDownedState(AController* C) const
{
	return HasLivingTeammate(C);
}

TArray<AOBPlayerStateBase*> AOBExpeditionGameMode::GetLivingTeammates(uint8 TeamId) const
{
	TArray<AOBPlayerStateBase*> Out;
	if (!GameState) return Out;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS);
		if (OBPS && OBPS->GetTeamId() == TeamId
			&& OBPS->GetExpeditionStatus() == EOBPlayerExpeditionStatus::Alive)
		{
			Out.Add(OBPS);
		}
	}
	
	return Out;
}

bool AOBExpeditionGameMode::HasLivingTeammate(AController* C) const
{
	const AOBPlayerStateBase* Me = C ? C->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	if (!Me) return false;

	for (const AOBPlayerStateBase* PS : GetLivingTeammates(Me->GetTeamId()))
	{
		if (PS != Me) return true;   // 나 자신 제외
	}
	
	return false;
}

void AOBExpeditionGameMode::UpdateSpectatorsForTeam(uint8 TeamId)
{
	if (!GameState) return;

	const TArray<AOBPlayerStateBase*> Living = GetLivingTeammates(TeamId);

	for (APlayerState* PS : GameState->PlayerArray)
	{
		AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS);
		if (!OBPS || OBPS->GetTeamId() != TeamId) continue;
		if (OBPS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Dead) continue;

		AOBPlayerController* PC = Cast<AOBPlayerController>(OBPS->GetOwningController());
		if (!PC) continue;

		if (Living.IsEmpty())
		{
			PC->ClientTeamWiped();   // 관전 종료 → 홈 복귀 화면
			continue;
		}

		// 보던 대상이 더는 생존자가 아니면 첫 생존자로 자동 전환.
		const AActor* Current = PC->GetViewTarget();
		const bool bStillValid = Living.ContainsByPredicate(
			[Current](const AOBPlayerStateBase* P) { return P->GetPawn() == Current; });

		if (!bStillValid)
		{
			if (APawn* TargetPawn = Living[0]->GetPawn())
			{
				PC->ClientBeginSpectate();     // 중복 가드 내장
				PC->SetViewTarget(TargetPawn); // 서버에서 호출해야 렐러번시까지 따라옴
			}
		}
	}
}

void AOBExpeditionGameMode::NotifyPlayerDowned(AController* C)
{
	if (!C) return;
	
	uint8 Team = 0;
	if (AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Downed);
		Team = PS->GetTeamId();
	}
	
	// 블리드아웃 타이머
	FTimerHandle& T = BleedOutTimers.FindOrAdd(C);
	FTimerDelegate D = FTimerDelegate::CreateUObject(this, &AOBExpeditionGameMode::FinishDownedPlayer, C);
	GetWorldTimerManager().SetTimer(T, D, BleedOutSeconds, false);
	
	// 마지막 생존자가 다운됐을 수 있음 -> 팀 전멸 검사
	CheckTeamWipe(Team);
}

void AOBExpeditionGameMode::RevivePlayer(AController* C)
{
	if (!C) return;
	
	if (FTimerHandle* T = BleedOutTimers.Find(C))
	{
		GetWorldTimerManager().ClearTimer(*T);
		BleedOutTimers.Remove(C);
	}
	if (AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>())
	{
		if (PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Downed) return;
		PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Alive);
	}
	if (APawn* P = C->GetPawn())
		if (AOBCharacterBase* Char = Cast<AOBCharacterBase>(P))
			Char->ReviveFromDowned(ReviveHealthPercent);
}

void AOBExpeditionGameMode::FinishDownedPlayer(AController* C)
{
	if (!C) return;
	if (FTimerHandle* T = BleedOutTimers.Find(C))
	{
		GetWorldTimerManager().ClearTimer(*T);
		BleedOutTimers.Remove(C);
	}
	
	uint8 Team = 0;
	if (AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>())
	{
		if (PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Downed) return; // 이미 부활 처리됨
		PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Dead);
		Team = PS->GetTeamId();
	}
	if (APawn* P = C->GetPawn())
		if (AOBCharacterBase* Char = Cast<AOBCharacterBase>(P))
			Char->FinishDeathFromDowned();
	
	CheckTeamWipe(Team);
	UpdateSpectatorsForTeam(Team);   // 관전 대상이 죽었을 수 있음
	CheckEndConditions();
}

void AOBExpeditionGameMode::CheckTeamWipe(uint8 TeamId)
{
	if (!GameState) return;
	
	bool bAnyAlive = false;
	TArray<AController*> DownedControllers;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS);
		if (!OBPS || OBPS->GetTeamId() != TeamId) continue;
		
		const EOBPlayerExpeditionStatus S = OBPS->GetExpeditionStatus();
		if (S == EOBPlayerExpeditionStatus::Alive)
			bAnyAlive = true;
		else if (S == EOBPlayerExpeditionStatus::Downed)
			DownedControllers.Add(OBPS->GetOwningController());
	}
	
	// 팀에 Alive 없음 -> 다운자 전원 사망(팀 전멸, 부활 불가)
	if (!bAnyAlive)
	{
		for (AController* DC : DownedControllers)
			FinishDownedPlayer(DC);
	}
}

void AOBExpeditionGameMode::AssignPersonalExtractsFor(AController* C, const FVector& SpawnOrigin)
{
	if (!C || !PersonalExtractClass) return;
	
	const AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>();
	const uint8 TeamId = PS ? PS->GetTeamId() : 0;

	// TeamId 0은 "미배정" 예약값. 팀 없이 배정하면 전원 공용으로 새어버린다.
	if (TeamId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Expedition] %s TeamId 미배정 → 개인탈출 배정 생략"), *C->GetName());
		return;
	}
	
	// 팀 단위 1회 배정. 뒤늦게 합류한 팀원에게는 좌표만 다시 실어 준다.
	if (PersonalZones.Contains(TeamId))
	{
		PushPersonalExtractsToTeam(TeamId);
		return;
	}
	
	if (!bPersonalPointsCollected)
	{
		CollectPersonalExtractPoints();
		bPersonalPointsCollected = true;
	}
	if (PersonalExtractPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Expedition] 개인 탈출 마커 없음 → Team %d 미배정"), TeamId);
		return;
	}
	
	TArray<AActor*> Markers = SelectPersonalMarkers(SpawnOrigin, TeamId);
	
	// 맵별 활성창(없으면 폴백 0~600)
	const int32 StartSec = ActiveMapData ? ActiveMapData->PersonalActiveStartSec : 0;
	const int32 EndSec = ActiveMapData ? ActiveMapData->PersonalActiveEndSec : 600;
	
	TArray<TObjectPtr<AOBExtractionZone>>& Zones = PersonalZones.FindOrAdd(TeamId).Zones;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	
	for (AActor* M : Markers)
	{
		if (!M) continue;
		
		// 네비 위로 스냅(도달 가능 위치 보장)
		const FVector MarkerLoc = M->GetActorLocation();
		FVector Loc = MarkerLoc;
		if (Nav)
		{
			FNavLocation Projected;
			// XY까지 500을 열어두면 마커에서 최대 5m 옆으로 끌려간다.
			// 필요한 건 높이 보정뿐이므로 수평 여유는 좁힌다.
			if (Nav->ProjectPointToNavigation(Loc, Projected, FVector(100.f, 100.f, 500.f)))
			{
				Loc = Projected.Location;
			}
		}

		const float SnapDelta = FVector::Dist(MarkerLoc, Loc);
		UE_LOG(LogTemp, Log,
			TEXT("[ExtractionDebug] Personal spawn Team=%d Marker=%s MarkerLoc=%s SpawnLoc=%s Delta=%.1fcm MarkerRot=%s"),
			TeamId, *M->GetName(), *MarkerLoc.ToCompactString(), *Loc.ToCompactString(),
			SnapDelta, *M->GetActorRotation().ToCompactString());
		if (SnapDelta > 200.f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ExtractionDebug] %s 마커가 네비메시에서 %.1fcm 떨어져 있다. 마커를 네비가 깔린 평지 위로 옮길 것."),
				*M->GetName(), SnapDelta);
		}

		const FTransform T(M->GetActorRotation(), Loc);
		
		// 지연 스폰 -> owner-only + 활성창 설정 후 FinishSpawning.
		AOBExtractionZone* Zone = GetWorld()->SpawnActorDeferred<AOBExtractionZone>(
			PersonalExtractClass, T, /*Owner*/nullptr, /*Instigator*/nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Zone) continue;
		
		Zone->ConfigureAsPersonal(TeamId);			// 팀 전원에게 복제(본인만 X)
		if (AOBExtractionSite* ExtractionSite = Cast<AOBExtractionSite>(M))
		{
			Zone->ConfigureExtractionSite(ExtractionSite);
		}
		Zone->SetActiveWindow(StartSec, EndSec);	// 맵 별 활성창
		UGameplayStatics::FinishSpawningActor(Zone, T);
		
		Zones.Add(Zone);
#if ENABLE_DRAW_DEBUG
		if (bDrawDebugPersonalExtract)
		{
			DrawDebugLine(GetWorld(), SpawnOrigin, Loc, FColor::Cyan, false, 30.f, 0, 20.f);
			DrawDebugSphere(GetWorld(), Loc, 200.f, 12, FColor::Cyan, false, 30.f, 0, 10.f);
		}
#endif
	}
	
	PushPersonalExtractsToTeam(TeamId);
	
	UE_LOG(LogTemp, Log, TEXT("[Expedition] Team %d 개인탈출 %d개 배정(팀 공유)"), TeamId, Zones.Num());
}

void AOBExpeditionGameMode::RequestRespawn(AController* Controller, APawn* DeadPawn)
{
	// [무리스폰 + 탈락 확정] Expedition은 리스폰하지 않는다.
	// - 여기서 개인 상태를 Dead로 확정하고, 세션 종료 조건을 다시 검사한다.
	if (!Controller) return;

	uint8 Team = 0;
	if (AOBPlayerStateBase* PS = Controller->GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Dead);
		Team = PS->GetTeamId();
	}

	UpdateSpectatorsForTeam(Team);   // 방금 죽은 본인 = 관전 시작 or 전멸
	CheckEndConditions();
}

void AOBExpeditionGameMode::NotifyPlayerExtracted(AController* Controller)
{
	if (!Controller) return;

	AOBPlayerStateBase* PS = Controller->GetPlayerState<AOBPlayerStateBase>();
	// 살아있는 플레이어만 탈출 가능(중복/사망자 무효 처리).
	if (!PS || PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive) return;
	
	// 폰을 정리하기 전에 가방을 찍는다. HandleExtracted가 장비를 해제한다.
	if (AOBCharacterBase* Char = Cast<AOBCharacterBase>(Controller->GetPawn()))
	{
		if (AOBPlayerController* OwningPC = Cast<AOBPlayerController>(Controller))
		{
			OwningPC->Client_ApplyExtractionResultV2(
				Char->GetExtractionStackGains(),
				Char->GetLootedUniqueItemInstances(),
				Char->GetReturnedLoadoutItemInstances());
		}
	}

	PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Extracted);
	PS->SetExtractionProgress(0.f, false);
	
	UpdateSpectatorsForTeam(PS->GetTeamId());   // 탈출자는 관전 대상에서 빠짐

	UE_LOG(LogTemp, Log, TEXT("[Expedition] Player extracted: %s"), *PS->GetPlayerName());

	// 탈출한 폰은 월드에서 제거(관전/결과는 Step 8). 충돌·표시·이동 정리.
	if (APawn* Pawn = Controller->GetPawn())
	{
		if (AOBCharacterBase* Char = Cast<AOBCharacterBase>(Pawn))
		{
			Char->HandleExtracted(); // ★ 능력 취소 + 무기 해제
		}
		Pawn->SetActorHiddenInGame(true);
		Pawn->SetActorEnableCollision(false);
		if (UPawnMovementComponent* Move = Pawn->GetMovementComponent())
		{
			Move->StopMovementImmediately();
		}
	}

	// 남은 인원 재평가 → 전원 Extracted/Dead면 세션 종료.
	CheckEndConditions();
}

void AOBExpeditionGameMode::CollectPublicExtractsForMap()
{
	AOBExpeditionGameState* GS = GetGameState<AOBExpeditionGameState>();
	if (!GS) return;

	TArray<FVector_NetQuantize> Locations;
	for (TActorIterator<AOBExtractionZone> It(GetWorld()); It; ++It)
	{
		if (AOBExtractionZone* Zone = *It)
		{
			Locations.Add(FVector_NetQuantize(Zone->GetActorLocation()));
		}
	}

	GS->SetPublicExtractLocations(Locations);

	// 0개면 레벨 배치 존이 스트리밍으로 안 잡힌 것이다.
	if (Locations.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Map] 공용 탈출구가 0개다. 레벨 배치 AOBExtractionZone의 WorldPartition > Is Spatially Loaded 체크를 해제할 것."));
	}
}

void AOBExpeditionGameMode::PushPersonalExtractsToTeam(uint8 TeamId)
{
	const FOBPersonalZoneList* Found = PersonalZones.Find(TeamId);
	if (!Found) return;

	TArray<FVector_NetQuantize> Locations;
	for (const TObjectPtr<AOBExtractionZone>& Zone : Found->Zones)
	{
		if (Zone)
		{
			Locations.Add(FVector_NetQuantize(Zone->GetActorLocation()));
		}
	}

	// 같은 팀 전원의 PlayerState에 싣는다. 각자 소유자 전용으로만 복제된다.
	for (APlayerState* PS : GameState->PlayerArray)
	{
		AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS);
		if (OBPS && OBPS->GetTeamId() == TeamId)
		{
			OBPS->SetPersonalExtractLocations(Locations);
		}
	}

	// UE_LOG(LogTemp, Log, TEXT("[Map] Team %d 개인 탈출구 %d개 배포."), TeamId, Locations.Num());
}

void AOBExpeditionGameMode::UpdateTeammateMapLocations()
{
	if (!GameState) return;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		AOBPlayerStateBase* Me = Cast<AOBPlayerStateBase>(PS);
		if (!Me) continue;

		TArray<FVector_NetQuantize> Locations;
		for (APlayerState* Other : GameState->PlayerArray)
		{
			const AOBPlayerStateBase* OtherPS = Cast<AOBPlayerStateBase>(Other);
			if (!OtherPS || OtherPS == Me) continue;
			
			// TeamId 0 = 미배정. 여기 걸러내지 않으면 미배정끼리 한 팀으로 보인다.
			if (OtherPS->GetTeamId() == 0 || OtherPS->GetTeamId() != Me->GetTeamId()) continue;
			
			// 죽거나 탈출한 팀원은 지도에서 뺀다.
			if (OtherPS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive) continue;

			if (const APawn* Pawn = OtherPS->GetPawn())
			{
				Locations.Add(FVector_NetQuantize(Pawn->GetActorLocation()));
			}
		}

		Me->SetTeammateMapLocations(Locations);
	}
}
