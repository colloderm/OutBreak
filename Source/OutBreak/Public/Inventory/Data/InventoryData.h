// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Data/OBEquipmentTypes.h"
#include "GameplayTagContainer.h"
#include "Item/OBItemRegistry.h"
#include "UObject/Object.h"
#include "InventoryData.generated.h"

/**
 * 
 */


// 아이템 정적 스펙 표는 DT_Items(FOBItemDefinitionRow)로 통합됐다.
// 아이콘=Icon, 최대 스택=MaxStack, 월드 드랍 액터=WorldItemClass 가 각각 대체한다.

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBInstalledAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Durability = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Meta = (ClampMin = "0"))
	int32 QualityTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bFoundInRaid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGuid RaidAcquisitionId;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FInventoryData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGameplayTag ItemTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 ItemStack = 0;

	// Stable identity for equipment state. Array indices can change when slots move.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGuid InstanceId;

	// Per-item runtime state. -1 means that the weapon has not been equipped yet.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 MagazineAmmo = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Durability = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Meta = (ClampMin = "0"))
	int32 QualityTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bFoundInRaid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGuid RaidAcquisitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FOBInstalledAttachment> Attachments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 DataVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Revision = 0;
	
	// 스펙은 표에서 그때그때 조회한다. 포인터를 들고 있지 않는다.
	const FOBItemDefinitionRow* GetDefinition() const
	{
		return UOBItemRegistry::FindItem(ItemTag);
	}
};

// UI drag/drop locations. SlotIndex is only a hint; mutations re-resolve by InstanceId.
UENUM(BlueprintType)
enum class EInventoryItemLocation : uint8
{
	None = 0,
	Backpack = 1,
	// Player-owned secondary storage. World loot containers remain separate
	// actors and never use this location.
	Container = 2,
	Equipment = 3,
	QuickSlot = 4
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

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag ItemTag;

	bool HasItem() const
	{
		return InstanceId.IsValid() ||
			(Location == EInventoryItemLocation::QuickSlot && ItemTag.IsValid());
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

	// A quick slot intentionally stores only the static item type. It survives
	// stack moves and merges, but is cleared when the final item is consumed.
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag ItemTag;

	bool IsAssigned() const { return ItemTag.IsValid(); }
};
