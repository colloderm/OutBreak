// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "InventorySystemSetting.generated.h"

class UInventorySlot;
class UInventoryWindow;

/**
 * Project-wide inventory defaults. Runtime inventory contents and equipped
 * backpack capacity remain owned by UPlayerInventoryComponent/DataAssets.
 */
UCLASS(Config = Game, DefaultConfig)
class OUTBREAK_API UInventorySystemSetting : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, Category = "Item Data")
	TSoftObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(Config, EditAnywhere, Category = "Widget")
	TSoftClassPtr<UInventoryWindow> InventoryWindowWidgetClass;

	UPROPERTY(Config, EditAnywhere, Category = "Widget")
	TSoftClassPtr<UInventorySlot> InventorySlotWidgetClass;

	UPROPERTY(Config, EditAnywhere, Category = "Widget",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 InventoryGridColumns = 5;

	UPROPERTY(Config, EditAnywhere, Category = "Widget")
	int32 InventoryWidgetZOrder = 20;

	// Used only before a backpack DataAsset is equipped. An equipped backpack's
	// UOBEquipmentData::BackpackSlotCount always takes precedence.
	UPROPERTY(Config, EditAnywhere, Category = "Capacity",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 FallbackBackpackSlotCount = 20;

	UPROPERTY(Config, EditAnywhere, Category = "Capacity",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 DefaultContainerSlotCount = 20;

	UPROPERTY(Config, EditAnywhere, Category = "Capacity",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 DefaultQuickSlotCount = 6;

	UPROPERTY(Config, EditAnywhere, Category = "World Drop",
		meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float DroppedItemForwardDistance = 100.f;
	
#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT(
			"OutBreakInventorySettings",
			"SectionText",
			"Inventory System");
	}
#endif
	
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
};
