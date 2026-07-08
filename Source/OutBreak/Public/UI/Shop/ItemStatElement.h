// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ItemStatElement.generated.h"


class UTextBlock;


/**
 * 
 */
UCLASS()
class OUTBREAK_API UItemStatElement : public UUserWidget
{
	GENERATED_BODY()

	
public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetStatData(const FShopItemStatViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ClearStatData();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_StatLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_StatValue;
	
	virtual void NativeConstruct() override;
};
