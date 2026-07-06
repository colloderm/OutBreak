// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBExpeditionMapCatalog.generated.h"

class UOBExpeditionMapData;

UCLASS()
class OUTBREAK_API UOBExpeditionMapCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	TArray<TObjectPtr<UOBExpeditionMapData>> AvailableMaps;
};
