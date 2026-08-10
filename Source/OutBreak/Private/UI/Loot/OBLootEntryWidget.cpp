// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Loot/OBLootEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Inventory/Data/InventoryData.h"
#include "Item/OBItemRegistry.h"
#include "UI/Tooltip/OBItemTooltipLibrary.h"

void UOBLootEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (BTN_Take)
	{
		BTN_Take->OnClicked.AddDynamic(this, &UOBLootEntryWidget::HandleTakeClicked);
	}
}

void UOBLootEntryWidget::SetEntry(const FGameplayTag& InItemTag, int32 InCount)
{
	InstanceId.Invalidate();
	ItemTag = InItemTag;
	Count = InCount;

	FText DisplayName;
	UTexture2D* Icon = nullptr;
	UOBItemRegistry::GetItemDisplay(ItemTag, DisplayName, Icon);

	if (TXT_Name)  
		TXT_Name->SetText(DisplayName);
	if (TXT_Count) 
		TXT_Count->SetText(Count > 1 ? FText::AsNumber(Count) : FText::GetEmpty());

	if (IMG_Icon)
	{
		IMG_Icon->SetBrushFromTexture(Icon);
		IMG_Icon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UOBLootEntryWidget::SetItemInstance(const FInventoryData& InItemInstance)
{
	SetEntry(InItemInstance.ItemTag, InItemInstance.ItemStack);
	InstanceId = InItemInstance.InstanceId;
	SetToolTipText(UOBItemTooltipLibrary::BuildFallbackTooltipText(InItemInstance));
}

void UOBLootEntryWidget::HandleTakeClicked()
{
	const int32 RequestedCount = ResolveClickCount();
	if (InstanceId.IsValid())
	{
		OnInstanceClicked.ExecuteIfBound(InstanceId, RequestedCount);
	}
	else
	{
		OnEntryClicked.ExecuteIfBound(ItemTag, RequestedCount);
	}
}

int32 UOBLootEntryWidget::ResolveClickCount() const
{
	if (Count <= 1) return Count;

	// UButton의 OnClicked는 수정키를 안 넘겨준다. 클릭 시점의 키보드 상태를 직접 읽는다.
	if (!FSlateApplication::IsInitialized()) return Count;

	const FModifierKeysState Mods = FSlateApplication::Get().GetModifierKeys();
	if (Mods.IsControlDown()) return 1;
	if (Mods.IsShiftDown())   return FMath::DivideAndRoundUp(Count, 2);
	
	return Count;
}
