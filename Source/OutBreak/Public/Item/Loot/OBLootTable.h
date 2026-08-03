// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Item/Data/OBItemTypes.h"
#include "OBLootTable.generated.h"

/**
 왜 존재하는가?
 - 같은 장소를 반복 파밍해도 나오는 게 매번 달라지도록, 확정 드랍이 아닌 가중치 추첨을 쓴다.
 무엇을 저장하는가?
 - 뽑을 횟수 범위 + 항목별 (아이템 태그 / 가중치 / 수량 범위).
 멀티플레이 역할?
 - 굴리는 건 서버뿐. 결과만 복제된다(클라가 굴리면 서버와 결과가 갈린다).
 */

USTRUCT(BlueprintType)
struct FOBLootEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FGameplayTag ItemTag;

	// 상대 가중치. 다른 항목과의 비율로만 의미가 있다(합이 100일 필요 없음). 0이면 안 나온다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", Meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", Meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", Meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 서버 전용. 같은 스트림을 주면 항상 같은 결과가 나온다(재파밍 방지에 사용).
	void Roll(FRandomStream& Stream, TArray<FOBItemStack>& OutItems) const;
	
public:
	// 한 번 채울 때 추첨하는 횟수. 0이면 빈 상자가 나올 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", Meta = (ClampMin = "0"))
	int32 MinRolls = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", Meta = (ClampMin = "0"))
	int32 MaxRolls = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TArray<FOBLootEntry> Entries;

	
};
