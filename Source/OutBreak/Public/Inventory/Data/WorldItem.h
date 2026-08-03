// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/Data/InventoryData.h"
#include "WorldItem.generated.h"

UCLASS()
class OUTBREAK_API AWorldItem : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWorldItem();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	FWorldItemData* GetWorldItemData() { return &ItemData; }
	const FInventoryData& GetItemInstance() const { return ItemInstance; }
	const TArray<FInventoryData>& GetContainedInventory() const { return ContainedInventory; }
	bool HasItemInstance() const { return ItemInstance.ItemStack > 0 && ItemInstance.InstanceId.IsValid(); }

	void InitializeDroppedItem(
		const FInventoryData& InItemInstance,
		const TArray<FInventoryData>& InContainedInventory);



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void PickUpCompleted();
	
	friend class UPlayerInventoryComponent;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	FWorldItemData ItemData;

	// Exact runtime instance preserved across drop/pickup (GUID, magazine, asset reference).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	FInventoryData ItemInstance;

	// Used by dropped backpacks. Empty for ordinary world items.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryData> ContainedInventory;
};
