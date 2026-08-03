// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Subsystem/ItemDataSubsystem.h"

#include "Engine/DataTable.h"
#include "Inventory/Data/InventorySystemSetting.h"

void UItemDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	const UInventorySystemSetting* Settings = GetDefault<UInventorySystemSetting>();
	
	if (!IsValid(Settings))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Item Settings are invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		
		return;
	}
	
	if (Settings->ItemDataTable.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : ItemDataTable is not configured"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		
		return;
	}
	
	ItemDataTable = Settings->ItemDataTable.LoadSynchronous();
	
	ensureMsgf(
		IsValid(ItemDataTable),
		TEXT("%s::%s: Failed to load ItemDataTable: %s"),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__),
		*Settings->ItemDataTable.ToSoftObjectPath().ToString());
	
}

void UItemDataSubsystem::Deinitialize()
{
	ItemDataTable = nullptr;
	
	Super::Deinitialize();
}

