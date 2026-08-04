// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Expedition/OBExpeditionResultWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "UI/Loot/OBLootEntryWidget.h"

void UOBExpeditionResultWidget::SetHaul(const TArray<FOBItemStack>& InHaul)
{
	if (TXT_HaulEmpty)
	{
		TXT_HaulEmpty->SetVisibility(
			InHaul.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!Box_Haul) return;
	Box_Haul->ClearChildren();

	if (!EntryWidgetClass) return;

	for (const FOBItemStack& Stack : InHaul)
	{
		if (Stack.IsEmpty()) continue;

		UOBLootEntryWidget* Entry = CreateWidget<UOBLootEntryWidget>(this, EntryWidgetClass);
		if (!Entry) continue;

		// 소유 창이 없다 = 클릭해도 아무 일 없다. 결과창은 읽기 전용이다.
		Entry->SetEntry(nullptr, Stack.ItemTag, Stack.Count);
		Box_Haul->AddChild(Entry);
	}
}