// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Inventory/Data/InventoryData.h"

#include "ItemDataSubsystem.generated.h"


class UDataTable;

/**
 * 
 */

UCLASS()
class OUTBREAK_API UItemDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	virtual void Deinitialize() override;
	
	const UDataTable* GetItemDataTable() const
	{
		return ItemDataTable;
	}
	
	
	const FItemMetaData* FindItemRow(const FName RowName, const FString& ContextString = TEXT("ItemDataSubsystem")) const
	{
		if (!IsValid(ItemDataTable) || RowName.IsNone())
		{
			return nullptr;
		}
		
		return ItemDataTable->FindRow<FItemMetaData>(RowName, ContextString, true);
	}
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ItemDataTable;
};
