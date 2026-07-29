// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Party/OBPartyTypes.h"
#include "OBPartyMemberRowWidget.generated.h"

class UTextBlock; 
class UImage;

UCLASS()
class OUTBREAK_API UOBPartyMemberRowWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Setup(const FOBPartyMember& Member);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UImage> LeaderIcon;  // 왕관
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UTextBlock> StatusText;
};
