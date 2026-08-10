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
	UE_LOG(LogTemp, Log, TEXT("[Matchmaking] 시작 map=%s level=%s queue=%s"),
		*InMap->DisplayName.ToString(), *InMap->Level.ToString(),
		bPartyQueue ? TEXT("Party") : TEXT("Solo"));
	
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
	
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		SetState(EOBMatchmakingState::Idle);
		return;
	}
	
	// 데디 접속은 Data Asset에서 명시적으로 켠 경우에만 Level보다 우선한다.
	// 주소 문자열이 예전 테스트 값으로 남아 있어도 Level 이동을 가로채지 않는다.
	if (SelectedMap->bUseTestServerAddress && !SelectedMap->TestServerAddress.IsEmpty())
	{
		FString Address = SelectedMap->TestServerAddress;
		if (!PartyCode.IsEmpty())
		{
			Address += FString::Printf(TEXT("?party=%s"), *PartyCode);
		}

		UE_LOG(LogTemp, Log, TEXT("[Matchmaking] 데디 접속 → %s"), *Address);
		if (APlayerController* PC = GI->GetFirstLocalPlayerController())
		{
			PC->ClientTravel(Address, TRAVEL_Absolute);
		}
	}
	else
	{
		if (SelectedMap->bUseTestServerAddress)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Matchmaking] 테스트 서버 사용이 켜졌지만 주소가 비어 있어 Level로 이동합니다."));
		}
		UE_LOG(LogTemp, Log, TEXT("[Matchmaking] 로컬 이동 → %s"), *SelectedMap->Level.ToString());
		UGameplayStatics::OpenLevelBySoftObjectPtr(World, SelectedMap->Level);
	}
	
	SetState(EOBMatchmakingState::Idle);
}

void UOBMatchmakingSubsystem::SetState(EOBMatchmakingState NewState)
{
	if (State == NewState) return;
	State = NewState;
	OnStateChanged.Broadcast(State);
}
