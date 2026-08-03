// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Data/OBEquipmentTypes.h"
#include "UObject/Object.h"
#include "InventoryData.generated.h"

class UOBItemDefinition;

/**
 * 
 */


USTRUCT(BlueprintType)
struct OUTBREAK_API FInventoryQueryResult
{
	GENERATED_BODY()
	
	FName QueryItemName;
	
	bool HasItem = false;
	
	TArray<int32> BackpackIndices;
	TArray<int32> ContainerIndices;
	
	int32 TotalStack = 0;
	
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
	int32 MaxItemStack = 1;
	
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FWorldItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UOBItemDefinition> ItemDefinition;
	
	// Legacy data-table identifier. Used only when ItemDefinition is not assigned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ItemStack = 1;
	
	
};


UENUM(BlueprintType)
enum class EItemType : uint8
{
	// Primary/Secondary/Melee is resolved from UOBWeaponData::WeaponSlot.
	// Inventory only needs to know that the item is a weapon.
	Weapon,
	// Head/Chest/Hands/Legs/Feet is resolved from UOBEquipmentData.
	Equipment,
	Consumable,
	Ammo,
	
};




USTRUCT(BlueprintType)
struct OUTBREAK_API FInventoryData
{
	GENERATED_BODY()

	// Existing project item asset. WeaponClass is resolved through this asset,
	// and the weapon slot is resolved from the class default object's UOBWeaponData.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UOBItemDefinition> ItemDefinition;
	
	UPROPERTY(BlueprintReadWrite)
	FName ItemName = NAME_None;
	
	UPROPERTY(BlueprintReadWrite)
	EItemType ItemType = EItemType::Consumable;
	
	UPROPERTY(BlueprintReadWrite)
	int32 ItemStack = 0;

	// Stable identity for equipment state. Array indices can change when slots move.
	UPROPERTY(BlueprintReadOnly)
	FGuid InstanceId;

	// Per-item runtime state. -1 means that the weapon has not been equipped yet.
	UPROPERTY(BlueprintReadOnly)
	int32 MagazineAmmo = -1;
};

// UI drag/drop locations. SlotIndex is only a hint; mutations re-resolve by InstanceId.
UENUM(BlueprintType)
enum class EInventoryItemLocation : uint8
{
	None,
	Backpack,
	Container,
	Equipment,
	QuickSlot
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FInventoryItemHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FGuid InstanceId;

	UPROPERTY(BlueprintReadWrite)
	EInventoryItemLocation Location = EInventoryItemLocation::None;

	UPROPERTY(BlueprintReadWrite)
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite)
	EOBEquipmentSlot EquipmentSlot = EOBEquipmentSlot::None;

	// Quick slots reference an item type rather than a concrete inventory instance.
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UOBItemDefinition> ItemDefinition;

	bool HasItem() const
	{
		return InstanceId.IsValid() ||
			(Location == EInventoryItemLocation::QuickSlot && ItemDefinition);
	}
};

// Equipment slots own their item instance. This avoids a backpack recursively
// referencing itself through the storage array that the backpack provides.
USTRUCT(BlueprintType)
struct OUTBREAK_API FEquipmentSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EOBEquipmentSlot Slot = EOBEquipmentSlot::None;

	UPROPERTY(BlueprintReadOnly)
	FInventoryData Item;

	bool IsEmpty() const { return Item.ItemStack <= 0 || !Item.InstanceId.IsValid(); }
};


USTRUCT(BlueprintType)
struct OUTBREAK_API FQuickSlotData
{
	GENERATED_BODY()

	// A quick slot intentionally stores only the static item type. It keeps
	// working after stacks move, merge, run out, or are acquired again.
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UOBItemDefinition> ItemDefinition;

	bool IsAssigned() const { return ItemDefinition != nullptr; }
};
