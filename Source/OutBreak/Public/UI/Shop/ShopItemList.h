// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopItemList.generated.h"

class UVerticalBox;

/**
 * 
 */
UCLASS()
class OUTBREAK_API UShopItemList : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> VBX_ItemList;
	
	virtual void NativeConstruct() override;
};
