// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/GameMode/OBGameModeBase.h"

#include "Character/OBCharacterBase.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Game/GameState/OBGameStateBase.h"

AOBGameModeBase::AOBGameModeBase()
{
	DefaultPawnClass = AOBCharacterBase::StaticClass();
	PlayerControllerClass = AOBPlayerController::StaticClass();
	PlayerStateClass = AOBPlayerStateBase::StaticClass();
	GameStateClass = AOBGameStateBase::StaticClass();
	
}
