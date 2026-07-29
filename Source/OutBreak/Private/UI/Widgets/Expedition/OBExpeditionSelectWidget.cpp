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
#include "Online/OBOnlinePartySubsystem.h"

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
	// 재클릭이면 선택 해제(토글), 아니면 단일 선택.
	SelectedMap = (SelectedMap == InMap) ? nullptr : InMap;

	for (UOBExpeditionMapEntryWidget* E : Entries)
	{
		if (E)
		{
			E->SetSelected(E->GetMapData() == SelectedMap);
		}
	}

	RefreshGating();
}

void UOBExpeditionSelectWidget::RefreshGating()
{
	if (!StartButton) return;

	const EOBMatchmakingState St = Matchmaking ? Matchmaking->GetState() : EOBMatchmakingState::Idle;
	if (St == EOBMatchmakingState::Searching)
	{
		StartButton->SetVisibility(ESlateVisibility::Visible);
		StartButton->SetIsEnabled(true); // 취소 허용
		return;
	}
	
	if (St == EOBMatchmakingState::Starting)
	{
		StartButton->SetVisibility(ESlateVisibility::Visible);
		StartButton->SetIsEnabled(false); 
		return;
	}
	
	// Idle: 지역 선택 전이면 버튼 자체를 숨김.
	if (SelectedMap == nullptr)
	{
		StartButton->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 지역 선택됨 → 버튼 표시, 팀장만 누를 수 있게 활성.
	StartButton->SetVisibility(ESlateVisibility::Visible);
	
	APlayerController* PC = GetOwningPlayer();
	AOBPlayerStateBase* PS = PC ? PC->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	const bool bLeader = PS ? PS->IsPartyLeader() : false;
	StartButton->SetIsEnabled(bLeader);
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
	// 1) 솔로 매칭 중이면 중지(토글). 파티는 매칭 검색을 안 쓰므로 이 분기와 무관.
	if (Matchmaking && Matchmaking->GetState() == EOBMatchmakingState::Searching)
	{
		Matchmaking->CancelMatchmaking();
		return;
	}
	
	if (!SelectedMap) return;
	
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	// 2) [M8-3] Steam 파티 중이면 → 온라인 경로.
	//    리더(=이 버튼 누를 수 있는 유일한 사람): 팀원에게 출발 신호 브로드캐스트 + 자신 이동.
	//    멤버: 이 함수에 오지 않음(버튼 비활성). PollLeaderStart로 자동 팔로우.
	if (UOBOnlinePartySubsystem* Online = GI->GetSubsystem<UOBOnlinePartySubsystem>())
	{
		if (Online->IsInParty())
		{
			const FString Addr = SelectedMap->TestServerAddress;
			if (Addr.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Expedition] 선택 맵 TestServerAddress 비어있음 → 파티 시작 불가"));
				return;
			}
			Online->LeaderStartExpedition(Addr);
			return;
		}
	}

	// 3) 솔로 → 기존 매칭(검색 후 이동). 실 파티는 위에서 처리하므로 bParty=false.
	if (Matchmaking)
	{
		Matchmaking->StartMatchmaking(SelectedMap, /*bParty=*/false);
	}
}
