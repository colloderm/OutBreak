// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UserCurrency.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class OUTBREAK_API UUserCurrency : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ScrapValue;
	
	
	virtual void NativeConstruct() override;
};
