#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/Data/InventoryData.h"
#include "InventoryDragDropOperation.generated.h"

class UPlayerInventoryComponent;

/** Drag payload that carries a server-verifiable inventory item handle. */
UCLASS(BlueprintType)
class OUTBREAK_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	virtual void DragCancelled_Implementation(
		const FPointerEvent& PointerEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop")
	TObjectPtr<UPlayerInventoryComponent> Inventory;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|DragDrop")
	FInventoryItemHandle SourceHandle;
};
