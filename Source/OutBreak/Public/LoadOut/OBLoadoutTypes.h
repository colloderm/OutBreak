// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBLoadoutTypes.generated.h"

class AOBWeaponBase;

/**
 * 개인 장비 구성(슬롯별 선택 무기).
 * - 디스크 저장을 고려해 TSoftClassPtr 사용(에셋 경로로 직렬화 → 미로드 클래스도 안전).
 * - 런타임에선 GetClasses()로 로드해 서버로 push.
 */
USTRUCT(BlueprintType)
struct FOBLoadout
{
	GENERATED_BODY()

	// 슬롯 → 무기 클래스. 슬롯당 1개(덮어쓰기).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TMap<EOBWeaponSlot, TSoftClassPtr<AOBWeaponBase>> SlotWeapons;
	
	// 보유(미장착) 무기 풀 = 창고. 상점 구매로 추가, 사망에도 유지.
	// ponytail: Set-of-class(클래스당 1개), 수량 필요하면 TMap<class,count>로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Loadout")
	TSet<TSoftClassPtr<AOBWeaponBase>> OwnedWeapons;
};