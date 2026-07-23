// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/KeyBindableBtn.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UKeyBindableBtn::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->OnClicked.RemoveDynamic(this, &UKeyBindableBtn::HandleClicked);
		ClickButton->OnClicked.AddDynamic(this, &UKeyBindableBtn::HandleClicked);
	}
}

void UKeyBindableBtn::NativeDestruct()
{
	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->OnClicked.RemoveDynamic(this, &UKeyBindableBtn::HandleClicked);
	}

	Super::NativeDestruct();
}

FReply UKeyBindableBtn::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!GetClickButton() && CanTrigger())
	{
		BroadcastAction();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UKeyBindableBtn::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CanTrigger() && ActionData.InputKey.IsValid() && InKeyEvent.GetKey() == ActionData.InputKey)
	{
		BroadcastAction();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UKeyBindableBtn::SetActionData(const FShopActionViewData& InData)
{
	ActionData = InData;
	bActionEnabled = InData.bCanExecute;

	if (TXT_Action)
	{
		TXT_Action->SetText(InData.Label);
	}

	if (TXT_BindedKey)
	{
		if (!InData.InputDisplayText.IsEmpty())
		{
			TXT_BindedKey->SetText(InData.InputDisplayText);
		}
		else if (InData.InputKey.IsValid())
		{
			TXT_BindedKey->SetText(InData.InputKey.GetDisplayName());
		}
		else
		{
			TXT_BindedKey->SetText(FText::GetEmpty());
		}
	}

	SetActionEnabled(InData.bCanExecute);
	SetVisibility(ESlateVisibility::Visible);
}

void UKeyBindableBtn::SetActionEnabled(bool bEnabled)
{
	bActionEnabled = bEnabled;
	SetIsEnabled(CanTrigger());

	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->SetIsEnabled(CanTrigger());
	}
}

void UKeyBindableBtn::ClearActionData()
{
	ActionData = FShopActionViewData();
	bActionEnabled = false;

	if (TXT_Action)
	{
		TXT_Action->SetText(FText::GetEmpty());
	}

	if (TXT_BindedKey)
	{
		TXT_BindedKey->SetText(FText::GetEmpty());
	}

	SetIsEnabled(false);
	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->SetIsEnabled(false);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

FName UKeyBindableBtn::GetActionId() const
{
	return ActionData.ActionId;
}

void UKeyBindableBtn::HandleClicked()
{
	BroadcastAction();
}

void UKeyBindableBtn::BroadcastAction()
{
	if (CanTrigger())
	{
		OnActionTriggered.Broadcast(ActionData.ActionId);
	}
}

bool UKeyBindableBtn::CanTrigger() const
{
	return bActionEnabled && ActionData.bCanExecute && !ActionData.ActionId.IsNone();
}

UButton* UKeyBindableBtn::GetClickButton() const
{
	if (Button)
	{
		return Button.Get();
	}

	UButton* FoundButton = nullptr;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&FoundButton](UWidget* Widget)
		{
			if (!FoundButton)
			{
				FoundButton = Cast<UButton>(Widget);
			}
		});
	}

	return FoundButton;
}
