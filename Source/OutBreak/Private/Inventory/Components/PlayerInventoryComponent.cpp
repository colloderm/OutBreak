// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Components/PlayerInventoryComponent.h"

#include "Inventory/Subsystem/ItemDataSubsystem.h"
#include "Inventory/Widget/InventoryWindow.h"


// Sets default values for this component's properties
UPlayerInventoryComponent::UPlayerInventoryComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	
	UpdateInventory();
}

FInventoryQueryResult UPlayerInventoryComponent::QueryHasItem(
	const FName QueryItemName) const
{
	FInventoryQueryResult Result;

	Result.QueryItemName = QueryItemName;
	Result.TotalStack = 0;

	if (QueryItemName.IsNone())
	{
		return Result;
	}

	for (int32 Index = 0; Index < InventoryBackPackArray.Num(); ++Index)
	{
		const FInventoryData& InventoryData =
			InventoryBackPackArray[Index];

		if (InventoryData.ItemName != QueryItemName)
		{
			continue;
		}

		Result.BackpackIndices.Add(Index);
		Result.TotalStack += InventoryData.ItemStack;
	}

	for (int32 Index = 0; Index < InventoryContrainerArray.Num(); ++Index)
	{
		const FInventoryData& InventoryData =
			InventoryContrainerArray[Index];

		if (InventoryData.ItemName != QueryItemName)
		{
			continue;
		}

		Result.ContainerIndices.Add(Index);
		Result.TotalStack += InventoryData.ItemStack;
	}

	Result.HasItem = Result.TotalStack > 0;

	return Result;
}

bool UPlayerInventoryComponent::QueryItemEnough(const FInventoryQueryResult& Result, int QueryItemStack)
{
	if (!Result.HasItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s : Item \"%s\" do not Has."), *GetClass()->GetName(), TEXT(__FUNCTION__), *Result.QueryItemName.ToString());
		return false;
	}
	
	if (Result.TotalStack < QueryItemStack)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s : Item \"%s\" not enough stack."), *GetClass()->GetName(), TEXT(__FUNCTION__), *Result.QueryItemName.ToString());
		return false;
	}
	
	if (Result.BackpackIndices.Num() == 0 || Result.ContainerIndices.Num() == 0)
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Item \"%s\" Query Result is Fatal Error."), *GetClass()->GetName(), TEXT(__FUNCTION__), *Result.QueryItemName.ToString());
		return false;
	}
	
	return true;
	
}

void UPlayerInventoryComponent::ConsumeItem(
	const FInventoryQueryResult& Result,
	const int32 WantItemStack)
{
	if (WantItemStack <= 0)
	{
		return;
	}

	if (!QueryItemEnough(Result, WantItemStack))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : Item \"%s\" is not enough."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*Result.QueryItemName.ToString());

		return;
	}

	int32 RemainWantItemStack = WantItemStack;

	// 백팩에서 먼저 소비
	for (const int32 Index : Result.BackpackIndices)
	{
		if (RemainWantItemStack <= 0)
		{
			break;
		}

		if (!InventoryBackPackArray.IsValidIndex(Index))
		{
			continue;
		}

		auto& Slot = InventoryBackPackArray[Index];

		const int32 ConsumeStack =
			FMath::Min(RemainWantItemStack, Slot.ItemStack);

		Slot.ItemStack -= ConsumeStack;
		RemainWantItemStack -= ConsumeStack;
	}

	// 백팩에서 부족한 수량을 컨테이너에서 소비
	for (const int32 Index : Result.ContainerIndices)
	{
		if (RemainWantItemStack <= 0)
		{
			break;
		}

		if (!InventoryContrainerArray.IsValidIndex(Index))
		{
			continue;
		}

		auto& Slot = InventoryContrainerArray[Index];

		const int32 ConsumeStack =
			FMath::Min(RemainWantItemStack, Slot.ItemStack);

		Slot.ItemStack -= ConsumeStack;
		RemainWantItemStack -= ConsumeStack;
	}

	ensureMsgf(
		RemainWantItemStack == 0,
		TEXT("%s::%s : Inventory query result does not match current inventory state."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__));
}

void UPlayerInventoryComponent::SetInventoryBackPackSize(int newSize)
{
	InventoryBackPackSize = newSize;
	
	UpdateInventory();
}


// Called when the game starts
void UPlayerInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UPlayerInventoryComponent::UpdateInventory()
{
	if (InventoryBackPackArray.Num() != InventoryBackPackSize)
	{
		InventoryBackPackArray.SetNum(InventoryBackPackSize);
	}
	
	
	if (InventoryContrainerArray.Num() != InventoryContainerSize)
	{
		InventoryContrainerArray.SetNum(InventoryContainerSize);
	}
	
	
	if (InventoryQuickSlotsArray.Num() != QuickSlotSize)
	{
		InventoryQuickSlotsArray.SetNum(QuickSlotSize);	
	}
	
	
	UpdateInventoryWidget();
	
}

void UPlayerInventoryComponent::UpdateInventoryWidget()
{
	// 위젯 데이터 재갱신 처리 요청.
	InventoryWidget->Update();
}


// Called every frame
void UPlayerInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

