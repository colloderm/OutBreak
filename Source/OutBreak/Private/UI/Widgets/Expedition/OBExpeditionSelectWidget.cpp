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
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Matchmaking/OBMatchmakingSubsystem.h"
#include "Party/OBPartySubsystem.h"

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
	
	if (UGameInstance* GI = GetGameInstance())
		Matchmaking = GI->GetSubsystem<UOBMatchmakingSubsystem>();
	
	if (Matchmaking)
	{
		Matchmaking->OnStateChanged.AddUObject(this, &UOBExpeditionSelectWidget::HandleMatchStateChanged);
		Matchmaking->OnTick.AddUObject(this, &UOBExpeditionSelectWidget::HandleMatchTick);
		
		// 백그라운드로 매칭 중일 수 있으니 현재 상태로 UI 동기화
		HandleMatchStateChanged(Matchmaking->GetState());
		HandleMatchTick(Matchmaking->GetRemainingSeconds());
	}
}

void UOBExpeditionSelectWidget::NativeDestruct()
{
	if (UWorld* W = GetWorld()) 
		W->GetTimerManager().ClearTimer(GatingTimer);
	
	if (Matchmaking)
	{
		Matchmaking->OnStateChanged.RemoveAll(this);
		Matchmaking->OnTick.RemoveAll(this);
	}
	
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

	const EOBMatchmakingState St = Matchmaking ? Matchmaking->GetState() : EOBMatchmakingState::Idle;
	if (St == EOBMatchmakingState::Searching)
	{
		StartButton->SetIsEnabled(true);  
		return;
	} // 취소 허용
	
	if (St == EOBMatchmakingState::Starting)
	{
		StartButton->SetIsEnabled(false); 
		return;
	}

	// Idle: 팀장 + 지역선택 시에만.
	APlayerController* PC = GetOwningPlayer();
	AOBPlayerStateBase* PS = PC ? PC->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	const bool bLeader = PS ? PS->IsPartyLeader() : false;
	StartButton->SetIsEnabled(bLeader && (SelectedMap != nullptr));
}

void UOBExpeditionSelectWidget::HandleMatchStateChanged(EOBMatchmakingState NewState)
{
	const bool bSearching = (NewState == EOBMatchmakingState::Searching);
	const bool bStarting  = (NewState == EOBMatchmakingState::Starting);

	if (StartButtonLabel)
		StartButtonLabel->SetText(FText::FromString(bSearching ? TEXT("탐사 중지") : TEXT("탐사 시작")));

	if (MatchStatusText)
	{
		if (bStarting)
			MatchStatusText->SetText(FText::FromString(TEXT("탐사 지역으로 이동 중…")));
		MatchStatusText->SetVisibility((bSearching || bStarting) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	RefreshGating(); // 즉시 버튼 활성 갱신
}

void UOBExpeditionSelectWidget::HandleMatchTick(int32 RemainingSeconds)
{
	if (!MatchStatusText || !Matchmaking) return;
	if (Matchmaking->GetState() != EOBMatchmakingState::Searching) return;

	const int32 M = RemainingSeconds / 60;
	const int32 S = RemainingSeconds % 60;
	MatchStatusText->SetText(FText::FromString(FString::Printf(TEXT("이동수단 대기중...  %02d:%02d"), M, S)));
}

void UOBExpeditionSelectWidget::HandleStartClicked()
{
	if (!Matchmaking) return;

	if (Matchmaking->GetState() == EOBMatchmakingState::Searching)
	{
		Matchmaking->CancelMatchmaking(); // 매칭중 → 중지
		return;
	}
	
	if (!SelectedMap) return;
	
	bool bParty = false;
	if (UGameInstance* GI = GetGameInstance())
		if (UOBPartySubsystem* Party = GI->GetSubsystem<UOBPartySubsystem>())
			bParty = (Party->GetPartySize() >= 2);   // 2인+ = 파티 큐
	
	Matchmaking->StartMatchmaking(SelectedMap, bParty);
}
