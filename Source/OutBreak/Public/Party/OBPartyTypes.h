// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBPartyTypes.generated.h"

USTRUCT(BlueprintType)
struct FOBPartyMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party") FString PlayerId;
	UPROPERTY(BlueprintReadOnly, Category = "Party") FText   DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Party") bool    bIsLeader = false; // 왕관
	UPROPERTY(BlueprintReadOnly, Category = "Party") bool    bOnline   = true;
};