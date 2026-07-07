// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OBMatchmakingSubsystem.generated.h"

class UOBExpeditionMapData;

UENUM(BlueprintType)
enum class EOBMatchmakingState : uint8
{
	Idle,       // 대기(미매칭)
	Searching,  // 매칭중(세션 채우는 중)
	Starting    // 세션 시작/이동
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOBMatchStateChanged, EOBMatchmakingState);
DECLARE_MULTICAST_DELEGATE_OneParam(FOBMatchTick, int32 /*RemainingSeconds*/);

/**
 * 매칭 상태머신(GameInstance 수명 → UI 닫아도 백그라운드 유지).
 * - 스텁: 최대 대기(기본 5분) 경과 시 선택 맵으로 로컬 이동.
 * - 데디 연결(M7) 시 StartSession을 실제 세션 검색/생성/join으로 교체.
 * - 개인/파티 큐는 지금 bPartyQueue로 기록만(실 분리는 백엔드 M7).
 */
UCLASS()
class OUTBREAK_API UOBMatchmakingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	void StartMatchmaking(UOBExpeditionMapData* InMap, bool bInPartyQueue); // Idle에서만
	void CancelMatchmaking();                                               // Searching에서만

	EOBMatchmakingState GetState() const { return State; }
	int32 GetRemainingSeconds() const { return FMath::Max(0, SearchMaxSeconds - ElapsedSeconds); }

	FOBMatchStateChanged OnStateChanged;
	FOBMatchTick OnTick;

protected:
	void SetState(EOBMatchmakingState NewState);
	void TickSearch();     // 1초마다
	void BeginStart();     // 대기완료/정원충족 → 시작
	void StartSession();   // 스텁: 로컬 OpenLevel

private:
	UPROPERTY()
	TObjectPtr<UOBExpeditionMapData> SelectedMap;

	EOBMatchmakingState State = EOBMatchmakingState::Idle;
	bool bPartyQueue = false;
	int32 ElapsedSeconds = 0;

	// 최대 매칭 대기(초). 지나면 모인 인원으로 시작. ★테스트 시 10 등으로 낮춰 확인.
	int32 SearchMaxSeconds = 10; //300

	FTimerHandle SearchTimer;
	
	// 알파 기본 접속 주소(MapData.TestServerAddress가 비었을 때).
	FString DefaultServerAddress = TEXT("127.0.0.1:7777");
};
