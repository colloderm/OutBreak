// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/Data/InventoryData.h"
#include "Item/Data/OBItemTypes.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBLoadoutTypes.generated.h"

/**
 * 개인 장비 구성 + 창고.
 * - 무기든 탄약이든 전부 아이템 태그로만 지목한다. 실제 무기 클래스는 ItemDefinition.WeaponClass가 안다.
 * - 디스크에 태그(FName)로 직렬화되므로 에셋을 옮겨도 안전하다(예전 TSoftClassPtr은 경로가 바뀌면 깨졌다).
 */
USTRUCT(BlueprintType)
struct FOBLoadout
{
	GENERATED_BODY()

	// Version 2 stores stateful equipment as concrete item instances. The
	// subsystem migrates older tag/count-only saves when they are loaded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	int32 DataVersion = 2;

	// 슬롯 → 아이템 태그. 슬롯당 1개(덮어쓰기).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TMap<EOBWeaponSlot, FGameplayTag> SlotWeapons;

	// Authoritative state for equipped weapons (durability, magazine,
	// attachments and raid provenance). SlotWeapons remains as a compatibility
	// and UI lookup index.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TMap<EOBWeaponSlot, FInventoryData> SlotWeaponInstances;

	// 창고. 무기(Count=1) / 탄약 / 소모품 / 귀중품이 모두 여기로 들어온다. 사망에도 유지.
	// 칸 개념이 없어서 MaxStack을 적용하지 않고 태그당 한 항목으로 합친다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TArray<FOBItemStack> StashItems;

	// Weapons, equipment and attachments are never collapsed into tag counts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TArray<FInventoryData> StashItemInstances;
	
	// 이번 탐사에 들고 들어갈 물건. 고르는 순간 창고에서 빠져나온다.
	// 죽으면 그대로 잃고, 탈출하면 가방째 정산되어 창고로 돌아온다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TArray<FOBItemStack> CarryItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TArray<FInventoryData> CarryItemInstances;
};
