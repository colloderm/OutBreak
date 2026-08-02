// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Inventory/Data/InventoryData.h"

#include "PlayerInventoryComponent.generated.h"


class AWorldItem;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UPlayerInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerInventoryComponent();
	
	
	FInventoryQueryResult QueryHasItem(const FName QueryItemName) const;
	bool QueryItemEnough(const FInventoryQueryResult& Result, int QueryItemStack);
	void ConsumeItem(const FInventoryQueryResult& Result, const int32 WantItemStack);
	
	void PickUpWorldItem(AWorldItem* WorldItem);
	
	void SetInventoryBackPackSize(int NewSize);
	void SetInventoryContainerSize(int NewSize);


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	
	void UpdateInventory();
	void UpdateInventoryWidget();
	
	
	/*
	 * @brief 현재 인벤토리에 지정된 아이템명과 아이템 개수만큼 추가를 시도합니다.
	 * @param ItemName : Code identification based on item data table
	 * @param ItemStack : Want Stack Number;
	 * @return bool : 현재 아이템 스택이 인벤토리에 전부 추가 됬으면 true 아니면 false를 반환 합니다. 
	 */
	bool AddItem(const FName ItemName, int& ItemStack);
	void RemoveItem(int RemoveIndex);

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInventoryWindow> InventoryWidget;
	
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	int InventoryBackPackSize = 20;
	
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	int InventoryContainerSize = 20;
	
	
	/* 고정 값 */
	int QuickSlotSize = 6;
	
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryData> InventoryBackPackArray;
	
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryData> InventoryContrainerArray;
	
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	TArray<FQuickSlotData> InventoryQuickSlotsArray;
	
};
