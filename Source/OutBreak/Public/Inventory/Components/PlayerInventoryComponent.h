// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Inventory/Data/InventoryData.h"

#include "PlayerInventoryComponent.generated.h"


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
	
	void SetInventoryBackPackSize(int newSize);


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	
	void UpdateInventory();
	void UpdateInventoryWidget();
	
	
	void AddItem(FInventoryData Data);
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
