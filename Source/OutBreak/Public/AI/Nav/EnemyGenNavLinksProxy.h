// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseGeneratedNavLinksProxy.h"
#include "AI/Struct/EnemyTraversalData.h"
#include "EnemyGenNavLinksProxy.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UEnemyGenNavLinksProxy : public UBaseGeneratedNavLinksProxy
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETraversalLinkType LinkTraversalType = ETraversalLinkType::None;

public:
	virtual bool OnLinkMoveStarted(class UObject* PathComp, const FVector& DestPoint) override;
	
	bool GetActiveLinkEndpoints(UObject* PathComp, const FVector& DestPoint, FVector& OutStart, FVector& OutEnd) const;
	
};
