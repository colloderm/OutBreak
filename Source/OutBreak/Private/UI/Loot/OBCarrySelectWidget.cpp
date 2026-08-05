// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Loot/OBCarrySelectWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Item/OBItemRegistry.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "UI/Loot/OBLootEntryWidget.h"

void UOBCarrySelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Rebuild();
}

UOBLoadoutSubsystem* UOBCarrySelectWidget::GetLoadout() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
}

void UOBCarrySelectWidget::Rebuild()
{
	UOBLoadoutSubsystem* LS = GetLoadout();
	if (!LS) return;

	FillList(Box_Stash, LS->GetStashItems(), true);
	FillList(Box_Carry, LS->GetCarryItems(), false);

	if (TXT_CarryEmpty)
	{
		TXT_CarryEmpty->SetVisibility(LS->GetCarryItems().IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UOBCarrySelectWidget::FillList(UPanelWidget* Box, const TArray<FOBItemStack>& Items, bool bFromStash)
{
	if (!Box) return;
	Box->ClearChildren();
	if (!EntryWidgetClass) return;

	TArray<FOBItemStack> Sorted = Items;
	UOBItemRegistry::SortStacks(Sorted);

	for (const FOBItemStack& Stack : Sorted)
	{
		if (Stack.IsEmpty()) continue;

		UOBLootEntryWidget* Entry = CreateWidget<UOBLootEntryWidget>(this, EntryWidgetClass);
		if (!Entry) continue;

		Entry->SetEntry(Stack.ItemTag, Stack.Count);
		Entry->OnEntryClicked.BindUObject(this, bFromStash
			? &UOBCarrySelectWidget::HandleStashClicked 
			: &UOBCarrySelectWidget::HandleCarryClicked);
		Box->AddChild(Entry);
	}
}

void UOBCarrySelectWidget::HandleStashClicked(const FGameplayTag& ItemTag, int32 Count)
{
	if (UOBLoadoutSubsystem* LS = GetLoadout())
	{
		LS->AddCarryItem(ItemTag, Count);
		Rebuild();
	}
}

void UOBCarrySelectWidget::HandleCarryClicked(const FGameplayTag& ItemTag, int32 Count)
{
	if (UOBLoadoutSubsystem* LS = GetLoadout())
	{
		LS->RemoveCarryItem(ItemTag, Count);
		Rebuild();
	}
}