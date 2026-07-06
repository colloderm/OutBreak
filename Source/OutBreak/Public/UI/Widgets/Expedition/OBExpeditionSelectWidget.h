// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBExpeditionSelectWidget.generated.h"

class UButton;
class UPanelWidget;
class UOBExpeditionMapCatalog;
class UOBExpeditionMapData;
class UOBExpeditionMapEntryWidget;

UCLASS()
class OUTBREAK_API UOBExpeditionSelectWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void BuildList();
	void HandleEntryClicked(UOBExpeditionMapData* InMap);

	UFUNCTION()
	void HandleStartClicked();

	// 팀장 + 선택 여부로 시작 버튼 활성화 갱신(0.3s).
	void RefreshGating();

	// 카드 컨테이너(VerticalBox/ScrollBox 등 — UPanelWidget).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ListBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(EditAnywhere, Category = "Expedition")
	TObjectPtr<UOBExpeditionMapCatalog> MapCatalog;
	
	// 카드 사이 세로 간격(px).
	UPROPERTY(EditAnywhere, Category = "Expedition")
	float EntrySpacing = 12.f;

	UPROPERTY(EditAnywhere, Category = "Expedition")
	TSubclassOf<UOBExpeditionMapEntryWidget> EntryWidgetClass;

	UPROPERTY()
	TArray<TObjectPtr<UOBExpeditionMapEntryWidget>> Entries;

	UPROPERTY()
	TObjectPtr<UOBExpeditionMapData> SelectedMap;

	FTimerHandle GatingTimer;
};
