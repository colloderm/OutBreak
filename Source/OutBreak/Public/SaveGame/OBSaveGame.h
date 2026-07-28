// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Loadout/OBLoadoutTypes.h"
#include "OBSaveGame.generated.h"

/**
 * 디스크 영속 데이터. 개인 Loadout(+ 이후 Stash/진행도).
 * - UGameplayStatics::SaveGameToSlot / LoadGameFromSlot로 저장/로드.
 */
UCLASS()
class OUTBREAK_API UOBSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FOBLoadout Loadout;
	
	UPROPERTY(SaveGame)
	int32 Currency = 0;

	// (이후) FOBStash Stash; 등 확장.
};