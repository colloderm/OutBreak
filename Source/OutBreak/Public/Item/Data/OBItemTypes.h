// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OBItemTypes.generated.h"

/**
왜 존재하는가?
 - 창고 / 가방 / 드랍테이블 / 컨테이너가 아이템을 주고받는 공통 단위.
무엇을 저장하는가?
 - 식별 태그 + 수량뿐. 실제 스펙(이름/가격/스택 한계)은 UOBItemDefinition에 있다.
멀티플레이 역할?
 - 태그+정수라 복제 비용이 작다. 인벤토리 배열째로 복제된다.
 */

// 자동 정렬 1순위. 열거 순서가 곧 정렬 순서다(순서를 바꾸면 정렬 결과가 바뀐다).
UENUM(BlueprintType)
enum class EOBItemCategory : uint8
{
	Weapon      UMETA(DisplayName = "무기"),
	Ammo        UMETA(DisplayName = "탄약"),
	Consumable  UMETA(DisplayName = "소모품"),
	Valuable    UMETA(DisplayName = "귀중품"),
	Material    UMETA(DisplayName = "재료")
};

USTRUCT(BlueprintType)
struct FOBItemStack
{
	GENERATED_BODY()

	// 무효 태그 = 빈 칸. 위치 기반 가방에서 "이 칸은 비었음"을 뜻한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Item")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Item", Meta = (ClampMin = "0"))
	int32 Count = 0;

	FOBItemStack() = default;
	FOBItemStack(const FGameplayTag& InTag, int32 InCount)
		: ItemTag(InTag), Count(InCount) {}

	bool IsEmpty() const { return !ItemTag.IsValid() || Count <= 0; }
	void Clear() { ItemTag = FGameplayTag(); Count = 0; }

	bool operator==(const FOBItemStack& Other) const
	{
		return ItemTag == Other.ItemTag && Count == Other.Count;
	}
};