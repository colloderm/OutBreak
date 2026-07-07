// Fill out your copyright notice in the Description page of Project Settings.

#include "Matchmaking/OBMatchmakingSubsystem.h"

#include "Game/Expedition/OBExpeditionMapData.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UOBMatchmakingSubsystem::StartMatchmaking(UOBExpeditionMapData* InMap, bool bInPartyQueue)
{
	if (State != EOBMatchmakingState::Idle || !InMap) return;
	
	SelectedMap = InMap;
	bPartyQueue = bInPartyQueue;
	ElapsedSeconds = 0;
	
	// (스텁) 실제로는 개인/파티 큐로 세션 검색을 시작해야함.
	UE_LOG(LogTemp, Log, TEXT("[Matchmaking] 시작 map=%s queue=%s"),
		*InMap->DisplayName.ToString(), bPartyQueue ? TEXT("Party") : TEXT("Solo"));
	
	SetState(EOBMatchmakingState::Searching);
	
	if (UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		W->GetTimerManager().SetTimer(SearchTimer, this, &UOBMatchmakingSubsystem::TickSearch, 1.f, true);
		OnTick.Broadcast(GetRemainingSeconds());
	}
}

void UOBMatchmakingSubsystem::CancelMatchmaking()
{
	if (State != EOBMatchmakingState::Searching) return;
	
	if (UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		W->GetTimerManager().ClearTimer(SearchTimer);
	
	UE_LOG(LogTemp, Log, TEXT("[Matchmaking] 취소"));
	SetState(EOBMatchmakingState::Idle);
}

void UOBMatchmakingSubsystem::TickSearch()
{
	++ElapsedSeconds;
	OnTick.Broadcast(GetRemainingSeconds());
	
	// (스텁) 실제로는 "정원 충족?"을 검사해 조기 시작 가능
	if (ElapsedSeconds >= SearchMaxSeconds)
		BeginStart();
}

void UOBMatchmakingSubsystem::BeginStart()
{
	if (UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		W->GetTimerManager().ClearTimer(SearchTimer);
	
	SetState(EOBMatchmakingState::Starting);
	StartSession();
}

void UOBMatchmakingSubsystem::StartSession()
{
	if (!SelectedMap || SelectedMap->Level.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Matchmaking] SelectedMap/Level 없음 -> 취소"));
		SetState(EOBMatchmakingState::Idle);
		return;
	}
	
	// TODO(M7): 데디 세션 검색/생성 + join(ClientTravel). 지금은 로컬 이동 스텁.
	UE_LOG(LogTemp, Log, TEXT("[Matchmaking] 세션 시작(스텁) → %s"), *SelectedMap->DisplayName.ToString());

	if (UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		UGameplayStatics::OpenLevelBySoftObjectPtr(W, SelectedMap->Level);

	// 다음 회차를 위해 상태 초기화(레벨 전환 중이라 UI는 곧 파괴됨).
	SetState(EOBMatchmakingState::Idle);
}

void UOBMatchmakingSubsystem::SetState(EOBMatchmakingState NewState)
{
	if (State == NewState) return;
	State = NewState;
	OnStateChanged.Broadcast(State);
}
