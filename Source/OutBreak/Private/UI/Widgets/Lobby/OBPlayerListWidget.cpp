// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/OBPlayerListWidget.h"

#include "UI/Widgets/Lobby/OBPlayerRowWidget.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"

void UOBPlayerListWidget::RefreshPlayers()
{
	if (!RowsBox || !RowWidgetClass) return;
	RowsBox->ClearChildren();

	int32 Count = 0;
	AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	APlayerState* MyPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			AOBPlayerStateBase* OBPS = Cast<AOBPlayerStateBase>(PS);
			if (!OBPS) continue;
			UOBPlayerRowWidget* Row = CreateWidget<UOBPlayerRowWidget>(this, RowWidgetClass);
			if (!Row) continue;
			Row->Setup(OBPS->GetPlayerName(), OBPS->IsReady(), OBPS == MyPS);
			RowsBox->AddChild(Row);
			++Count;
		}
	}
	for (int32 i = Count; i < MaxSlots; ++i)
	{
		if (UOBPlayerRowWidget* Row = CreateWidget<UOBPlayerRowWidget>(this, RowWidgetClass))
		{
			Row->SetEmpty();
			RowsBox->AddChild(Row);
		}
	}
	if (CountText) 
		CountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Count, MaxSlots)));
}