// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameModeBase.h"
#include "Game/Expedition/OBHelicopterTypes.h"
#include "OBExpeditionGameMode.generated.h"

class AOBPlayerStateBase;
class AOBExtractionZone;
class UOBExpeditionMapCatalog;
class AOBExpeditionSpawnZone;
class UOBExpeditionMapData;
class AOBExpeditionGameState;
class AOBInsertionHelicopter;
class AOBInsertionTargetStreamingProxy;
class AOBHelicopterRoute;
class UOBLandingZoneScannerComponent;
class AOBPlayerController;

struct FOBPendingInsertionTargetRequest
{
	TArray<FVector> Candidates;
	int32 CandidateIndex = INDEX_NONE;
	TWeakObjectPtr<AOBPlayerController> FeedbackPlayer;
	bool bAutomatic = false;
	float RequestStartedServerTime = 0.f;
	float CandidateStartedServerTime = 0.f;
};

// 세션 종료 사유. 결과 위젯(Step 8)/로그용.
UENUM()
enum class EOBExpeditionEndReason : uint8
{
	TimedOut,     // 제한시간 초과
	AllResolved   // 살아있는 플레이어가 없음(전원 탈출 or 전멸)
};

// 개인 탈출구 배열 래퍼(UHT는 TMap 값에 TArray를 직접 못 씀 → struct로 감쌈).
USTRUCT()
struct FOBPersonalZoneList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AOBExtractionZone>> Zones;
};

UCLASS()
class OUTBREAK_API AOBExpeditionGameMode : public AOBGameModeBase
{
	GENERATED_BODY()
	
public:
	AOBExpeditionGameMode();
	
	// [무리스폰] 베이스는 지연 후 RestartPlayer. Expedition은 리스폰 금지 → 여기서 막는다.
	// - 실제 "사망 상태 마킹(Dead) + 종료판정"은 Step 4에서 이 안에 채운다.
	virtual void RequestRespawn(AController* Controller, APawn* DeadPawn) override;
	
	// 탈출 성공 처리(ExtractionZone이 호출). 상태=Extracted + 폰 정리 + 종료판정.
	void NotifyPlayerExtracted(AController* Controller);

	/** Server entry point used by AOBPlayerController's insertion-map RPC. */
	void RequestInsertionPoint(AOBPlayerController* RequestingPlayer, const FVector2D& WorldXY);

	/** Normalizes an untrusted client claim to one server-selected leader per team. */
	void HandlePartyLeaderClaim(AOBPlayerController* RequestingPlayer, bool bRequestedLeader);

	TSubclassOf<AOBInsertionHelicopter> GetDefaultExtractionHelicopterClass() const;
	
	// 팀별 존 배정에 따라 시작지점을 고른다.
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	// 존이면 반경 내 랜덤 위치로 스폰(파티원 산개).
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;
	
	// 플레이어 접속 종료. 마지막 생존자가 나갔을 수 있어 종료 조건을 재평가.
	virtual void Logout(AController* Exiting) override;

	// 종료된 탐사에 새 접속을 막는다. Phase=Ended가 복제되면 클라가 무조건
	// 결과화면을 띄우므로(OBPlayerController), 리사이클 전에는 아예 받지 않는다.
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	
	virtual bool ShouldEnterDownedState(AController* C) const override; // 팀 생존자 존재?
	virtual void NotifyPlayerDowned(AController* C) override;           // 블리드아웃 시작
	void RevivePlayer(AController* C);
	
	void FinishDownedPlayer(AController* C);   // 블리드아웃/전멸/디버그 자살 → 사망 확정
	bool HasLivingTeammate(AController* C) const;
	
	// 해당 팀의 살아있는 멤버(관전 대상 후보). PlayerArray 순서라 순환이 안정적.
	TArray<AOBPlayerStateBase*> GetLivingTeammates(uint8 TeamId) const;

	// 팀 생존자 구성이 바뀔 때 호출 → 그 팀 사망자들의 관전 대상/전멸 화면 갱신.
	void UpdateSpectatorsForTeam(uint8 TeamId);
	
protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	
	virtual void PreInitializeComponents() override;

	// 진입 플레이어 공통 초기화(신규 접속 + 심리스 트래블 양쪽에서 호출됨).
	// - TeamId 부여 + ExpeditionStatus=Alive 리셋.
	virtual void GenericPlayerInitialization(AController* C) override;

	//~ 세션 진행 ------------------------------------------------------------
	
	void CollectSpawnZones(); // 레벨의 존 수집 + 셔플
	
	AOBExpeditionSpawnZone* GetOrAssignZoneForTeam(uint8 InTeamId);
	
	void ValidateZoneSeparation() const; // 3~5분 이격 검증
	
	void StartExpedition();
	void BeginInsertionPhase();
	void RegisterPlayerForInsertion(APlayerController* NewPlayer);
	bool SpawnAndSeatInsertionPawn(APlayerController* NewPlayer, AOBInsertionHelicopter* Helicopter);
	AOBInsertionHelicopter* GetOrCreateInsertionHelicopter(uint8 TeamId);
	AOBHelicopterRoute* GetOrAssignInsertionRoute(uint8 TeamId);
	void CollectHelicopterRoutes();
	void AutoSelectInsertionPoint(uint8 TeamId);
	void BeginInsertionTargetResolution(
		uint8 TeamId,
		const TArray<FVector>& Candidates,
		AOBPlayerController* FeedbackPlayer,
		bool bAutomatic);
	void StartNextInsertionTargetCandidate(uint8 TeamId);
	void PollInsertionTargetStreaming(uint8 TeamId);
	void ValidateCurrentInsertionTarget(uint8 TeamId);
	void FinishInsertionTargetFailure(uint8 TeamId, const FString& Message);
	AOBInsertionTargetStreamingProxy* GetOrCreateInsertionTargetStreamingProxy(uint8 TeamId);
	void NotifyTeamInsertionPresentation(uint8 TeamId, EOBInsertionPhase Phase, const FString& Message, bool bForceMapOpen);
	void ReleaseInsertionTargetStreamingProxy(uint8 TeamId);
	void TickInsertionWatchdog();
	bool ResolveAndBeginInsertion(uint8 TeamId, const FVector& RequestedLocation, AOBPlayerController* FeedbackPlayer);
	void TryCompleteInsertion();
	void CompleteInsertionAfterGracePeriod();
	void AssignPersonalExtractsForTeam(uint8 TeamId, const FVector& InsertionOrigin);
	void UpdateReplicatedInsertionState(uint8 TeamId, EOBInsertionPhase Phase);

	UFUNCTION()
	void HandleInsertionHelicopterPhaseChanged(AOBInsertionHelicopter* Helicopter, EOBInsertionPhase NewPhase);

	UFUNCTION()
	void HandleInsertionPassengerDeployed(AOBInsertionHelicopter* Helicopter, AController* Passenger);

	UFUNCTION()
	void HandleAllInsertionPassengersDeployed(AOBInsertionHelicopter* Helicopter);

	void TickSessionTimer();

	void EndExpedition(EOBExpeditionEndReason Reason);

	// 살아있는(또는 다운 상태로 아직 게임 중인) 플레이어가 하나도 없으면 종료.
	// - Step 4의 사망/탈출 처리에서 상태 변경 후 호출된다.
	void CheckEndConditions();

	//~ 헬퍼 -----------------------------------------------------------------

	// GameState를 Expedition 타입으로 캐스팅해 반환(캐시). 서버 로직 전반에서 사용.
	AOBExpeditionGameState* GetExpeditionGameState() const;
	
	int32 ResolveMaxPlayers() const;
	int32 ResolveSessionLength() const;

	// 현재 맵에 해당하는 MapData(StartPlay에서 1회 결정).
	UOBExpeditionMapData* ResolveMapData();
	
	//~ 개인 탈출구(스폰 기준 원거리 + 주변 좌우, 팀 분산, 세션 랜덤) ----------
	void CollectPersonalExtractPoints();											 // 태그 마커 수집
	void AssignPersonalExtractsFor(AController* C, const FVector& SpawnOrigin);		 // 스폰 확정 후 배정
	TArray<AActor*> SelectPersonalMarkers(const FVector& SpawnOrigin, uint8 TeamId); // 선정 알고리즘
	
	// 진입 URL 옵션(?party=<코드>) 파싱을 위해 오버라이드.
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal) override;
	
	uint8 ResolveTeamForCode(const FString& PartyCode); // 코드→TeamId(같은 코드=같은 팀, 없으면 고유)
	
	// [지도] 레벨 배치 공용 탈출구를 모아 GameState에 싣는다(클라가 액터를 못 찾으므로).
	void CollectPublicExtractsForMap();

	// [지도] 팀에 배정된 개인 탈출구 좌표를 그 팀 전원의 PlayerState에 싣는다.
	void PushPersonalExtractsToTeam(uint8 TeamId);
	
	// [지도] 1초마다 각 플레이어의 PS에 "그 팀의 다른 팀원" 위치를 싣는다.
	void UpdateTeammateMapLocations();

protected:
	// 이 맵의 세션 설정(정원/시간). 맵별 GameMode BP에서 해당 맵의 MapData 에셋을 지정.
	// - 없으면 아래 inline 기본값 사용(안전 폴백).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TObjectPtr<UOBExpeditionMapData> MapData;
	
	// 세션 총 길이(초). 맵별 GameMode BP에서 조정(15~30분). 시작 시 GameState로 복제.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition", meta = (ClampMin = "60"))
	int32 SessionLength = 900; // 기본 15분

	// 이 시간(초) 이하로 남으면 "막바지" 진입. HUD 경고 + (Step 9)호드 어그로.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition", meta = (ClampMin = "0"))
	int32 FinalMinuteThreshold = 60;

	// 마지막 플레이어가 나간 뒤 이 시간(초)만큼 기다렸다가 서버를 리사이클한다.
	// 접속 종료/재접속 노이즈를 흡수할 정도만 주면 된다.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition", meta = (ClampMin = "0"))
	float EmptyServerRecycleDelay = 10.f;

	// 파티코드 없는 접속자를 전원 같은 TeamId로 묶을지.
	// 파티 시스템이 ?party= 코드를 붙여 주므로 기본은 false다.
	// true면 각자 솔로로 들어온 플레이어끼리 한 팀이 되어 아군 판정·관전·팀전멸이 전부 어긋난다.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	bool bUseSharedTeam = false;
	
	// 단일 GameMode로 여러 맵을 쓰기 위한 카탈로그. 현재 레벨과 매칭해 MapData 결정.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TObjectPtr<UOBExpeditionMapCatalog> MapCatalog;
	
	// 존 간 최소 이격(cm). 미달 시 경고 로그. ~1000m(도보 약 3분).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Spawn")
	float MinZoneSeparation = 100000.f;

	/**
	 * Selects the authoritative entry path for this GameMode.
	 * True: spawn and seat players in the insertion helicopter.
	 * False: use the existing SpawnZone/PlayerStart Unreal restart path.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Insertion",
		meta = (DisplayName = "Enable Helicopter Insertion"))
	bool bEnableHelicopterInsertion = true;

	/** Assign BP_OBInsertionHelicopter here. Native class remains a logic-only fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Insertion")
	TSubclassOf<AOBInsertionHelicopter> InsertionHelicopterClass;

	/** Optional extraction-specific visual child. Falls back to InsertionHelicopterClass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Extraction")
	TSubclassOf<AOBInsertionHelicopter> ExtractionHelicopterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Insertion", meta = (ClampMin = "1"))
	float InsertionSelectionTimeout = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Insertion", meta = (ClampMin = "0"))
	float InsertionCompletionGraceSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Insertion|Streaming", meta = (ClampMin = "0.05"))
	float InsertionStreamingPollInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Insertion|Safety", meta = (ClampMin = "5.0"))
	float MaxInsertionRappelSeconds = 45.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expedition|Insertion")
	bool bAllowLateJoinAtResolvedInsertionPoint = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Expedition|Insertion")
	TObjectPtr<UOBLandingZoneScannerComponent> LandingZoneScanner;
	
	// 개인 탈출구로 스폰할 클래스(BP_ExtractionZone_Personal 지정).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Extraction")
	TSubclassOf<AOBExtractionZone> PersonalExtractClass;
	
	// 스폰에서 이 거리(cm) 이상 떨어진 마커만 '먼 후보'로 취급.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Extraction", meta = (ClampMin = "0"))
	float MinSpawnDistance = 60000.f;

	// 먼 후보 상위 N개 중 랜덤으로 중심 선정(세션 다양성).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Extraction", meta = (ClampMin = "1"))
	int32 FarPoolSize = 4;
	
	// 두 탈출구 사이 최소 방향각(도, 스폰 기준). 서로 다른 방향으로 벌려 '진짜 루트 선택' 유도.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Extraction", meta = (ClampMin = "0", ClampMax = "180"))
	float MinDirectionAngleDeg = 60.f;

	// 디버그: 스폰→개인탈출 라인/구체 그리기(서버).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Extraction")
	bool bDrawDebugPersonalExtract = false;

	UPROPERTY(Transient)
	TObjectPtr<UOBExpeditionMapData> ActiveMapData;
	
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Down")
	float BleedOutSeconds = 30.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Down")
	float ReviveHealthPercent = 0.35f;
	
	FTimerHandle TeammateMapTimer;
	
private:
	void CheckTeamWipe(uint8 TeamId);          // 팀에 Alive 0명이면 다운자 전원 사망
	void NormalizePartyLeaderForTeam(uint8 TeamId);

private:	
	FTimerHandle SessionTimerHandle;

	// 이미 종료 처리했는지(중복 EndExpedition 방지).
	bool bExpeditionEnded = false;

	// 잔류 인원 0명이면 같은 맵으로 절대 트래블 → 월드/GameMode 전체 재생성.
	void RecycleIfEmpty();
	FTimerHandle EmptyServerRecycleTimer;

	// 서버 부팅 직후에도 0명이다. 한 번이라도 사람이 들어온 뒤에만 리사이클한다.
	bool bHasEverHadPlayers = false;
	
	bool bZonesCollected = false;   // 존을 한 번만 수집했는지

	// 개인전일 때 다음에 배정할 TeamId(1부터 증가). 0은 "미배정" 예약.
	uint8 NextTeamId = 1;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<AOBExpeditionSpawnZone>> AvailableZones; // 아직 배정 안 된 존
	TMap<uint8, TObjectPtr<AOBExpeditionSpawnZone>> TeamZones; // 팀ID → 배정된 존
	
	bool bPersonalPointsCollected = false;

	// 레벨의 개인 탈출 후보 마커 전체(소비 안 함 — 플레이어별 독립 선정).
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> PersonalExtractPoints;

	// 팀ID → 그 팀의 개인 탈출구들. 팀원 전원이 같은 탈출구를 공유한다.
	UPROPERTY(Transient)
	TMap<uint8, FOBPersonalZoneList> PersonalZones;

	// 팀ID → 이미 소비한 '중심' 마커(팀원끼리 다른 지역 강제).
	// (중첩 컨테이너라 UPROPERTY 불가. 마커는 레벨 액터라 GC 안전)
	TMap<uint8, TSet<TObjectPtr<AActor>>> TeamUsedCenters;
	
	// 컨트롤러 → 진입 시 파싱한 파티 코드(?party=).
	TMap<TObjectPtr<AController>, FString> PartyCodeByController;
	
	// 코드(파티/솔로) → 배정된 TeamId. 같은 코드=같은 팀, 새 코드=중복없는 고유 발급.
	TMap<FString, uint8> PartyTeams;

	TMap<TObjectPtr<AController>, FTimerHandle> BleedOutTimers;

	UPROPERTY(Transient)
	TMap<uint8, TObjectPtr<AOBInsertionHelicopter>> TeamInsertionHelicopters;

	UPROPERTY(Transient)
	TMap<uint8, TObjectPtr<AOBInsertionTargetStreamingProxy>> TeamInsertionTargetStreamingProxies;

	UPROPERTY(Transient)
	TMap<uint8, TObjectPtr<AOBHelicopterRoute>> TeamInsertionRoutes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AOBHelicopterRoute>> AvailableInsertionRoutes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AController>> PendingInsertionControllers;

	TMap<uint8, FOBTeamInsertionState> TeamInsertionRuntimeStates;
	TMap<uint8, FTimerHandle> InsertionSelectionTimers;
	TMap<uint8, FTimerHandle> InsertionStreamingPollTimers;
	TMap<uint8, FOBPendingInsertionTargetRequest> PendingInsertionTargetRequests;
	TMap<uint8, float> LastInsertionWatchdogWarningTimes;
	FTimerHandle InsertionCompletionTimer;
	FTimerHandle InsertionWatchdogTimer;
	bool bInsertionHasStarted = false;
	bool bInsertionHasCompleted = false;
};
