// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameState/OBGameStateBase.h"

#include "Net/UnrealNetwork.h"

AOBGameStateBase::AOBGameStateBase()
{
	// 현재 전용 설정 없음. 공유 상태 필드가 생기면 여기서 기본값을 초기화한다.
}

void AOBGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// 향후 공유 필드 추가 예시(웨이브 등):
	// DOREPLIFETIME(AOBGameStateBase, CurrentWave);
}
