// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Sections/ShopActionSectionWidget.h"

#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UShopActionSectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BuyButton)
	{
		BuyButton->OnClicked.AddUniqueDynamic(this, &UShopActionSectionWidget::HandleBuyClicked);
	}
	if (BarterButton)
	{
		BarterButton->OnClicked.AddUniqueDynamic(this, &UShopActionSectionWidget::HandleBarterClicked);
	}
	if (IncreaseQuantityButton)
	{
		IncreaseQuantityButton->OnClicked.AddUniqueDynamic(this, &UShopActionSectionWidget::HandleIncreaseQuantityClicked);
	}
	if (DecreaseQuantityButton)
	{
		DecreaseQuantityButton->OnClicked.AddUniqueDynamic(this, &UShopActionSectionWidget::HandleDecreaseQuantityClicked);
	}
}

void UShopActionSectionWidget::NativeDestruct()
{
	if (BuyButton)
	{
		BuyButton->OnClicked.RemoveDynamic(this, &UShopActionSectionWidget::HandleBuyClicked);
	}
	if (BarterButton)
	{
		BarterButton->OnClicked.RemoveDynamic(this, &UShopActionSectionWidget::HandleBarterClicked);
	}
	if (IncreaseQuantityButton)
	{
		IncreaseQuantityButton->OnClicked.RemoveDynamic(this, &UShopActionSectionWidget::HandleIncreaseQuantityClicked);
	}
	if (DecreaseQuantityButton)
	{
		DecreaseQuantityButton->OnClicked.RemoveDynamic(this, &UShopActionSectionWidget::HandleDecreaseQuantityClicked);
	}

	Super::NativeDestruct();
}

void UShopActionSectionWidget::ApplyActionState(const FShopActionState& ActionState)
{
	SelectedItemId = ActionState.ItemId;
	MaximumQuantity = FMath::Max(1, ActionState.MaximumQuantity);
	bBuyEnabled = ActionState.bCanBuy;
	bBarterEnabled = ActionState.bCanBarter;
	bHasStock = ActionState.bHasStock;

	ApplyQuantity(ActionState.Quantity, false);

	if (UnavailableReasonText)
	{
		const bool bHasReason = !ActionState.UnavailableReason.IsEmpty();
		UnavailableReasonText->SetText(ActionState.UnavailableReason);
		UnavailableReasonText->SetVisibility(bHasReason ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (HoldProgressBar)
	{
		HoldProgressBar->SetPercent(0.0f);
	}

	RefreshButtonState();
}

void UShopActionSectionWidget::SetSelectedItemId(FName ItemId)
{
	SelectedItemId = ItemId;
	RefreshButtonState();
}

void UShopActionSectionWidget::SetQuantity(int32 InQuantity)
{
	ApplyQuantity(InQuantity, true);
}

void UShopActionSectionWidget::SetMaximumQuantity(int32 InMaximumQuantity)
{
	MaximumQuantity = FMath::Max(1, InMaximumQuantity);
	ApplyQuantity(Quantity, false);
	RefreshButtonState();
}

void UShopActionSectionWidget::SetBuyEnabled(bool bEnabled)
{
	bBuyEnabled = bEnabled;
	RefreshButtonState();
}

void UShopActionSectionWidget::SetBarterEnabled(bool bEnabled)
{
	bBarterEnabled = bEnabled;
	RefreshButtonState();
}

void UShopActionSectionWidget::ResetActionState()
{
	SelectedItemId = NAME_None;
	Quantity = 1;
	MaximumQuantity = 1;
	bBuyEnabled = false;
	bBarterEnabled = false;
	bHasStock = true;

	if (UnavailableReasonText)
	{
		UnavailableReasonText->SetText(FText::GetEmpty());
		UnavailableReasonText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HoldProgressBar)
	{
		HoldProgressBar->SetPercent(0.0f);
	}

	RefreshQuantityText();
	RefreshButtonState();
}

void UShopActionSectionWidget::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
	RefreshButtonState();
}

void UShopActionSectionWidget::HandleBuyClicked()
{
	if (SelectedItemId.IsNone() || !bBuyEnabled || !bHasStock)
	{
		return;
	}

	FShopPurchaseRequest Request;
	Request.ItemId = SelectedItemId;
	Request.Quantity = Quantity;
	OnPurchaseRequested.Broadcast(Request);
}

void UShopActionSectionWidget::HandleBarterClicked()
{
	if (SelectedItemId.IsNone() || !bBarterEnabled)
	{
		return;
	}

	FShopBarterRequest Request;
	Request.ItemId = SelectedItemId;
	Request.Quantity = Quantity;
	OnBarterRequested.Broadcast(Request);
}

void UShopActionSectionWidget::HandleIncreaseQuantityClicked()
{
	SetQuantity(Quantity + 1);
}

void UShopActionSectionWidget::HandleDecreaseQuantityClicked()
{
	SetQuantity(Quantity - 1);
}

void UShopActionSectionWidget::ApplyQuantity(int32 InQuantity, bool bBroadcastChange)
{
	const int32 ClampedQuantity = FMath::Clamp(InQuantity, 1, FMath::Max(1, MaximumQuantity));
	const bool bChanged = Quantity != ClampedQuantity;
	Quantity = ClampedQuantity;

	RefreshQuantityText();
	RefreshButtonState();

	if (bBroadcastChange && bChanged)
	{
		OnQuantityChanged.Broadcast(Quantity);
	}
}

void UShopActionSectionWidget::RefreshButtonState()
{
	const bool bHasItem = !SelectedItemId.IsNone();
	if (BuyButton)
	{
		BuyButton->SetIsEnabled(bInteractionEnabled && bHasItem && bHasStock && bBuyEnabled);
	}
	if (BarterButton)
	{
		BarterButton->SetIsEnabled(bInteractionEnabled && bHasItem && bBarterEnabled);
	}
	if (IncreaseQuantityButton)
	{
		IncreaseQuantityButton->SetIsEnabled(bInteractionEnabled && bHasItem && Quantity < MaximumQuantity);
	}
	if (DecreaseQuantityButton)
	{
		DecreaseQuantityButton->SetIsEnabled(bInteractionEnabled && bHasItem && Quantity > 1);
	}
}

void UShopActionSectionWidget::RefreshQuantityText()
{
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Quantity));
	}
	if (MaximumQuantityText)
	{
		MaximumQuantityText->SetText(FText::AsNumber(MaximumQuantity));
	}
}
