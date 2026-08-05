#include "Inventory/Widget/InventoryDragDropOperation.h"

#include "Inventory/Components/PlayerInventoryComponent.h"

void UInventoryDragDropOperation::DragCancelled_Implementation(
	const FPointerEvent& PointerEvent)
{
	// A handled slot/window drop never reaches this path. An unhandled drop
	// outside the inventory window discards a concrete item, while the
	// component's quick-slot rule only clears its metadata assignment.
	if (Inventory && Inventory->IsInventoryOpen() &&
		!Inventory->IsPointInsideInventoryWindow(
			PointerEvent.GetScreenSpacePosition()))
	{
		Inventory->DropItem(SourceHandle);
	}

	Super::DragCancelled_Implementation(PointerEvent);
}
