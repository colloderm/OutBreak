// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameModeBase.h"
#include "OBExpeditionGameMode.generated.h"

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

UCLASS()
class OUTBREAK_API AOBExpeditionGameMode : public AOBGameModeBase
{
	GENERATED_BODY()
	
public:
	AOBExpeditionGameMode();
	
	// [무리스폰] 베이스는 지연 후 RestartPlayer. Expedition은 리스폰 금지 → 여기서 막는다.
	// - 실제 "사망 상태 마킹(Dead) + 종료판정"은 Step 4에서 이 안에 채운다.
	virtual void RequestRespawn(AController* Controller, APawn* DeadPawn) override;
	
	// 팀별 존 배정에 따라 시작지점을 고른다.
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	// 존이면 반경 내 랜덤 위치로 스폰(파티원 산개).
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;
	
	// 플레이어 접속 종료. 마지막 생존자가 나갔을 수 있어 종료 조건을 재평가.
	virtual void Logout(AController* Exiting) override;
	
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
	
	// 존 간 최소 이격(cm). 미달 시 경고 로그. ~1000m(도보 약 3분).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition|Spawn")
	float MinZoneSeparation = 100000.f;

private:	
	FTimerHandle SessionTimerHandle;

	// 이미 종료 처리했는지(중복 EndExpedition 방지).
	bool bExpeditionEnded = false;

	// 개인전일 때 다음에 배정할 TeamId(1부터 증가). 0은 "미배정" 예약.
	uint8 NextTeamId = 1;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<AOBExpeditionSpawnZone>> AvailableZones; // 아직 배정 안 된 존
	TMap<uint8, TObjectPtr<AOBExpeditionSpawnZone>> TeamZones; // 팀ID → 배정된 존
};
