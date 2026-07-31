// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InventoryData.generated.h"

/**
 * 
 */


USTRUCT(BlueprintType)
struct OUTBREAK_API FInventoryQueryResult
{
	GENERATED_BODY()
	
	bool HasItem = false;
	
	TArray<int> Indices;
	
	int TotalStack = -1;
};


USTRUCT(Blueprintable)
struct OUTBREAK_API FItemMetaData : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> ItemTexture;
	
	// 현재 Complier Error를 피하기 위해 Actor Class 사용 추후 전용 클래스로 변경. 
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> WorldItemClass;
	
	UPROPERTY(EditAnywhere)
	
	
	
	
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FWorldItemData
{
	GENERATED_BODY()
	
	

	UPROPERTY(BlueprintReadWrite)
	int ItemStack = -1;
	
	
};


UENUM(Blueprintable)
enum class EItemType : uint8
{
	PrimaryWeapon,
	SeconderyWeapon,
	MeleeWeapon,
	Helmet,
	Armor,
	Glave,
	Pants,
	Shoes,
	Consumable,
	Ammo,
	
};




USTRUCT(BlueprintType)
struct OUTBREAK_API FInventoryData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	FName ItemName;
	
	UPROPERTY(BlueprintReadWrite)
	EItemType ItemType;
	
	UPROPERTY(BlueprintReadWrite)
	int ItemStack = -1;
	
	
};
