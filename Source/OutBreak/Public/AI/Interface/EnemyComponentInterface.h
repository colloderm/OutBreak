// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AI/Data/EnemyAsset.h"
#include "EnemyComponentInterface.generated.h"

/**
 * 
 */
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class OUTBREAK_API UEnemyComponentInterface : public UInterface
{
	GENERATED_BODY()
};


class OUTBREAK_API IEnemyComponentInterface
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	virtual void InitializeEnemyComponent(UEnemyAsset* inAsset) = 0;
	
	UFUNCTION()
	virtual void EnsureAsset(UObject* inObject,UEnemyAsset* inAsset)
	{
		if (!ensureAlwaysMsgf(IsValid(inAsset),
		                      TEXT("IEnemyComponentInterface::%s: %s is not Initialization Enemy Component."),
		                      TEXT(__FUNCTION__),
		                      *inObject->GetClass()->GetName()))
		
		{
			return;
		}
	}
};
