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
#include "Character/OBCharacterBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Player/Controller/OBPlayerController.h"

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
		PartyCodeByController.Remove(Exiting);
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
		if (AOBPlayerStateBase* PS = NewPlayerController->GetPlayerState<AOBPlayerStateBase>())
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
