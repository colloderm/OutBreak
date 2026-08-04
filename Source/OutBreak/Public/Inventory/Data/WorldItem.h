// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/OBInteractableActor.h"
#include "Inventory/Data/InventoryData.h"
#include "WorldItem.generated.h"

class AOBPlayerController;

UCLASS()
class OUTBREAK_API AWorldItem : public AOBInteractableActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWorldItem();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	const FInventoryData& GetItemInstance() const { return ItemInstance; }
	const TArray<FInventoryData>& GetContainedInventory() const { return ContainedInventory; }
	bool HasItemInstance() const { return ItemInstance.ItemStack > 0 && ItemInstance.InstanceId.IsValid(); }

	void InitializeDroppedItem(
		const FInventoryData& InItemInstance,
		const TArray<FInventoryData>& InContainedInventory);

	virtual void Interact_Implementation(AOBPlayerController* PC) override;
	virtual FText GetInteractPromptText_Implementation() const override;



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void PickUpCompleted();
	
	friend class UPlayerInventoryComponent;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:
	// Editor-placed and dropped items use the same runtime representation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	FInventoryData ItemInstance;

	// Used by dropped backpacks. Empty for ordinary world items.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryData> ContainedInventory;
};
