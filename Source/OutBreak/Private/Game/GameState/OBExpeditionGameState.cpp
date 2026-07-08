// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameState/OBExpeditionGameState.h"

#include "Net/UnrealNetwork.h"

AOBExpeditionGameState::AOBExpeditionGameState()
{
}

void AOBExpeditionGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// 4개 필드 모두 전 클라에 복제. (조건 없는 기본 복제)
	DOREPLIFETIME(AOBExpeditionGameState, Phase);
	DOREPLIFETIME(AOBExpeditionGameState, TimeRemaining);
	DOREPLIFETIME(AOBExpeditionGameState, SessionLength);
	DOREPLIFETIME(AOBExpeditionGameState, bFinalMinute);
}

//~ 서버 세터 ---------------------------------------------------------------

void AOBExpeditionGameState::SetPhase(EOBExpeditionPhase NewPhase)
{
	if (!HasAuthority() || Phase == NewPhase) return;
	
	Phase = NewPhase;
	
	OnPhaseChanged.Broadcast(Phase);
}

void AOBExpeditionGameState::SetTimeRemaining(int32 NewSecondes)
{
	if (!HasAuthority()) return;
	
	const int32 Clamped = FMath::Max(0, NewSecondes);
	if (TimeRemaining == Clamped) return;
	
	TimeRemaining = Clamped;
	OnTimeRemainingChanged.Broadcast(TimeRemaining); // 호스트 로컬 HUD 갱신용.
}

void AOBExpeditionGameState::SetSessionLength(int32 InSessionLength)
{
	if (!HasAuthority()) return;
	
	SessionLength = FMath::Max(0, InSessionLength);
}

void AOBExpeditionGameState::SetFinalMinute(bool bInFinalMinute)
{
	if (!HasAuthority() || bFinalMinute == bInFinalMinute) return;
	
	bFinalMinute = bInFinalMinute;
	// 현재는 별도 델리게이트 없이 bFinalMinute 값을 위젯이 폴링/바인딩으로 읽음.
	// Step 9(호드 어그로)에서 서버측 트리거 로직을 GameMode에 추가 예정.
}

void AOBExpeditionGameState::OnRep_Phase()
{
	// 클라가 서버로부터 Phase 값을 새로 받았을 때 호출됨 -> HUD에 알림
	OnPhaseChanged.Broadcast(Phase);
}

void AOBExpeditionGameState::OnRep_TimeRemaining()
{
	// 클라 타이머 텍스트 갱신
	OnTimeRemainingChanged.Broadcast(TimeRemaining);
}

