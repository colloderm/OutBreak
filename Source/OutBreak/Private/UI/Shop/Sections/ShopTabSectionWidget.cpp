// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Sections/ShopTabSectionWidget.h"

#include "Components/ListView.h"
#include "UI/Shop/EntryData/ShopTabEntryData.h"

void UShopTabSectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TabListView)
	{
		TabListView->OnItemClicked().AddUObject(this, &UShopTabSectionWidget::HandleTabItemClicked);
	}
}

void UShopTabSectionWidget::NativeDestruct()
{
	if (TabListView)
	{
		TabListView->OnItemClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UShopTabSectionWidget::ApplyTabs(const TArray<FShopTabViewData>& Tabs)
{
	ClearTabs();

	if (!TabListView)
	{
		return;
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(Tabs.Num());
	TabEntryDataObjects.Reserve(Tabs.Num());

	for (const FShopTabViewData& Tab : Tabs)
	{
		UShopTabEntryData* EntryData = NewObject<UShopTabEntryData>(this);
		EntryData->Initialize(Tab, Tab.TabId == SelectedTabId);
		TabEntryDataObjects.Add(EntryData);
		ListItems.Add(EntryData);
	}

	TabListView->SetListItems(ListItems);
}

void UShopTabSectionWidget::SetSelectedTab(FName TabId)
{
	SelectedTabId = TabId;
	RebuildSelectionState();
}

void UShopTabSectionWidget::ClearTabs()
{
	TabEntryDataObjects.Reset();
	if (TabListView)
	{
		TabListView->ClearListItems();
	}
}

void UShopTabSectionWidget::SetInteractionEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);
	if (TabListView)
	{
		TabListView->SetIsEnabled(bEnabled);
	}
}

void UShopTabSectionWidget::HandleTabItemClicked(UObject* Item)
{
	const UShopTabEntryData* EntryData = Cast<UShopTabEntryData>(Item);
	if (!EntryData || !EntryData->GetViewData().bEnabled)
	{
		return;
	}

	OnTabRequested.Broadcast(EntryData->GetTabId());
}

void UShopTabSectionWidget::RebuildSelectionState()
{
	for (UShopTabEntryData* EntryData : TabEntryDataObjects)
	{
		if (EntryData)
		{
			EntryData->SetSelected(EntryData->GetTabId() == SelectedTabId);
		}
	}
	if (TabListView)
	{
		TabListView->RequestRefresh();
	}
}
