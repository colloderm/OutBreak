// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBPlayerRowWidget.generated.h"

class UTextBlock; 
class UImage;

UCLASS()
class OUTBREAK_API UOBPlayerRowWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Setup(const FString& PlayerName, bool bReady, bool bIsMe);
	void SetEmpty();
	
protected:
	UPROPERTY(meta=(BindWidget))         
	TObjectPtr<UTextBlock> NameText;
	
	UPROPERTY(meta=(BindWidget))         
	TObjectPtr<UImage> ReadyCheck;
	
	UPROPERTY(meta=(BindWidgetOptional)) 
	TObjectPtr<UImage> CrownImage;
};
