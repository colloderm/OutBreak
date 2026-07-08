// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopCategoryElement.generated.h"


class UBorder;
class UTextBlock;

/**
 * 
 */
UCLASS()
class OUTBREAK_API UShopCategoryElement : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_Category_Name;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_Category_Count;
	
	virtual void NativeConstruct() override;
};
