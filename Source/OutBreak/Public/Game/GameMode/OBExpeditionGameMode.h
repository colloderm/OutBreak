// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameModeBase.h"
#include "OBExpeditionGameMode.generated.h"

class AOBPlayerStateBase;
class AOBExtractionZone;
class UOBExpeditionMapCatalog;
class AOBExpeditionSpawnZone;
class UOBExpeditionMapData;
class AOBExpeditionGameState;

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
	
	// 팀별 존 배정에 따라 시작지점을 고른다.
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	// 존이면 반경 내 랜덤 위치로 스폰(파티원 산개).
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;
	
	// 플레이어 접속 종료. 마지막 생존자가 나갔을 수 있어 종료 조건을 재평가.
	virtual void Logout(AController* Exiting) override;
	
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
	virtual void StartPlay() override;

	// 진입 플레이어 공통 초기화(신규 접속 + 심리스 트래블 양쪽에서 호출됨).
	// - TeamId 부여 + ExpeditionStatus=Alive 리셋.
	virtual void GenericPlayerInitialization(AController* C) override;

	//~ 세션 진행 ------------------------------------------------------------
	
	void CollectSpawnZones(); // 레벨의 존 수집 + 셔플
	
	AOBExpeditionSpawnZone* GetOrAssignZoneForTeam(uint8 InTeamId);
	
	void ValidateZoneSeparation() const; // 3~5분 이격 검증
	
	void StartExpedition();

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

	// 파티(공유팀) 여부. true=전원 같은 TeamId(협동), false=개인전(고유 TeamId).
	// - 알파: 3인 협동이므로 기본 true. 로비에서 팀 편성 붙이면 이 값 대신 사용.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	bool bUseSharedTeam = true;
	
	// 단일 GameMode로 여러 맵을 쓰기 위한 카탈로그. 현재 레벨과 매칭해 MapData 결정.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TObjectPtr<UOBExpeditionMapCatalog> MapCatalog;
	
	// 존 간 최소 이격(cm). 미달 시 경고 로그. ~1000m(도보 약 3분).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Spawn")
	float MinZoneSeparation = 100000.f;
	
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
	
private:
	void CheckTeamWipe(uint8 TeamId);          // 팀에 Alive 0명이면 다운자 전원 사망

private:	
	FTimerHandle SessionTimerHandle;

	// 이미 종료 처리했는지(중복 EndExpedition 방지).
	bool bExpeditionEnded = false;
	
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

	// 컨트롤러 → 그 플레이어의 개인 탈출구들(정리용).
	UPROPERTY(Transient)
	TMap<TObjectPtr<AController>, FOBPersonalZoneList> PersonalZones;

	// 팀ID → 이미 소비한 '중심' 마커(팀원끼리 다른 지역 강제).
	// (중첩 컨테이너라 UPROPERTY 불가. 마커는 레벨 액터라 GC 안전)
	TMap<uint8, TSet<TObjectPtr<AActor>>> TeamUsedCenters;
	
	// 컨트롤러 → 진입 시 파싱한 파티 코드(?party=).
	TMap<TObjectPtr<AController>, FString> PartyCodeByController;
	
	// 코드(파티/솔로) → 배정된 TeamId. 같은 코드=같은 팀, 새 코드=중복없는 고유 발급.
	TMap<FString, uint8> PartyTeams;

	TMap<TObjectPtr<AController>, FTimerHandle> BleedOutTimers;
};
