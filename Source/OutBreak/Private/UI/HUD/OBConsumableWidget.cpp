// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/HUD/OBConsumableWidget.h"

#include "Inventory/Components/OBInventoryComponent.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Components/TextBlock.h"

void UOBConsumableWidget::SetInventory(UOBInventoryComponent* InInventory)
{
	if (Inventory.IsValid() && ChangedHandle.IsValid())
		Inventory->OnInventoryChanged.Remove(ChangedHandle);

	Inventory = InInventory;
	if (Inventory.IsValid())
		ChangedHandle = Inventory->OnInventoryChanged.AddUObject(this, &UOBConsumableWidget::Refresh);

	Refresh();
}

void UOBConsumableWidget::Refresh()
{
	UOBInventoryComponent* Inv = Inventory.Get();
	const int32 Bandages = Inv ? Inv->GetItemCount(OBGameplayTags::Item_Bandage) : 0;
	const int32 Grenades = Inv ? Inv->GetItemCount(OBGameplayTags::Item_Grenade) : 0;

	if (BandageCountText) 
		BandageCountText->SetText(FText::AsNumber(Bandages));
	
	if (GrenadeCountText) 
		GrenadeCountText->SetText(FText::AsNumber(Grenades));
}

void UOBConsumableWidget::NativeDestruct()
{
	if (Inventory.IsValid() && ChangedHandle.IsValid())
		Inventory->OnInventoryChanged.Remove(ChangedHandle);
	
	Super::NativeDestruct();
}
