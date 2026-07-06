// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Expedition/OBExpeditionSelectWidget.h"

#include "UI/Widgets/Expedition/OBExpeditionMapEntryWidget.h"
#include "Game/Expedition/OBExpeditionMapCatalog.h"
#include "Game/Expedition/OBExpeditionMapData.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Player/State/OBPlayerStateBase.h"
#include "TimerManager.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBoxSlot.h"

void UOBExpeditionSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartButton) 
		StartButton->OnClicked.AddDynamic(this, &UOBExpeditionSelectWidget::HandleStartClicked);

	BuildList();

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			GatingTimer, this, &UOBExpeditionSelectWidget::RefreshGating, 0.3f, true);
		
		RefreshGating();
	}
}

void UOBExpeditionSelectWidget::NativeDestruct()
{
	if (UWorld* W = GetWorld()) 
		W->GetTimerManager().ClearTimer(GatingTimer);
	
	Super::NativeDestruct();
}

void UOBExpeditionSelectWidget::BuildList()
{
	if (!ListBox || !EntryWidgetClass || !MapCatalog) return;

	ListBox->ClearChildren();
	Entries.Reset();

	for (UOBExpeditionMapData* Map : MapCatalog->AvailableMaps)
	{
		if (!Map) continue;

		UOBExpeditionMapEntryWidget* Entry = CreateWidget<UOBExpeditionMapEntryWidget>(this, EntryWidgetClass);
		if (!Entry) continue;

		Entry->Setup(Map);
		Entry->OnEntryClicked.AddUObject(this, &UOBExpeditionSelectWidget::HandleEntryClicked);
		
		UPanelSlot* ExpSlot = ListBox->AddChild(Entry);
		// 컨테이너가 VerticalBox든 ScrollBox든 각 카드 아래쪽에 간격을 준다.
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(ExpSlot))
			VSlot->SetPadding(FMargin(0.f, 0.f, 0.f, EntrySpacing));
		else if (UScrollBoxSlot* SSlot = Cast<UScrollBoxSlot>(ExpSlot))
			SSlot->SetPadding(FMargin(0.f, 0.f, 0.f, EntrySpacing));

		Entries.Add(Entry);
	}
}

void UOBExpeditionSelectWidget::HandleEntryClicked(UOBExpeditionMapData* InMap)
{
	// 단일 선택: 클릭한 지역만 선택 표시(세션 1개 = 맵 1개).
	SelectedMap = InMap;
	for (UOBExpeditionMapEntryWidget* E : Entries)
		if (E) E->SetSelected(E->GetMapData() == SelectedMap);
}

void UOBExpeditionSelectWidget::RefreshGating()
{
	if (!StartButton) return;

	APlayerController* PC = GetOwningPlayer();
	AOBPlayerStateBase* PS = PC ? PC->GetPlayerState<AOBPlayerStateBase>() : nullptr;

	const bool bLeader = PS ? PS->IsPartyLeader() : false;   // 팀원=false(M6)
	const bool bHasSelection = (SelectedMap != nullptr);
	StartButton->SetIsEnabled(bLeader && bHasSelection);     // 팀장 + 지역선택 시에만
}

void UOBExpeditionSelectWidget::HandleStartClicked()
{
	if (!SelectedMap) return;

	// TODO(M5): 매칭 상태머신 시작(선택 맵으로 큐 진입) + "탐사 시작↔중지" 토글 + [매칭중] 표시 + 5분 규칙.
	UE_LOG(LogTemp, Log, TEXT("[Expedition] 매칭 요청: %s"), *SelectedMap->DisplayName.ToString());
}
