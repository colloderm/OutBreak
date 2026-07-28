// Fill out your copyright notice in the Description page of Project Settings.


#include "Dialogue/OBDialogueWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Player/Controller/OBPlayerController.h"

void UOBDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// 버튼 클릭을 인덱스 별 핸들러에 1회 연결
	if (Btn_0)
		Btn_0->OnClicked.AddDynamic(this, &UOBDialogueWidget::OnOption0);
	if (Btn_1)
		Btn_1->OnClicked.AddDynamic(this, &UOBDialogueWidget::OnOption1);
	if (Btn_2)
		Btn_2->OnClicked.AddDynamic(this, &UOBDialogueWidget::OnOption2);
	if (Btn_3)
		Btn_3->OnClicked.AddDynamic(this, &UOBDialogueWidget::OnOption3);
	if (Btn_Close) 
		Btn_Close->OnClicked.AddDynamic(this, &UOBDialogueWidget::OnCloseClicked);
}

void UOBDialogueWidget::StartDialogue(UDataTable* InTable, FName StartNode)
{
	Table = InTable;
	ShowNode(StartNode);
}

void UOBDialogueWidget::ShowNode(FName RowName)
{
	if (!Table || RowName.IsNone()) return;
	
	const FOBDialogueNode* Node =
		Table->FindRow<FOBDialogueNode>(RowName, TEXT("OBDialogue"), /*bWarnIfMissing=*/true);
	if (!Node) return;
	
	if (Text_Name)
		Text_Name->SetText(Node->SpeakerName);
	if (Text_Body)
		Text_Body->SetText(Node->Body);
	
	CurrentOptions = Node->Options;
	
	// 4개 버튼: 옵션 있으면 라벨 세팅 후 표시, 없으면 숨김.
	SetButton(0, Btn_0, Btn_0_Label);
	SetButton(1, Btn_1, Btn_1_Label);
	SetButton(2, Btn_2, Btn_2_Label);
	SetButton(3, Btn_3, Btn_3_Label);
}

void UOBDialogueWidget::SetButton(int32 Index, UButton* Btn, UTextBlock* Label)
{
	if (!Btn) return;
	
	if (CurrentOptions.IsValidIndex(Index))
	{
		if (Label) Label->SetText(CurrentOptions[Index].Label);
		Btn->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Btn->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOBDialogueWidget::OnOption0()
{
	HandleOption(0);
}

void UOBDialogueWidget::OnOption1()
{
	HandleOption(1);
}

void UOBDialogueWidget::OnOption2()
{
	HandleOption(2);
}

void UOBDialogueWidget::OnOption3()
{
	HandleOption(3);
}

void UOBDialogueWidget::OnCloseClicked()
{
	if (AOBPlayerController* PC = Cast<AOBPlayerController>(GetOwningPlayer()))
	{
		PC->CloseInteractionWidget();
	}
}

void UOBDialogueWidget::HandleOption(int32 Index)
{
	if (!CurrentOptions.IsValidIndex(Index)) return;

	const FOBDialogueOption Opt = CurrentOptions[Index];   // 값 복사(아래서 ShowNode가 배열을 덮어씀)

	// 부수효과 액션은 NPC에게 위임(이벤트 디스패처).
	if (Opt.Action != EOBDialogueAction::None)
	{
		OnDialogueAction.Broadcast(Opt.Action);
		return;   // NPC가 결과에 따라 GoToNode 또는 CloseInteractionWidget 호출
	}

	// 순수 대사 이동(액션 없는 선택지).
	if (Opt.NextNode.IsNone())
	{
		OnCloseClicked();
		return;
	}

	// 다음 노드로 이동.
	ShowNode(Opt.NextNode);
}
