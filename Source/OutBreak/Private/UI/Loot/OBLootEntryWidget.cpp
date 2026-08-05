// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Loot/OBLootEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Item/OBItemRegistry.h"
#include "UI/Loot/OBLootWindow.h"

void UOBLootEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (BTN_Take)
	{
		BTN_Take->OnClicked.AddDynamic(this, &UOBLootEntryWidget::HandleTakeClicked);
	}
}

void UOBLootEntryWidget::SetEntry(UOBLootWindow* InOwner, const FGameplayTag& InItemTag, int32 InCount)
{
	Owner = InOwner;
	ItemTag = InItemTag;
	Count = InCount;

	// 이름/아이콘 규칙(무기는 WeaponData 상속)은 레지스트리가 판단한다.
	FText DisplayName;
	UTexture2D* Icon = nullptr;
	UOBItemRegistry::GetItemDisplay(ItemTag, DisplayName, Icon);

	if (TXT_Name)
	{
		TXT_Name->SetText(DisplayName);
	}

	if (TXT_Count)
	{
		TXT_Count->SetText(Count > 1 ? FText::AsNumber(Count) : FText::GetEmpty());
	}

	if (IMG_Icon)
	{
		IMG_Icon->SetBrushFromTexture(Icon);
		IMG_Icon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UOBLootEntryWidget::HandleTakeClicked()
{
	if (Owner)
	{
		Owner->RequestTake(ItemTag, Count);
	}
}
