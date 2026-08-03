// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Item/Data/OBItemTypes.h"
#include "OBItemDefinition.generated.h"

class AOBWeaponBase;
class UTexture2D;

/**
왜 존재하는가?
 - 아이템 한 종류의 스펙. DT_Items(DataTable)의 행 하나가 이 구조체다.
 - 에셋 1개 = 아이템 1개였다가 표로 옮겼다. 코드가 아이템을 태그로만 지목해서 저장 방식만 바뀌었다.
멀티플레이 역할?
 - 모든 머신이 같은 표를 읽는다. 태그만 복제하고 스펙은 각자 조회한다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FOBItemDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()
	
	// 이 아이템의 고유 식별자. 창고/가방/드랍테이블이 전부 이 태그로만 아이템을 지목한다.
	// 탄약은 반드시 무기의 AmmoType과 같은 태그(Ammo.Pistol 등)를 써야 재장전이 바로 조회된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FGameplayTag ItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EOBItemCategory Category = EOBItemCategory::Material;

	// 자동 정렬 2순위. 같은 카테고리 안의 순서를 직접 강제할 때만 쓴다(작을수록 앞).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	int32 SortOrder = 0;

	// ===== 표시 =====

	// 비워두면 무기 아이템은 WeaponData의 값을 상속한다(같은 정보를 두 번 입력하지 않게).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", Meta = (MultiLine = "true"))
	FText Description;

	// 소프트 참조. 표 전체가 로드돼도 아이콘까지 다 올라오지 않게 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	// ===== 휴대 =====

	// 한 칸에 쌓을 수 있는 최대 수량. 1이면 무조건 한 칸을 차지한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Carry", Meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	// 1개당 무게(kg). 무게 총량 제한은 아직 없다 — 지금은 수치만 채워둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Carry", Meta = (ClampMin = "0.0"))
	float Weight = 0.f;

	// ===== 상점 =====

	// 구매가. 0이면 상점에서 살 수 없다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", Meta = (ClampMin = "0"))
	int32 BuyPrice = 0;

	// 판매가. 0이면 팔 수 없다. 자동 정렬 3순위로도 쓴다(비싼 것부터).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", Meta = (ClampMin = "0"))
	int32 SellPrice = 0;

	// ===== 무기 전용 =====

	// 무기 카테고리 전용. 이 아이템이 실제로 어떤 무기가 되는지.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftClassPtr<AOBWeaponBase> WeaponClass;
};
