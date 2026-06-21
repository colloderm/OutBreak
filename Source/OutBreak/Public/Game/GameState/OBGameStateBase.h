// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "OBGameStateBase.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API AOBGameStateBase : public AGameStateBase
{
	GENERATED_BODY()
public:
	AOBGameStateBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
