// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameMode/OBLobbyGameMode.h"

#include "Player/State/OBPlayerStateBase.h"
#include "GameFramework/GameStateBase.h"

void AOBLobbyGameMode::TryStartGame(AController* Requester)
{
	if (bRequireAllReady && !AreAllPlayersReady()) return;
	if (GameLevel.IsNull()) return;

	const FString Path = GameLevel.ToSoftObjectPath().GetLongPackageName();
	GetWorld()->ServerTravel(Path);   // bUseSeamlessTravel=true → 매끄러운 전환 + PlayerState 보존
}

bool AOBLobbyGameMode::AreAllPlayersReady() const
{
	if (!GameState) return false;
	if (GameState->PlayerArray.Num() == 0) return false;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS))
			if (!OBPS->IsReady()) return false;
	}
	return true;
}
