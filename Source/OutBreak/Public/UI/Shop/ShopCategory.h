// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopCategory.generated.h"


class UVerticalBox;
class UTextBlock;

/**
 * 
 */
UCLASS()
class OUTBREAK_API UShopCategory : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UVerticalBox> VBX_CategoryList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_NewStockTime;
	
	virtual void NativeConstruct() override;
};
