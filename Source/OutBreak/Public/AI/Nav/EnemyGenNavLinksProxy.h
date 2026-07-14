// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseGeneratedNavLinksProxy.h"
#include "EnemyGenNavLinksProxy.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UEnemyGenNavLinksProxy : public UBaseGeneratedNavLinksProxy
{
	GENERATED_BODY()
	
	virtual bool OnLinkMoveStarted(class UObject* PathComp, const FVector& DestPoint) override;
};
