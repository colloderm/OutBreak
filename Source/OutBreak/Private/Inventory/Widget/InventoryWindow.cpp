// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventoryWindow.h"
#include "Components/UniformGridPanel.h"
#include "Inventory/Widget/InventorySlot.h"

void UInventoryWindow::Update()
{
	auto Children = InventorySlots->GetAllChildren();
	
	for (auto Child : Children)
	{
		UInventorySlot* InventorySlot = Cast<UInventorySlot>(Child);
		
		if (!IsValid(InventorySlot)) continue;
		
		InventorySlot->Update();		
	}
}
