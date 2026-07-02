// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ShopKeyHintEntryData.generated.h"

/**
 * Footer의 조작 힌트 ListView에 전달되는 표시 전용 UObject다.
 * 입력 매핑 이름이나 키 이미지는 WBP/게임 시스템에서 표시 문자열로 변환해 주입한다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UShopKeyHintEntryData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FText& InActionText, const FText& InKeyText);

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FText GetActionText() const { return ActionText; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FText GetKeyText() const { return KeyText; }

private:
	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	FText ActionText;

	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	FText KeyText;
};
