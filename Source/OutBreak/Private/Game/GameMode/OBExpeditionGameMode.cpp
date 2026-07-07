// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameMode/OBExpeditionGameMode.h"

#include "EngineUtils.h"
#include "Game/Expedition/OBExpeditionMapCatalog.h"
#include "Game/Expedition/OBExpeditionMapData.h"
#include "Game/Expedition/OBExpeditionSpawnZone.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "GameFramework/GameSession.h"
#include "Player/State/OBPlayerStateBase.h"
#include "GameFramework/PlayerState.h"
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
