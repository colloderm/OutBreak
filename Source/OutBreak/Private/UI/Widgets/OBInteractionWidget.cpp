// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/OBInteractionWidget.h"

#include "Components/Button.h"
#include "Player/Controller/OBPlayerController.h"

// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/OBInteractionWidget.h"

#include "Components/Button.h"
#include "Player/Controller/OBPlayerController.h"

void UOBInteractionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UIOnly 모드에서 키 입력은 포커스된 위젯에만 온다. 포커스를 못 받으면 ESC도 못 받는다.
	SetIsFocusable(true);

	if (BTN_Close)
	{
		BTN_Close->OnClicked.AddDynamic(this, &UOBInteractionWidget::HandleCloseClicked);
	}
}

FReply UOBInteractionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == CloseKey)
	{
		RequestClose();
		return FReply::Handled();   // 소비해야 PIE 정지 같은 상위 처리로 새지 않는다
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UOBInteractionWidget::HandleCloseClicked()
{
	RequestClose();
}

void UOBInteractionWidget::RequestClose()
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
	{
		PC->CloseInteractionWidget();
	}
}