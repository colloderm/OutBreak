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
 - 아이템 한 종류의 스펙 원본. 태그 하나로 이걸 찾아 이름/아이콘/가격/스택을 얻는다.
멀티플레이 역할?
 - 에셋이라 모든 머신에 동일. 복제하지 않는다(태그만 복제하고 각자 조회).
 */
UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 이 아이템의 고유 식별자. 창고/가방/드랍테이블이 전부 이 태그로만 아이템을 지목한다.
	// 탄약은 반드시 무기의 AmmoType과 같은 태그(Ammo.Pistol 등)를 써야 한다
	// → 재장전이 별도 매핑 없이 가방을 바로 조회할 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FGameplayTag ItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EOBItemCategory Category = EOBItemCategory::Material;

	// 자동 정렬 2순위. 같은 카테고리 안의 순서를 직접 강제할 때만 쓴다(작을수록 앞).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	int32 SortOrder = 0;

	// ===== 표시 =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", Meta = (MultiLine = "true"))
	FText Description;

	// 인벤토리 칸 / 루팅 목록 / 상점에 쓰는 아이콘.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> Icon;

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

	// 이 아이템이 실제로 어떤 무기가 되는지. 창고/장착이 이 클래스를 쓴다.
	// 디스크 저장을 고려해 Soft(에셋 경로 직렬화).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon",
		Meta = (EditCondition = "Category == EOBItemCategory::Weapon", EditConditionHides))
	TSoftClassPtr<AOBWeaponBase> WeaponClass;
};
