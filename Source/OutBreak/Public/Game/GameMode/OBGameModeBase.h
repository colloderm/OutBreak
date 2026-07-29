// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OBGameModeBase.generated.h"

UCLASS()
class OUTBREAK_API AOBGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	AOBGameModeBase();
	
	/*
	왜 호출되는가? - 사망한 플레이어를 일정 시간 후 리스폰시키기 위해.
	언제 호출되는가? - Character.HandleDeath에서.
	서버/클라? - 서버 전용(GameMode는 서버에만 존재).
	*/
	virtual void RequestRespawn(AController* Controller, APawn* DeadPawn);
	
	// 치명타 시 '다운'으로 갈지(팀 생존자 존재). 베이스=항상 false(즉시 사망).
	virtual bool ShouldEnterDownedState(AController* C) const { return false; }
	// 다운 처리(블리드아웃 시작). 베이스=무동작.
	virtual void NotifyPlayerDowned(AController* C) {}
protected:
	// 사망 후 리스폰까지 지연 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Respawn")
	float RespawnDelay = 3.f;
};
