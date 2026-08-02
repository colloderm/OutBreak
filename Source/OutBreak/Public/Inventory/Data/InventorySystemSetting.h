// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "InventorySystemSetting.generated.h"

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig)
class OUTBREAK_API UInventorySystemSetting : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, Category = "Item Data")
	TSoftObjectPtr<UDataTable> ItemDataTable;
	
	UPROPERTY(Config, EditAnywhere, Category = "Slot Widget Class")
	TSubclassOf<UUserWidget> SlotWidget;
	
#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT(
			"OutBreakItemSettings",
			"SectionText",
			"ItemData");
	}
#endif
	
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
};
