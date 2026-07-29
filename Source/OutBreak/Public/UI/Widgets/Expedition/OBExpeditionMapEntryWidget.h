// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBExpeditionMapEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UOBExpeditionMapData;

// 카드 클릭 알림(선택된 MapData 전달).
DECLARE_MULTICAST_DELEGATE_OneParam(FOBOnMapEntryClicked, UOBExpeditionMapData*);

UCLASS()
class OUTBREAK_API UOBExpeditionMapEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Setup(UOBExpeditionMapData* InMapData);
	void SetSelected(bool bSelected);
	UOBExpeditionMapData* GetMapData() const { return MapData; }
	
	FOBOnMapEntryClicked OnEntryClicked;
	
protected:
	virtual void NativeConstruct() override;
	
	UFUNCTION()
	void HandleClicked();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RootButton;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DifficultyText;

	// 선택 시 표시(체크/하이라이트).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CheckImage;

	UPROPERTY()
	TObjectPtr<UOBExpeditionMapData> MapData;
};
