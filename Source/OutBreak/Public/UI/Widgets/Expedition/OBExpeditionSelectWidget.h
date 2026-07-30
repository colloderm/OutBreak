// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/OBInteractionWidget.h"
#include "OBExpeditionSelectWidget.generated.h"

class UTextBlock;
class UOBMatchmakingSubsystem;
enum class EOBMatchmakingState : uint8;
class UButton;
class UPanelWidget;
class UOBExpeditionMapCatalog;
class UOBExpeditionMapData;
class UOBExpeditionMapEntryWidget;

UCLASS()
class OUTBREAK_API UOBExpeditionSelectWidget : public UOBInteractionWidget
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
	
	void HandleMatchStateChanged(EOBMatchmakingState NewState);
	void HandleMatchTick(int32 RemainingSeconds);
	
protected:
	// 카드 컨테이너(VerticalBox/ScrollBox 등 — UPanelWidget).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ListBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;
	
	// "탐사 시작"/"탐사 중지" 라벨(StartButton 내부 TextBlock).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StartButtonLabel;

	// "이동수단 대기중 mm:ss" 상태 표시(6시 방향).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchStatusText;

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
	
	UPROPERTY()
	TObjectPtr<UOBMatchmakingSubsystem> Matchmaking;

	FTimerHandle GatingTimer;
};
