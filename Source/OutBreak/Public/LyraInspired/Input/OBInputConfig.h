// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "OBInputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FOBInputAction
{
	GENERATED_BODY()
	
	// 바인딩 할 Enhanced Input 액션.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;
	
	// 이 액션이 발동할 능력의 입력 태그(예: InputTag.Weapon.Fire)
	UPROPERTY(EditDefaultsOnly, Category = "Input", Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

// 능력 발동용 입력 액션↔태그 매핑을 모은 데이터. 컨트롤러가 이걸로 바인딩.
UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBInputConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	// 능력 발동에 쓰는 입력 액션 목록(발사 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FOBInputAction> AbilityInputActions;
};
