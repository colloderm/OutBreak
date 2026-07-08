// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameMode/OBExpeditionGameMode.h"

#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Engine/TargetPoint.h"
#include "Game/Expedition/OBExpeditionMapCatalog.h"
#include "Game/Expedition/OBExpeditionMapData.h"
#include "Game/Expedition/OBExpeditionSpawnZone.h"
#include "Game/Expedition/OBExtractionZone.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Player/State/OBPlayerStateBase.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

AOBExpeditionGameMode::AOBExpeditionGameMode()
{
	GameSessionClass = AOBExpeditionGameState::StaticClass();
}

void AOBExpeditionGameMode::StartPlay()
{
	Super::StartPlay();
	
	ActiveMapData = ResolveMapData();
	
	// [데디 정원 강제] GameSession은 InitGame 단계에서 이미 생성됨.
	// MaxPlayers는 원격 클라 접속 시 PreLogin의 AtCapacity() 판정에 사용됨.
	if (GameSession)
		GameSession->MaxPlayers = ResolveMaxPlayers();
	
	StartExpedition();
}

void AOBExpeditionGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	
	if (!C) return;
	
	// 진입 플레이어의 세션 초기화
	if (AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>())
	{
		// TeamID 부여: 협동이면 전원 1, 개인전이면 고유값
		const uint8 AssignedTeam = bUseSharedTeam ? 1 : NextTeamId++;
		PS->SetTeamId(AssignedTeam);
		
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
	// [개인 탈출] 스폰 지점(존)이 확정된 지금 시점에 스폰기준 배정.
	if (NewPlayer && StartSpot)
	{
		AssignPersonalExtractsFor(NewPlayer, StartSpot->GetActorLocation());
	}

	// 시작지점이 존이면 반경 내 랜덤(네비 투영) 트랜스폼으로 스폰 -> 파티원 산개
	if (AOBExpeditionSpawnZone* Zone = Cast<AOBExpeditionSpawnZone>(StartSpot))
	{
		RestartPlayerAtTransform(NewPlayer, Zone->GetScatteredSpawnTransform());
		return;
	}
	
	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
}

void AOBExpeditionGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	
	// 남은 인원 기준으로 종료 여부 재평가.
	CheckEndConditions();
	
	if (FOBPersonalZoneList* Zones = PersonalZones.Find(Exiting))
	{
		for (const TObjectPtr<AOBExtractionZone>& Z : Zones->Zones)
		{
			if (Z) Z->Destroy();
		}
		PersonalZones.Remove(Exiting);
	}
}

void AOBExpeditionGameMode::ValidateZoneSeparation() const
{
	// 3~5분 이격 검증(디자이너 배치 싨 ㅜ감지). 실패해도 게임은 진행(경고만)
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
		GS->SetPhase(EOBExpeditionPhase::Ended);
	}

	// TODO(Step 8): 팀별 승/패 산정, 결과 위젯 표시, 잠시 후 로비/Home으로 ServerTravel.
	UE_LOG(LogTemp, Log, TEXT("[Expedition] Ended. Reason=%d"), (int32)Reason);
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
	
	// 2. 먼 순 정렬.
	Candidates.Sort([&SpawnOrigin](const AActor& L, const AActor& R)
	{
		return FVector::Dist2D(SpawnOrigin, L.GetActorLocation()) > FVector::Dist2D(SpawnOrigin, R.GetActorLocation());
	});
	
	// 3. 상위 FarPool 중, 팀이 아직 안 쓴 중심을 랜덤 선정
	const int32 PoolN = FMath::Clamp(FarPoolSize, 1, Candidates.Num());
	TArray<AActor*> FarPool(Candidates.GetData(), PoolN);
	
	// 같은 팀이 이전에 사용한 중심 탈출구 제외
	TSet<TObjectPtr<AActor>>& Used = TeamUsedCenters.FindOrAdd(TeamId);
	TArray<AActor*> Fresh;
	for (AActor* M : FarPool)
	{
		if (!Used.Contains(M))
		{
			Fresh.Add(M);
		}
	}
	
	TArray<AActor*>& CenterPool = (Fresh.Num() > 0) ? Fresh : FarPool; // 새로운 탈출구 후보가 고갈 시 선택
	if (Fresh.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Expedition] Team %d 개인탈출 중심풀 고갈 → 중복 허용 폴백"), TeamId);		
	}
	
	AActor* Center = CenterPool[FMath::RandRange(0, CenterPool.Num() - 1)];
	Used.Add(Center);
	Result.Add(Center);
	
	// 4~5 중심 주변 좌/우 각각 랜덤 1개.
	if (bAssignSideExtracts)
	{
		const FVector Dir = (Center->GetActorLocation() - SpawnOrigin).GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal(); // Up*Fwd = Right (외적)
		
		TArray<AActor*> LeftCands, RightCands;
		for (AActor* M : Candidates)
		{
			if (M == Center) continue;
			const FVector Off = M->GetActorLocation() - Center->GetActorLocation();
			if (Off.Size2D() > NeighborRadius) continue; // 중심 '주변'만
			
			const float Side = FVector::DotProduct(Off, Right); // + 우 / - 좌 (내적)
			if (Side > SideMinOffset)
				RightCands.Add(M);
			else if (Side < -SideMinOffset)
				LeftCands.Add(M);
		}
		
		if (LeftCands.Num() > 0)
			Result.Add(LeftCands[FMath::RandRange(0, LeftCands.Num() - 1)]);
		if (RightCands.Num() > 0)
			Result.Add(RightCands[FMath::RandRange(0, RightCands.Num() - 1)]);
	}
	
	return Result;
}

void AOBExpeditionGameMode::AssignPersonalExtractsFor(AController* C, const FVector& SpawnOrigin)
{
	if (!C || !PersonalExtractClass) return;
	if (PersonalZones.Contains(C)) return; // 1회만
	
	if (!bPersonalPointsCollected)
	{
		CollectPersonalExtractPoints();
		bPersonalPointsCollected = true;
	}
	if (PersonalExtractPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Expedition] 개인 탈출 마커 없음 → %s 미배정"), *C->GetName());
		return;
	}
	
	const AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>();
	const uint8 TeamId = PS ? PS->GetTeamId() : 0;
	
	TArray<AActor*> Markers = SelectPersonalMarkers(SpawnOrigin, TeamId);
	
	// 맵별 활성창(없으면 폴백 0~600)
	const int32 StartSec = ActiveMapData ? ActiveMapData->PersonalActiveStartSec : 0;
	const int32 EndSec = ActiveMapData ? ActiveMapData->PersonalActiveEndSec : 600;
	
	TArray<TObjectPtr<AOBExtractionZone>>& Zones = PersonalZones.FindOrAdd(C).Zones;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	
	for (AActor* M : Markers)
	{
		if (!M) continue;
		
		// 네비 위로 스냅(도달 가능 위치 보장)
		FVector Loc = M->GetActorLocation();
		if (Nav)
		{
			FNavLocation Projected;
			if (Nav->ProjectPointToNavigation(Loc, Projected, FVector(500.f)))
			{
				Loc = Projected.Location;
			}
		}
		const FTransform T(M->GetActorRotation(), Loc);
		
		// 지연 스폰 -> owner-only + 활성창 설정 후 FinishSpawning.
		AOBExtractionZone* Zone = GetWorld()->SpawnActorDeferred<AOBExtractionZone>(
			PersonalExtractClass, T, /*Owner*/nullptr, /*Instigator*/nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Zone) continue;
		
		Zone->ConfigureAsPersonal(C);				// 소유 클라에만 복제(본인만 보임)
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
	
	UE_LOG(LogTemp, Log, TEXT("[Expedition] %s(Team %d) 개인탈출 %d개 배정"), *C->GetName(), TeamId, Zones.Num());
}

void AOBExpeditionGameMode::RequestRespawn(AController* Controller, APawn* DeadPawn)
{
	// [무리스폰 + 탈락 확정] Expedition은 리스폰하지 않는다.
	// - 여기서 개인 상태를 Dead로 확정하고, 세션 종료 조건을 다시 검사한다.
	if (!Controller) return;

	if (AOBPlayerStateBase* PS = Controller->GetPlayerState<AOBPlayerStateBase>())
	{
		// TODO(Step 6): 다운/부활 도입 시 → 여기서 Alive를 우선 Downed로 두고,
		//   블리드아웃 만료 또는 팀 전멸 시점에 Dead로 확정하도록 분기 예정.
		//   현재(Step 4)는 다운 단계 없이 즉시 Dead.
		PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Dead);
	}

	// 죽은 폰: 서버측 이동/충돌 정지는 Character::StartDeath가 이미 처리.
	// (관전/시체 정리는 Step 6/8에서 결정 — 여기선 그대로 둔다)

	// 살아있는(또는 다운) 플레이어가 남았는지 재평가 → 없으면 세션 종료.
	CheckEndConditions();
}

void AOBExpeditionGameMode::NotifyPlayerExtracted(AController* Controller)
{
	if (!Controller) return;

	AOBPlayerStateBase* PS = Controller->GetPlayerState<AOBPlayerStateBase>();
	// 살아있는 플레이어만 탈출 가능(중복/사망자 무효 처리).
	if (!PS || PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive) return;

	PS->SetExpeditionStatus(EOBPlayerExpeditionStatus::Extracted);
	PS->SetExtractionProgress(0.f, false);

	UE_LOG(LogTemp, Log, TEXT("[Expedition] Player extracted: %s"), *PS->GetPlayerName());

	// 탈출한 폰은 월드에서 제거(관전/결과는 Step 8). 충돌·표시·이동 정리.
	if (APawn* Pawn = Controller->GetPawn())
	{
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
