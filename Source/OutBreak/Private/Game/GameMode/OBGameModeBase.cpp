// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameMode/OBGameModeBase.h"

#include "Character/OBCharacterBase.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Game/GameState/OBGameStateBase.h"
#include "UI/HUD/OBHUD.h"

AOBGameModeBase::AOBGameModeBase()
{
	DefaultPawnClass = AOBCharacterBase::StaticClass();
	PlayerControllerClass = AOBPlayerController::StaticClass();
	PlayerStateClass = AOBPlayerStateBase::StaticClass();
	GameStateClass = AOBGameStateBase::StaticClass();
	HUDClass = AOBHUD::StaticClass();
	
	// 로비→게임 매끄러운 전환 + PlayerState(선택값) 보존.
	bUseSeamlessTravel = true;
}

void AOBGameModeBase::RequestRespawn(AController* Controller, APawn* DeadPawn)
{
	if (!Controller) return;
	
	// 약참조로 캡처(타이머 동안 파괴 가능성 대비)
	TWeakObjectPtr<AController> WeakController = Controller;
	TWeakObjectPtr<APawn> WeakDeadPawn = DeadPawn;
	
	FTimerHandle RespawnTimer;
	FTimerDelegate RespawnDel = FTimerDelegate::CreateWeakLambda(this,
		[this, WeakController, WeakDeadPawn]()
		{
			// 죽은 폰 제거(컨트롤러 자동 UnPossess)
			if (WeakDeadPawn.IsValid())
			{
				WeakDeadPawn->Destroy();
			}
			
			// 새 폰 스폰 + 빙의(PlayerStart 사용). PossessedBy가 체력/무기/태그를 재 초기화
			if (WeakController.IsValid())
			{
				RestartPlayer(WeakController.Get());
			}
		});
	
	GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDel, RespawnDelay, false);
}
