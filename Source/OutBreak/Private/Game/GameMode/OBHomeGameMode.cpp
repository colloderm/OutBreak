#include "Game/GameMode/OBHomeGameMode.h"

AOBHomeGameMode::AOBHomeGameMode()
{
	bUseSeamlessTravel = false;
}

void AOBHomeGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	
	// Home은 개인 로컬 레벨 ㅡ 이미 1명이면 추가(원격) 접속 거부
	if (GetNumPlayers() >= 1)
	{
		ErrorMessage = TEXT("Home is single-player only");
	}
}
