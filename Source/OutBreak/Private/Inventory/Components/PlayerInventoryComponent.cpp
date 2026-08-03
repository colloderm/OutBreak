// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Components/PlayerInventoryComponent.h"

#include "Inventory/Data/WorldItem.h"
#include "Inventory/Subsystem/ItemDataSubsystem.h"
#include "Inventory/Widget/InventoryWindow.h"


UPlayerInventoryComponent::UPlayerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	InventoryBackPackArray.SetNum(FMath::Max(InventoryBackPackSize, 0));
	InventoryContrainerArray.SetNum(FMath::Max(InventoryContainerSize, 0));
	InventoryQuickSlotsArray.SetNum(FMath::Max(QuickSlotSize, 0));
}

FInventoryQueryResult UPlayerInventoryComponent::QueryHasItem(
	const FName QueryItemName) const
{
	FInventoryQueryResult Result;
	Result.QueryItemName = QueryItemName;

	if (QueryItemName.IsNone())
	{
		return Result;
	}

	for (int32 Index = 0; Index < InventoryBackPackArray.Num(); ++Index)
	{
		const FInventoryData& InventoryData = InventoryBackPackArray[Index];
		if (InventoryData.ItemName != QueryItemName ||
			InventoryData.ItemStack <= 0)
		{
			continue;
		}

		Result.BackpackIndices.Add(Index);
		Result.TotalStack += InventoryData.ItemStack;
	}

	for (int32 Index = 0; Index < InventoryContrainerArray.Num(); ++Index)
	{
		const FInventoryData& InventoryData = InventoryContrainerArray[Index];
		if (InventoryData.ItemName != QueryItemName ||
			InventoryData.ItemStack <= 0)
		{
			continue;
		}

		Result.ContainerIndices.Add(Index);
		Result.TotalStack += InventoryData.ItemStack;
	}

	Result.HasItem = Result.TotalStack > 0;
	return Result;
}

bool UPlayerInventoryComponent::QueryItemEnough(
	const FInventoryQueryResult& Result,
	const int32 QueryItemStack) const
{
	if (QueryItemStack <= 0 || Result.QueryItemName.IsNone())
	{
		return false;
	}

	if (!Result.HasItem ||
		(Result.BackpackIndices.IsEmpty() && Result.ContainerIndices.IsEmpty()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s::%s : Item \"%s\" was not found."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*Result.QueryItemName.ToString());
		return false;
	}

	if (Result.TotalStack < QueryItemStack)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s::%s : Item \"%s\" does not have enough stack (%d/%d)."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*Result.QueryItemName.ToString(),
			Result.TotalStack,
			QueryItemStack);
		return false;
	}

	return true;
}

void UPlayerInventoryComponent::ConsumeItem(
	const FInventoryQueryResult& Result,
	const int32 WantItemStack)
{
	if (WantItemStack <= 0 || Result.QueryItemName.IsNone())
	{
		return;
	}

	// Query results contain array indices, so refresh them before mutating data.
	const FInventoryQueryResult CurrentResult =
		QueryHasItem(Result.QueryItemName);
	if (!QueryItemEnough(CurrentResult, WantItemStack))
	{
		return;
	}

	int32 RemainWantItemStack = WantItemStack;
	auto ConsumeFromInventory =
		[&](TArray<FInventoryData>& Inventory, const TArray<int32>& Indices)
		{
			for (const int32 Index : Indices)
			{
				if (RemainWantItemStack <= 0)
				{
					break;
				}

				if (!Inventory.IsValidIndex(Index))
				{
					continue;
				}

				FInventoryData& Slot = Inventory[Index];
				if (Slot.ItemName != CurrentResult.QueryItemName ||
					Slot.ItemStack <= 0)
				{
					continue;
				}

				const int32 ConsumeStack =
					FMath::Min(RemainWantItemStack, Slot.ItemStack);
				Slot.ItemStack -= ConsumeStack;
				RemainWantItemStack -= ConsumeStack;

				if (Slot.ItemStack == 0)
				{
					Slot = FInventoryData();
				}
			}
		};

	ConsumeFromInventory(
		InventoryBackPackArray,
		CurrentResult.BackpackIndices);
	ConsumeFromInventory(
		InventoryContrainerArray,
		CurrentResult.ContainerIndices);

	ensureMsgf(
		RemainWantItemStack == 0,
		TEXT("%s::%s : Inventory changed while consuming item \"%s\"."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__),
		*CurrentResult.QueryItemName.ToString());

	UpdateInventoryWidget();
}

void UPlayerInventoryComponent::PickUpWorldItem(AWorldItem* WorldItem)
{
	if (!IsValid(WorldItem))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s::%s : WorldItem is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}

	FWorldItemData* ItemData = WorldItem->GetWorldItemData();
	if (AddItem(ItemData->ItemName, ItemData->ItemStack))
	{
		WorldItem->PickUpCompleted();
	}
}

void UPlayerInventoryComponent::SetInventoryBackPackSize(const int NewSize)
{
	InventoryBackPackSize = FMath::Max(NewSize, 0);
	UpdateInventory();
}

void UPlayerInventoryComponent::SetInventoryContainerSize(const int NewSize)
{
	InventoryContainerSize = FMath::Max(NewSize, 0);
	UpdateInventory();
}

void UPlayerInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateInventory();
}

void UPlayerInventoryComponent::UpdateInventory()
{
	if (InventoryBackPackArray.Num() != InventoryBackPackSize)
	{
		InventoryBackPackArray.SetNum(FMath::Max(InventoryBackPackSize, 0));
	}

	if (InventoryContrainerArray.Num() != InventoryContainerSize)
	{
		InventoryContrainerArray.SetNum(FMath::Max(InventoryContainerSize, 0));
	}

	if (InventoryQuickSlotsArray.Num() != QuickSlotSize)
	{
		InventoryQuickSlotsArray.SetNum(FMath::Max(QuickSlotSize, 0));
	}

	UpdateInventoryWidget();
}

void UPlayerInventoryComponent::UpdateInventoryWidget()
{
	if (!IsValid(InventoryWidget))
	{
		return;
	}

	InventoryWidget->SetInventoryArray(InventoryBackPackArray);
}

bool UPlayerInventoryComponent::AddItem(
	const FName ItemName,
	int32& ItemStack)
{
	if (ItemName.IsNone() || ItemStack < 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s::%s : Item name or stack is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return false;
	}

	if (ItemStack == 0)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : World is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!IsValid(GameInstance))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : GameInstance is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return false;
	}

	UItemDataSubsystem* ItemDataSubsystem =
		GameInstance->GetSubsystem<UItemDataSubsystem>();
	if (!IsValid(ItemDataSubsystem))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : ItemDataSubsystem is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return false;
	}

	const FItemMetaData* MetaData =
		ItemDataSubsystem->FindItemRow(
			ItemName,
			TEXT("UPlayerInventoryComponent::AddItem"));
	if (MetaData == nullptr || MetaData->MaxItemStack <= 0)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : Item metadata for \"%s\" is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*ItemName.ToString());
		return false;
	}

	const int32 MaxStack = MetaData->MaxItemStack;
	const int32 OriginalStack = ItemStack;
	int32 RemainingStack = ItemStack;

	auto FillExistingStacks =
		[&](TArray<FInventoryData>& Inventory)
		{
			for (FInventoryData& Slot : Inventory)
			{
				if (RemainingStack <= 0)
				{
					break;
				}

				if (Slot.ItemName != ItemName ||
					Slot.ItemStack <= 0 ||
					Slot.ItemStack >= MaxStack)
				{
					continue;
				}

				const int32 AddStack = FMath::Min(
					MaxStack - Slot.ItemStack,
					RemainingStack);
				Slot.ItemStack += AddStack;
				RemainingStack -= AddStack;
			}
		};

	auto FillEmptySlots =
		[&](TArray<FInventoryData>& Inventory)
		{
			for (FInventoryData& Slot : Inventory)
			{
				if (RemainingStack <= 0)
				{
					break;
				}

				if (!Slot.ItemName.IsNone() && Slot.ItemStack > 0)
				{
					continue;
				}

				const int32 AddStack =
					FMath::Min(MaxStack, RemainingStack);
				Slot = FInventoryData();
				Slot.ItemName = ItemName;
				Slot.ItemStack = AddStack;
				RemainingStack -= AddStack;
			}
		};

	FillExistingStacks(InventoryBackPackArray);
	FillExistingStacks(InventoryContrainerArray);
	FillEmptySlots(InventoryBackPackArray);
	FillEmptySlots(InventoryContrainerArray);

	ItemStack = RemainingStack;
	if (RemainingStack != OriginalStack)
	{
		UpdateInventoryWidget();
	}

	return RemainingStack == 0;
}

void UPlayerInventoryComponent::RemoveItem(const int RemoveIndex)
{
	if (!InventoryBackPackArray.IsValidIndex(RemoveIndex))
	{
		return;
	}

	InventoryBackPackArray[RemoveIndex] = FInventoryData();
	UpdateInventoryWidget();
}

void UPlayerInventoryComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
