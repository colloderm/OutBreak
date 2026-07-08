// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameModeBase.h"
#include "OBLobbyGameMode.generated.h"



UCLASS()
class OUTBREAK_API AOBLobbyGameMode : public AOBGameModeBase
{
	GENERATED_BODY()
	
public:
	// 호스트 요청 시 준비 검증 후 게임맵으로 Seamless Travel.
	void TryStartGame(AController* Requester);

protected:
	bool AreAllPlayersReady() const;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TSoftObjectPtr<UWorld> GameLevel;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	bool bRequireAllReady = true;
};
