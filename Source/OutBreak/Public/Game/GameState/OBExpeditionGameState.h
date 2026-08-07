// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameStateBase.h"
#include "Game/Expedition/OBExpeditionTypes.h"
#include "OBExpeditionGameState.generated.h"

class UOBExpeditionMapData;
class AOBPlayerStateBase;
// HUD/위젯이 "페이즈가 바뀌었다"를 이벤트로 구독하기 위한 것.
// - 왜 델리게이트인가: 위젯이 매 프레임 Tick으로 폴링하지 않고, 값이 바뀔 때만 콜백받게 하려고.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOBExpeditionPhaseChanged, EOBExpeditionPhase, NewPhase);

// 남은 시간(초)이 복제되어 갱신될 때 HUD 타이머 텍스트를 다시 그리기 위함.
// - 매초 1회만 변하도록 GameMode가 정수 초 단위로 세팅(불필요한 복제/브로드캐스트 방지).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOBExpeditionTimeChanged, int32, NewTimeRemaining);

UCLASS()
class OUTBREAK_API AOBExpeditionGameState : public AOBGameStateBase
{
	GENERATED_BODY()
	
public:
	AOBExpeditionGameState();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	//~ 서버 전용 세터 ------------------------------
	
	// 페이즈 전환
	void SetPhase(EOBExpeditionPhase NewPhase);
	
	// 남은 시간(초) 세팅
	void SetTimeRemaining(int32 NewSecondes);
	
	// 세션 총 길이(초) 세팅
	void SetSessionLength(int32 InSessionLength);
	
	// "마지막 1분" 진입 플래그 :: HUD 경고 연출.
	void SetFinalMinute(bool bInFinalMinute);
	
	// 루팅 시드(세션마다 1회 결정). 컨테이너가 이걸로 난수 스트림을 만든다.
	void SetLootSeed(int32 InSeed);
	
	//~ 읽기 접근자 (클라/서버 공용) ------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "Expedition")
	EOBExpeditionPhase GetPhase() const { return Phase; };
	
	UFUNCTION(BlueprintPure, Category = "Expedition")
	int32 GetTimeRemaining() const { return TimeRemaining; };
	
	UFUNCTION(BlueprintPure, Category = "Expedition")
	int32 GetSessionLength() const { return SessionLength; };
	
	// 세션 시작 이후 경과 초. 탈출구가 개인/공용 시간창 판정에 사용.
	// - 예: ElapsedSeconds < 600(10분) 이면 개인 탈출구만, 이후 공용 탈출구.
	UFUNCTION(BlueprintPure, Category = "Expedition")
	int32 GetElapsedSeconds() const { return FMath::Max(0, SessionLength - TimeRemaining); }

	UFUNCTION(BlueprintPure, Category = "Expedition")
	bool IsInProgress() const { return Phase == EOBExpeditionPhase::InProgress; }
	
	// 결과창용: 세션 참가자 목록(결과 표시). PlayerArray를 우리 타입으로 캐스팅.
	UFUNCTION(BlueprintPure, Category = "Expedition")
	TArray<AOBPlayerStateBase*> GetExpeditionPlayers() const;
	
	// HUD용 "mm:ss" 문자열.
	UFUNCTION(BlueprintPure, Category = "Expedition")
	FText GetTimeRemainingText() const;
	
	UFUNCTION(BlueprintPure, Category = "Expedition")
	int32 GetLootSeed() const { return LootSeed; }
	
	// [지도] 공용 탈출구 위치. 전원 공유.
	const TArray<FVector_NetQuantize>& GetPublicExtractLocations() const { return PublicExtractLocations; }
	void SetPublicExtractLocations(const TArray<FVector_NetQuantize>& InLocations);
	
	// [지도] 클라가 지도 텍스처/월드 범위를 알아야 한다. 서버 GameMode의 ActiveMapData를 그대로 싣는다.
	const UOBExpeditionMapData* GetMapData() const { return MapData; }
	void SetMapData(UOBExpeditionMapData* InMapData);
	
public:
	//~ HUD 구독용 델리게이트 인스턴스 ----------------------------------------

	// BlueprintAssignable: 블루프린트 위젯에서 Bind Event 가능.
	UPROPERTY(BlueprintAssignable, Category = "Expedition")
	FOBExpeditionPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Expedition")
	FOBExpeditionTimeChanged OnTimeRemainingChanged;
	
protected:
	//~ OnRep 콜백 (클라에서만 자동 호출) --------------------------------------

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION()
	void OnRep_TimeRemaining();
	
protected:
	// [복제] 현재 페이즈. ReplicatedUsing: 클라에서 값 수신 시 OnRep_Phase 호출→델리게이트 브로드캐스트.
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "Expedition")
	EOBExpeditionPhase Phase = EOBExpeditionPhase::Warmup;
	
	// [복제] 남은 시간(초). 클라 수신 시 OnRep_TimeRemaining→HUD 갱신.
	UPROPERTY(ReplicatedUsing = OnRep_TimeRemaining, BlueprintReadOnly, Category = "Expedition")
	int32 TimeRemaining = 0;

	// [복제] 세션 총 길이(초). 시작 시 1회 세팅되므로 OnRep 콜백 불필요(단순 복제).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Expedition")
	int32 SessionLength = 0;

	// [복제] 막바지 1분 여부. 현재는 HUD 연출용. (Step 9에서 호드 어그로 트리거로 확장)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Expedition")
	bool bFinalMinute = false;
	
	// [복제] 세션 루팅 시드. 시작 시 1회 세팅되므로 OnRep 불필요.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Expedition")
	int32 LootSeed = 0;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Expedition")
	TArray<FVector_NetQuantize> PublicExtractLocations;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Expedition")
	TObjectPtr<UOBExpeditionMapData> MapData;
	
};
