// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBPlayerListWidget.generated.h"

class UVerticalBox; 
class UTextBlock; 
class UOBPlayerRowWidget;

UCLASS()
class OUTBREAK_API UOBPlayerListWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void RefreshPlayers();
	
protected:
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UVerticalBox> RowsBox;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UTextBlock> CountText;
	
	UPROPERTY(EditAnywhere, Category="Lobby") 
	TSubclassOf<UOBPlayerRowWidget> RowWidgetClass;
	
	UPROPERTY(EditAnywhere, Category="Lobby") 
	int32 MaxSlots = 3;
};
