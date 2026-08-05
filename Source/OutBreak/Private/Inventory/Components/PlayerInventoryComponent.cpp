// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Components/PlayerInventoryComponent.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Equipment/Data/OBEquipmentData.h"
#include "Inventory/Data/InventorySystemSetting.h"
#include "Inventory/Data/WorldItem.h"
#include "Inventory/Widget/InventoryWindow.h"
#include "Item/Data/OBItemDefinition.h"
#include "Item/OBItemRegistry.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Weapon/OBWeaponBase.h"


UPlayerInventoryComponent::UPlayerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	const UInventorySystemSetting* Settings =
		GetDefault<UInventorySystemSetting>();
	if (Settings)
	{
		InventoryBackPackSize = FMath::Max(
			0,
			Settings->FallbackBackpackSlotCount);
		QuickSlotSize = FMath::Max(
			0,
			Settings->DefaultQuickSlotCount);
	}

	InventoryBackPackArray.SetNum(FMath::Max(InventoryBackPackSize, 0));
	InventoryQuickSlotsArray.SetNum(FMath::Max(QuickSlotSize, 0));
	InitializeEquipmentSlots();
}

void UPlayerInventoryComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UPlayerInventoryComponent,
		InventoryBackPackArray,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UPlayerInventoryComponent,
		EquipmentSlots,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UPlayerInventoryComponent,
		InventoryQuickSlotsArray,
		COND_OwnerOnly);
	DOREPLIFETIME(UPlayerInventoryComponent, ActiveWeaponInstanceId);
	DOREPLIFETIME(UPlayerInventoryComponent, ActiveWeaponSlot);
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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (WorldItem->HasItemInstance())
	{
		EOBEquipmentSlot DroppedSlot = EOBEquipmentSlot::None;
		if (ResolveEquipmentSlot(WorldItem->GetItemInstance(), DroppedSlot) &&
			DroppedSlot == EOBEquipmentSlot::Backpack)
		{
			if (PickUpDroppedBackpack(WorldItem))
			{
				WorldItem->PickUpCompleted();
			}
			return;
		}

		if (PickUpDroppedItemInstance(WorldItem))
		{
			WorldItem->PickUpCompleted();
		}
		return;
	}

}

bool UPlayerInventoryComponent::PickUpDroppedItemInstance(
	AWorldItem* WorldItem)
{
	if (!WorldItem || !WorldItem->HasItemInstance() ||
		!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const FInventoryData& DroppedItem = WorldItem->GetItemInstance();
	const FOBItemDefinitionRow* ItemRow = DroppedItem.GetDefinition();
	if (!ItemRow || DroppedItem.ItemStack <= 0)
	{
		return false;
	}

	const int32 OriginalStack = DroppedItem.ItemStack;
	const int32 Moved = TryAddItemInstance(DroppedItem);
	if (Moved < OriginalStack)
	{
		FInventoryData RemainingItem = DroppedItem;
		RemainingItem.ItemStack = OriginalStack - Moved;
		const TArray<FInventoryData> EmptyContents;
		WorldItem->InitializeDroppedItem(
			RemainingItem,
			EmptyContents);
	}
	return Moved == OriginalStack;
}

bool UPlayerInventoryComponent::PickUpDroppedBackpack(AWorldItem* WorldItem)
{
	if (!WorldItem || !WorldItem->HasItemInstance())
	{
		return false;
	}

	FEquipmentSlotEntry* BackpackEntry =
		FindEquipmentSlot(EOBEquipmentSlot::Backpack);
	if (!BackpackEntry || !BackpackEntry->IsEmpty())
	{
		return false;
	}

	const FInventoryData& BackpackItem = WorldItem->GetItemInstance();
	const int32 Capacity = GetBackpackCapacity(BackpackItem);
	if (Capacity <= 0)
	{
		return false;
	}

	const TArray<FInventoryData>& DroppedContents =
		WorldItem->GetContainedInventory();
	for (int32 Index = Capacity; Index < DroppedContents.Num(); ++Index)
	{
		if (DroppedContents[Index].ItemStack > 0)
		{
			return false;
		}
	}

	// Preserve the dropped backpack's exact slot layout. Items left in the
	// component by a legacy/default-capacity transition are merged only into
	// empty slots so they cannot overwrite the backpack's own contents.
	TArray<FInventoryData> RestoredItems = DroppedContents;
	RestoredItems.SetNum(Capacity);
	for (const FInventoryData& ExistingItem : InventoryBackPackArray)
	{
		if (ExistingItem.ItemStack <= 0)
		{
			continue;
		}

		const int32 EmptyIndex = RestoredItems.IndexOfByPredicate(
			[](const FInventoryData& Item)
			{
				return Item.ItemStack <= 0;
			});
		if (EmptyIndex == INDEX_NONE)
		{
			return false;
		}
		RestoredItems[EmptyIndex] = ExistingItem;
	}

	BackpackEntry->Item = BackpackItem;
	InventoryBackPackArray = MoveTemp(RestoredItems);
	InventoryBackPackSize = Capacity;
	NotifyInventoryChanged();
	return true;
}

void UPlayerInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateInventory();
}

void UPlayerInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseInventory();
	UnbindActiveWeapon();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwapTimerHandle);
	}
	SetSwitching(false);
	Super::EndPlay(EndPlayReason);
}

UInventoryWindow* UPlayerInventoryComponent::OpenInventory()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	if (!IsValid(InventoryWidget))
	{
		const UInventorySystemSetting* Settings =
			GetDefault<UInventorySystemSetting>();
		if (!Settings || Settings->InventoryWindowWidgetClass.IsNull())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("%s::%s : InventoryWindowWidgetClass is not configured."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__));
			return nullptr;
		}

		TSubclassOf<UInventoryWindow> WindowClass =
			Settings->InventoryWindowWidgetClass.LoadSynchronous();
		if (!WindowClass)
		{
			return nullptr;
		}
		InventoryWidget = CreateWidget<UInventoryWindow>(
			PlayerController,
			WindowClass);
	}
	if (!IsValid(InventoryWidget))
	{
		return nullptr;
	}

	UpdateInventoryWidget();
	if (!InventoryWidget->IsInViewport())
	{
		const UInventorySystemSetting* Settings =
			GetDefault<UInventorySystemSetting>();
		InventoryWidget->AddToViewport(
			Settings ? Settings->InventoryWidgetZOrder : 0);
	}
	return InventoryWidget;
}

void UPlayerInventoryComponent::CloseInventory()
{
	if (IsValid(InventoryWidget) && InventoryWidget->IsInViewport())
	{
		InventoryWidget->RemoveFromParent();
	}
}

bool UPlayerInventoryComponent::IsInventoryOpen() const
{
	return IsValid(InventoryWidget) && InventoryWidget->IsInViewport();
}

void UPlayerInventoryComponent::UpdateInventory()
{
	if (InventoryBackPackArray.Num() != InventoryBackPackSize)
	{
		InventoryBackPackArray.SetNum(FMath::Max(InventoryBackPackSize, 0));
	}

	if (InventoryQuickSlotsArray.Num() != QuickSlotSize)
	{
		InventoryQuickSlotsArray.SetNum(FMath::Max(QuickSlotSize, 0));
	}
	InitializeEquipmentSlots();

	if (GetOwner() && GetOwner()->HasAuthority() &&
		ActiveWeaponInstanceId.IsValid() &&
		!FindEquippedItem(ActiveWeaponInstanceId))
	{
		UnequipWeapon();
	}

	NotifyInventoryChanged();
}

void UPlayerInventoryComponent::UpdateInventoryWidget()
{
	if (!IsValid(InventoryWidget))
	{
		return;
	}

	InventoryWidget->SetInventorySource(
		this,
		EInventoryItemLocation::Backpack,
		InventoryBackPackArray);
}

int32 UPlayerInventoryComponent::TryAddItem(
	const FGameplayTag& ItemTag,
	const int32 RequestedAmount)
{
	return AddItemRowInternal(
		UOBItemRegistry::FindItem(ItemTag),
		RequestedAmount);
}

int32 UPlayerInventoryComponent::TryAddItemInstance(
	const FInventoryData& ItemInstance)
{
	return AddItemRowInternal(
		ItemInstance.GetDefinition(),
		ItemInstance.ItemStack,
		nullptr,
		&ItemInstance);
}

bool UPlayerInventoryComponent::TryExtractItemInstance(
	const FGuid& InstanceId,
	FInventoryData& OutItemInstance)
{
	OutItemInstance = FInventoryData();
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !InstanceId.IsValid())
	{
		return false;
	}

	for (FInventoryData& Item : InventoryBackPackArray)
	{
		if (Item.InstanceId != InstanceId)
		{
			continue;
		}

		OutItemInstance = Item;
		Item = FInventoryData();
		NotifyInventoryChanged();
		const FOBItemDefinitionRow* ItemRow = OutItemInstance.GetDefinition();
		if (ItemRow && ItemRow->Category == EOBItemCategory::Ammo)
		{
			OnAmmoPoolChanged.Broadcast();
		}
		return true;
	}

	for (FEquipmentSlotEntry& Entry : EquipmentSlots)
	{
		if (Entry.Item.InstanceId != InstanceId)
		{
			continue;
		}
		if (Entry.Slot == EOBEquipmentSlot::Backpack &&
			InventoryBackPackArray.ContainsByPredicate(
				[](const FInventoryData& Item)
				{
					return Item.ItemStack > 0;
				}))
		{
			return false;
		}

		if (Entry.Item.InstanceId == ActiveWeaponInstanceId)
		{
			SyncActiveMagazine();
			UnequipWeapon();
		}
		OutItemInstance = Entry.Item;
		Entry.Item = FInventoryData();
		if (Entry.Slot == EOBEquipmentSlot::Backpack)
		{
			const UInventorySystemSetting* Settings =
				GetDefault<UInventorySystemSetting>();
			InventoryBackPackSize = Settings
				? FMath::Max(0, Settings->FallbackBackpackSlotCount)
				: 0;
			InventoryBackPackArray.SetNum(InventoryBackPackSize);
		}
		NotifyInventoryChanged();
		return true;
	}

	return false;
}

int32 UPlayerInventoryComponent::AddItemRowInternal(
	const FOBItemDefinitionRow* ItemRow,
	const int32 RequestedAmount,
	FGuid* OutFirstAddedInstanceId,
	const FInventoryData* SourceInstance)
{
	if (OutFirstAddedInstanceId)
	{
		OutFirstAddedInstanceId->Invalidate();
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return 0;
	}

	if (!ItemRow || !ItemRow->ItemTag.IsValid() ||
		ItemRow->MaxStack <= 0 || RequestedAmount <= 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s::%s : Item row or stack is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return 0;
	}

	const bool bWeapon = ItemRow->Category == EOBItemCategory::Weapon;
	const bool bEquipment = ItemRow->Category == EOBItemCategory::Equipment;
	const bool bEquippable = bWeapon || bEquipment;
	const int32 MaxStack = bEquippable ? 1 : ItemRow->MaxStack;
	int32 RemainingStack = RequestedAmount;
	bool bUsedSourceInstance = false;

	auto FillExistingStacks =
		[&](TArray<FInventoryData>& Inventory)
		{
			if (bEquippable)
			{
				return;
			}

			for (FInventoryData& Slot : Inventory)
			{
				if (RemainingStack <= 0)
				{
					break;
				}

				if (Slot.ItemTag != ItemRow->ItemTag ||
					Slot.ItemStack <= 0 || Slot.ItemStack >= MaxStack)
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

				if (Slot.ItemStack > 0)
				{
					continue;
				}

				const int32 AddStack = FMath::Min(MaxStack, RemainingStack);
				const bool bUseSource =
					bEquippable && SourceInstance && !bUsedSourceInstance;
				Slot = bUseSource ? *SourceInstance : FInventoryData();
				Slot.ItemTag = ItemRow->ItemTag;
				Slot.ItemStack = AddStack;
				if (!Slot.InstanceId.IsValid())
				{
					Slot.InstanceId = FGuid::NewGuid();
				}
				if (!bUseSource)
				{
					Slot.MagazineAmmo = -1;
				}
				bUsedSourceInstance = bUsedSourceInstance || bUseSource;
				if (OutFirstAddedInstanceId && !OutFirstAddedInstanceId->IsValid())
				{
					*OutFirstAddedInstanceId = Slot.InstanceId;
				}
				RemainingStack -= AddStack;
			}
		};

	FillExistingStacks(InventoryBackPackArray);
	FillEmptySlots(InventoryBackPackArray);

	const int32 AddedAmount = RequestedAmount - RemainingStack;
	if (AddedAmount > 0)
	{
		NotifyInventoryChanged();
		if (ItemRow->Category == EOBItemCategory::Ammo)
		{
			OnAmmoPoolChanged.Broadcast();
		}
	}

	return AddedAmount;
}

int32 UPlayerInventoryComponent::GetItemCount(const FGameplayTag& ItemTag) const
{
	if (!ItemTag.IsValid())
	{
		return 0;
	}

	int32 Total = 0;
	for (const FInventoryData& Slot : InventoryBackPackArray)
	{
		if (Slot.ItemTag == ItemTag && Slot.ItemStack > 0)
		{
			Total += Slot.ItemStack;
		}
	}
	return Total;
}

int32 UPlayerInventoryComponent::TryRemoveItem(
	const FGameplayTag& ItemTag,
	const int32 RequestedAmount)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!ItemTag.IsValid() || RequestedAmount <= 0)
	{
		return 0;
	}

	int32 Remaining = RequestedAmount;
	for (FInventoryData& Slot : InventoryBackPackArray)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Slot.ItemTag != ItemTag || Slot.ItemStack <= 0)
		{
			continue;
		}

		const int32 Removed = FMath::Min(Remaining, Slot.ItemStack);
		Slot.ItemStack -= Removed;
		Remaining -= Removed;
		if (Slot.ItemStack == 0)
		{
			Slot = FInventoryData();
		}
	}

	const int32 RemovedAmount = RequestedAmount - Remaining;
	if (RemovedAmount > 0)
	{
		NotifyInventoryChanged();
		const FOBItemDefinitionRow* ItemRow = UOBItemRegistry::FindItem(ItemTag);
		if (ItemRow && ItemRow->Category == EOBItemCategory::Ammo)
		{
			OnAmmoPoolChanged.Broadcast();
		}
	}
	return RemovedAmount;
}

void UPlayerInventoryComponent::GetLootableItemInstances(
	TArray<FInventoryData>& OutItems)
{
	OutItems.Reset();
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	SyncActiveMagazine();
	for (const FEquipmentSlotEntry& Entry : EquipmentSlots)
	{
		if (!Entry.IsEmpty())
		{
			OutItems.Add(Entry.Item);
		}
	}
	for (const FInventoryData& Item : InventoryBackPackArray)
	{
		if (Item.ItemTag.IsValid() && Item.ItemStack > 0)
		{
			OutItems.Add(Item);
		}
	}
}

void UPlayerInventoryComponent::ClearLootableItemInstances()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	UnequipWeapon();
	for (FEquipmentSlotEntry& Entry : EquipmentSlots)
	{
		Entry.Item = FInventoryData();
	}
	for (FQuickSlotData& QuickSlot : InventoryQuickSlotsArray)
	{
		QuickSlot.ItemTag = FGameplayTag();
	}

	const UInventorySystemSetting* Settings =
		GetDefault<UInventorySystemSetting>();
	InventoryBackPackSize = Settings
		? FMath::Max(0, Settings->FallbackBackpackSlotCount)
		: 0;
	InventoryBackPackArray.Reset();
	InventoryBackPackArray.SetNum(InventoryBackPackSize);
	NotifyInventoryChanged();
	OnAmmoPoolChanged.Broadcast();
}

int32 UPlayerInventoryComponent::GetAmmo(const FGameplayTag& AmmoType) const
{
	return GetItemCount(AmmoType);
}

void UPlayerInventoryComponent::AddAmmo(
	const FGameplayTag& AmmoType,
	const int32 Amount)
{
	TryAddItem(AmmoType, Amount);
}

int32 UPlayerInventoryComponent::ConsumeAmmoFromPool(
	const FGameplayTag& AmmoType,
	const int32 Amount)
{
	return TryRemoveItem(AmmoType, Amount);
}

bool UPlayerInventoryComponent::EquipStartingBackpack(
	const FGameplayTag& BackpackItemTag)
{
	AActor* OwnerActor = GetOwner();
	FEquipmentSlotEntry* BackpackEntry =
		FindEquipmentSlot(EOBEquipmentSlot::Backpack);
	const FOBItemDefinitionRow* BackpackRow =
		UOBItemRegistry::FindItem(BackpackItemTag);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !BackpackRow ||
		!BackpackEntry || !BackpackEntry->IsEmpty())
	{
		return false;
	}

	FInventoryData BackpackItem;
	BackpackItem.ItemTag = BackpackRow->ItemTag;
	BackpackItem.ItemStack = 1;
	BackpackItem.InstanceId = FGuid::NewGuid();

	EOBEquipmentSlot RequiredSlot = EOBEquipmentSlot::None;
	if (!ResolveEquipmentSlot(BackpackItem, RequiredSlot) ||
		RequiredSlot != EOBEquipmentSlot::Backpack)
	{
		return false;
	}

	const int32 Capacity = GetBackpackCapacity(BackpackItem);
	if (Capacity <= 0 || !CanSetBackpackCapacity(Capacity, INDEX_NONE))
	{
		return false;
	}

	BackpackEntry->Item = MoveTemp(BackpackItem);
	CompactBackpack(Capacity);
	NotifyInventoryChanged();
	return true;
}

bool UPlayerInventoryComponent::AddWeapon(
	TSubclassOf<AOBWeaponBase> WeaponClass)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !WeaponClass)
	{
		return false;
	}

	const FGameplayTag ItemTag =
		UOBItemRegistry::FindTagForWeaponClass(WeaponClass.Get());
	const FOBItemDefinitionRow* ItemRow =
		UOBItemRegistry::FindItem(ItemTag);
	if (!ItemRow)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : 무기 %s 에 대응하는 DT_Items 행이 없다."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(WeaponClass.Get()));
		return false;
	}

	TSubclassOf<AOBWeaponBase> ResolvedClass;
	const UOBWeaponData* WeaponData = nullptr;
	if (!ResolveWeaponDefinition(ItemRow, ResolvedClass, WeaponData) ||
		ResolvedClass != WeaponClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : 아이템 %s 이(가) 무기 %s 로 해석되지 않는다."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*ItemRow->ItemTag.ToString(),
			*GetNameSafe(WeaponClass.Get()));
		return false;
	}

	FGuid AddedWeaponId;
	if (AddItemRowInternal(ItemRow, 1, &AddedWeaponId) != 1 ||
		!AddedWeaponId.IsValid())
	{
		return false;
	}

	if (!AssignItemToEquipmentSlot(
		AddedWeaponId,
		ToEquipmentSlot(WeaponData->WeaponSlot)))
	{
		return false;
	}

	if (WeaponData && WeaponData->AmmoType.IsValid() &&
		WeaponData->MaxReserveAmmo > 0)
	{
		AddAmmo(WeaponData->AmmoType, WeaponData->MaxReserveAmmo);
	}
	return true;
}

bool UPlayerInventoryComponent::ResolveWeaponDefinition(
	const FOBItemDefinitionRow* ItemRow,
	TSubclassOf<AOBWeaponBase>& OutWeaponClass,
	const UOBWeaponData*& OutWeaponData) const
{
	OutWeaponClass = nullptr;
	OutWeaponData = nullptr;
	if (!ItemRow || ItemRow->Category != EOBItemCategory::Weapon ||
		ItemRow->WeaponClass.IsNull())
	{
		return false;
	}

	UClass* LoadedClass = ItemRow->WeaponClass.LoadSynchronous();
	if (!LoadedClass || !LoadedClass->IsChildOf(AOBWeaponBase::StaticClass()))
	{
		return false;
	}

	AOBWeaponBase* WeaponCDO = LoadedClass->GetDefaultObject<AOBWeaponBase>();
	if (!WeaponCDO || !WeaponCDO->GetWeaponData())
	{
		return false;
	}

	OutWeaponClass = LoadedClass;
	OutWeaponData = WeaponCDO->GetWeaponData();
	return true;
}

EOBEquipmentSlot UPlayerInventoryComponent::ToEquipmentSlot(
	const EOBWeaponSlot WeaponSlot)
{
	switch (WeaponSlot)
	{
	case EOBWeaponSlot::Primary:
		return EOBEquipmentSlot::PrimaryWeapon;
	case EOBWeaponSlot::Secondary:
		return EOBEquipmentSlot::SecondaryWeapon;
	case EOBWeaponSlot::Melee:
		return EOBEquipmentSlot::MeleeWeapon;
	default:
		return EOBEquipmentSlot::None;
	}
}

bool UPlayerInventoryComponent::ResolveEquipmentSlot(
	const FInventoryData& Item,
	EOBEquipmentSlot& OutEquipmentSlot) const
{
	OutEquipmentSlot = EOBEquipmentSlot::None;
	const FOBItemDefinitionRow* ItemRow = Item.GetDefinition();
	if (!ItemRow || Item.ItemStack <= 0)
	{
		return false;
	}

	TSubclassOf<AOBWeaponBase> WeaponClass;
	const UOBWeaponData* WeaponData = nullptr;
	if (ResolveWeaponDefinition(ItemRow, WeaponClass, WeaponData))
	{
		OutEquipmentSlot = ToEquipmentSlot(WeaponData->WeaponSlot);
		return OutEquipmentSlot != EOBEquipmentSlot::None;
	}

	if (ItemRow->Category == EOBItemCategory::Equipment &&
		ItemRow->EquipmentData)
	{
		OutEquipmentSlot = ItemRow->EquipmentData->EquipmentSlot;
		return OutEquipmentSlot != EOBEquipmentSlot::None;
	}
	return false;
}

void UPlayerInventoryComponent::InitializeEquipmentSlots()
{
	for (uint8 Value = static_cast<uint8>(EOBEquipmentSlot::None) + 1;
		Value < static_cast<uint8>(EOBEquipmentSlot::MAX);
		++Value)
	{
		const EOBEquipmentSlot Slot = static_cast<EOBEquipmentSlot>(Value);
		if (!FindEquipmentSlot(Slot))
		{
			FEquipmentSlotEntry& Entry = EquipmentSlots.AddDefaulted_GetRef();
			Entry.Slot = Slot;
		}
	}
}

FEquipmentSlotEntry* UPlayerInventoryComponent::FindEquipmentSlot(
	const EOBEquipmentSlot EquipmentSlot)
{
	return EquipmentSlots.FindByPredicate(
		[EquipmentSlot](const FEquipmentSlotEntry& Entry)
		{
			return Entry.Slot == EquipmentSlot;
		});
}

const FEquipmentSlotEntry* UPlayerInventoryComponent::FindEquipmentSlot(
	const EOBEquipmentSlot EquipmentSlot) const
{
	return EquipmentSlots.FindByPredicate(
		[EquipmentSlot](const FEquipmentSlotEntry& Entry)
		{
			return Entry.Slot == EquipmentSlot;
		});
}

FEquipmentSlotEntry* UPlayerInventoryComponent::FindEquipmentSlotByItem(
	const FGuid& InstanceId)
{
	return EquipmentSlots.FindByPredicate(
		[&InstanceId](const FEquipmentSlotEntry& Entry)
		{
			return InstanceId.IsValid() && Entry.Item.InstanceId == InstanceId;
		});
}

FInventoryData* UPlayerInventoryComponent::FindEquippedItem(
	const FGuid& InstanceId)
{
	FEquipmentSlotEntry* Entry = EquipmentSlots.FindByPredicate(
		[&InstanceId](const FEquipmentSlotEntry& Candidate)
		{
			return InstanceId.IsValid() &&
				Candidate.Item.InstanceId == InstanceId;
		});
	return Entry ? &Entry->Item : nullptr;
}

const FInventoryData* UPlayerInventoryComponent::FindEquippedItem(
	const FGuid& InstanceId) const
{
	const FEquipmentSlotEntry* Entry = EquipmentSlots.FindByPredicate(
		[&InstanceId](const FEquipmentSlotEntry& Candidate)
		{
			return InstanceId.IsValid() &&
				Candidate.Item.InstanceId == InstanceId;
		});
	return Entry ? &Entry->Item : nullptr;
}

FInventoryData* UPlayerInventoryComponent::FindBackpackItem(
	const FGuid& InstanceId)
{
	return InventoryBackPackArray.FindByPredicate(
		[&InstanceId](const FInventoryData& Item)
		{
			return InstanceId.IsValid() && Item.InstanceId == InstanceId;
		});
}

const FInventoryData* UPlayerInventoryComponent::FindBackpackItem(
	const FGuid& InstanceId) const
{
	return InventoryBackPackArray.FindByPredicate(
		[&InstanceId](const FInventoryData& Item)
		{
			return InstanceId.IsValid() && Item.InstanceId == InstanceId;
		});
}

FInventoryData* UPlayerInventoryComponent::FindItemInInventory(
	const EInventoryItemLocation Location,
	const FGuid& InstanceId,
	const int32 IndexHint)
{
	TArray<FInventoryData>* Inventory = nullptr;
	if (Location == EInventoryItemLocation::Backpack)
	{
		Inventory = &InventoryBackPackArray;
	}
	if (!Inventory || !InstanceId.IsValid())
	{
		return nullptr;
	}

	if (Inventory->IsValidIndex(IndexHint) &&
		(*Inventory)[IndexHint].InstanceId == InstanceId)
	{
		return &(*Inventory)[IndexHint];
	}

	return Inventory->FindByPredicate(
		[&InstanceId](const FInventoryData& Item)
		{
			return Item.InstanceId == InstanceId;
		});
}

const FInventoryData* UPlayerInventoryComponent::FindItemInInventory(
	const EInventoryItemLocation Location,
	const FGuid& InstanceId,
	const int32 IndexHint) const
{
	const TArray<FInventoryData>* Inventory = nullptr;
	if (Location == EInventoryItemLocation::Backpack)
	{
		Inventory = &InventoryBackPackArray;
	}
	if (!Inventory || !InstanceId.IsValid())
	{
		return nullptr;
	}

	if (Inventory->IsValidIndex(IndexHint) &&
		(*Inventory)[IndexHint].InstanceId == InstanceId)
	{
		return &(*Inventory)[IndexHint];
	}

	return Inventory->FindByPredicate(
		[&InstanceId](const FInventoryData& Item)
		{
			return Item.InstanceId == InstanceId;
		});
}

FInventoryItemHandle UPlayerInventoryComponent::MakeBackpackHandle(
	const int32 BackpackIndex) const
{
	FInventoryItemHandle Handle;
	Handle.Location = EInventoryItemLocation::Backpack;
	Handle.SlotIndex = BackpackIndex;
	if (InventoryBackPackArray.IsValidIndex(BackpackIndex))
	{
		Handle.InstanceId = InventoryBackPackArray[BackpackIndex].InstanceId;
		Handle.ItemTag = InventoryBackPackArray[BackpackIndex].ItemTag;
	}
	return Handle;
}

FInventoryItemHandle UPlayerInventoryComponent::MakeEquipmentHandle(
	const EOBEquipmentSlot EquipmentSlot) const
{
	FInventoryItemHandle Handle;
	Handle.Location = EInventoryItemLocation::Equipment;
	Handle.EquipmentSlot = EquipmentSlot;
	if (const FEquipmentSlotEntry* Entry = FindEquipmentSlot(EquipmentSlot))
	{
		Handle.InstanceId = Entry->Item.InstanceId;
		Handle.ItemTag = Entry->Item.ItemTag;
	}
	return Handle;
}

FInventoryItemHandle UPlayerInventoryComponent::MakeQuickSlotHandle(
	const int32 QuickSlotIndex) const
{
	FInventoryItemHandle Handle;
	Handle.Location = EInventoryItemLocation::QuickSlot;
	Handle.SlotIndex = QuickSlotIndex;
	if (InventoryQuickSlotsArray.IsValidIndex(QuickSlotIndex))
	{
		Handle.ItemTag = InventoryQuickSlotsArray[QuickSlotIndex].ItemTag;
	}
	return Handle;
}

bool UPlayerInventoryComponent::GetItemFromHandle(
	const FInventoryItemHandle& Handle,
	FInventoryData& OutItem) const
{
	if (Handle.Location == EInventoryItemLocation::QuickSlot)
	{
		FQuickSlotData QuickSlot;
		if (!GetQuickSlot(Handle.SlotIndex, QuickSlot) ||
			!QuickSlot.ItemTag.IsValid())
		{
			OutItem = FInventoryData();
			return false;
		}

		OutItem = FInventoryData();
		OutItem.ItemTag = QuickSlot.ItemTag;
		OutItem.ItemStack = GetItemCount(QuickSlot.ItemTag);
		return true;
	}

	const FInventoryData* Item = nullptr;
	if (Handle.Location == EInventoryItemLocation::Equipment)
	{
		const FEquipmentSlotEntry* Entry = FindEquipmentSlot(Handle.EquipmentSlot);
		if (Entry && Entry->Item.InstanceId == Handle.InstanceId)
		{
			Item = &Entry->Item;
		}
	}
	else
	{
		Item = FindItemInInventory(
			Handle.Location,
			Handle.InstanceId,
			Handle.SlotIndex);
	}

	if (!Item)
	{
		OutItem = FInventoryData();
		return false;
	}
	OutItem = *Item;
	return true;
}

bool UPlayerInventoryComponent::GetQuickSlot(
	const int32 QuickSlotIndex,
	FQuickSlotData& OutQuickSlot) const
{
	if (!InventoryQuickSlotsArray.IsValidIndex(QuickSlotIndex))
	{
		OutQuickSlot = FQuickSlotData();
		return false;
	}
	OutQuickSlot = InventoryQuickSlotsArray[QuickSlotIndex];
	return OutQuickSlot.IsAssigned();
}

int32 UPlayerInventoryComponent::GetQuickSlotItemCount(
	const int32 QuickSlotIndex) const
{
	if (!InventoryQuickSlotsArray.IsValidIndex(QuickSlotIndex) ||
		!InventoryQuickSlotsArray[QuickSlotIndex].ItemTag.IsValid())
	{
		return 0;
	}
	return GetItemCount(InventoryQuickSlotsArray[QuickSlotIndex].ItemTag);
}

void UPlayerInventoryComponent::AssignQuickSlot(
	const int32 QuickSlotIndex,
	const FInventoryItemHandle& Source)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_AssignQuickSlot(QuickSlotIndex, Source);
		return;
	}
	AssignQuickSlotInternal(QuickSlotIndex, Source);
}

void UPlayerInventoryComponent::Server_AssignQuickSlot_Implementation(
	const int32 QuickSlotIndex,
	FInventoryItemHandle Source)
{
	AssignQuickSlotInternal(QuickSlotIndex, Source);
}

bool UPlayerInventoryComponent::AssignQuickSlotInternal(
	const int32 QuickSlotIndex,
	const FInventoryItemHandle& Source)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		!InventoryQuickSlotsArray.IsValidIndex(QuickSlotIndex) ||
		Source.Location == EInventoryItemLocation::QuickSlot)
	{
		return false;
	}

	FInventoryData Item;
	if (!GetItemFromHandle(Source, Item))
	{
		return false;
	}

	// 퀵슬롯은 "사용 가능한 아이템 종류"만 가리킨다. 사용 어빌리티가 없으면 등록 불가.
	const FOBItemDefinitionRow* ItemRow = Item.GetDefinition();
	if (!ItemRow || !ItemRow->ItemTag.IsValid() || !ItemRow->UseAbility)
	{
		return false;
	}

	InventoryQuickSlotsArray[QuickSlotIndex].ItemTag = ItemRow->ItemTag;
	NotifyInventoryChanged();
	return true;
}

void UPlayerInventoryComponent::ClearQuickSlot(const int32 QuickSlotIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_ClearQuickSlot(QuickSlotIndex);
		return;
	}
	ClearQuickSlotInternal(QuickSlotIndex);
}

void UPlayerInventoryComponent::Server_ClearQuickSlot_Implementation(
	const int32 QuickSlotIndex)
{
	ClearQuickSlotInternal(QuickSlotIndex);
}

bool UPlayerInventoryComponent::ClearQuickSlotInternal(
	const int32 QuickSlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		!InventoryQuickSlotsArray.IsValidIndex(QuickSlotIndex) ||
		!InventoryQuickSlotsArray[QuickSlotIndex].IsAssigned())
	{
		return false;
	}

	InventoryQuickSlotsArray[QuickSlotIndex] = FQuickSlotData();
	NotifyInventoryChanged();
	return true;
}

void UPlayerInventoryComponent::UseQuickSlot(const int32 QuickSlotIndex)
{
	UseQuickSlotInternal(QuickSlotIndex);
}

bool UPlayerInventoryComponent::UseQuickSlotInternal(
	const int32 QuickSlotIndex)
{
	if (!GetOwner() ||
		!InventoryQuickSlotsArray.IsValidIndex(QuickSlotIndex))
	{
		return false;
	}

	const FOBItemDefinitionRow* ItemRow = UOBItemRegistry::FindItem(
		InventoryQuickSlotsArray[QuickSlotIndex].ItemTag);
	if (!ItemRow || !ItemRow->ItemTag.IsValid() ||
		!ItemRow->UseAbility ||
		GetItemCount(ItemRow->ItemTag) <= 0)
	{
		return false;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	return ASC && ASC->TryActivateAbilityByClass(ItemRow->UseAbility);
}

bool UPlayerInventoryComponent::GetEquippedItem(
	const EOBEquipmentSlot EquipmentSlot,
	FInventoryData& OutItem) const
{
	return GetItemFromHandle(MakeEquipmentHandle(EquipmentSlot), OutItem);
}

bool UPlayerInventoryComponent::IsItemEquipped(
	const FGuid InstanceId,
	EOBEquipmentSlot& OutEquipmentSlot) const
{
	OutEquipmentSlot = EOBEquipmentSlot::None;
	if (!InstanceId.IsValid())
	{
		return false;
	}

	for (const FEquipmentSlotEntry& Entry : EquipmentSlots)
	{
		if (Entry.Item.InstanceId == InstanceId)
		{
			OutEquipmentSlot = Entry.Slot;
			return true;
		}
	}
	return false;
}

bool UPlayerInventoryComponent::AssignItemToEquipmentSlot(
	const FGuid& InstanceId,
	const EOBEquipmentSlot EquipmentSlot)
{
	return MoveInventoryItemToEquipment(
		EInventoryItemLocation::Backpack,
		InstanceId,
		INDEX_NONE,
		EquipmentSlot);
}

int32 UPlayerInventoryComponent::GetBackpackCapacity(
	const FInventoryData& BackpackItem) const
{
	const FOBItemDefinitionRow* BackpackRow = BackpackItem.GetDefinition();
	if (!BackpackRow ||
		BackpackRow->Category != EOBItemCategory::Equipment ||
		!BackpackRow->EquipmentData ||
		BackpackRow->EquipmentData->EquipmentSlot != EOBEquipmentSlot::Backpack)
	{
		return 0;
	}
	return FMath::Max(0, BackpackRow->EquipmentData->BackpackSlotCount);
}

bool UPlayerInventoryComponent::CanSetBackpackCapacity(
	const int32 NewCapacity,
	const int32 ReplacedItemIndex) const
{
	int32 OccupiedAfterSwap = 0;
	for (int32 Index = 0; Index < InventoryBackPackArray.Num(); ++Index)
	{
		if (InventoryBackPackArray[Index].ItemStack > 0 &&
			Index != ReplacedItemIndex)
		{
			++OccupiedAfterSwap;
		}
	}

	const FEquipmentSlotEntry* CurrentBackpack =
		FindEquipmentSlot(EOBEquipmentSlot::Backpack);
	if (ReplacedItemIndex != INDEX_NONE &&
		CurrentBackpack && !CurrentBackpack->IsEmpty())
	{
		++OccupiedAfterSwap;
	}
	return OccupiedAfterSwap <= FMath::Max(0, NewCapacity);
}

void UPlayerInventoryComponent::CompactBackpack(const int32 NewCapacity)
{
	TArray<FInventoryData> Compacted;
	Compacted.Reserve(FMath::Max(0, NewCapacity));
	for (FInventoryData& Item : InventoryBackPackArray)
	{
		if (Item.ItemStack > 0)
		{
			Compacted.Add(MoveTemp(Item));
		}
	}
	Compacted.SetNum(FMath::Max(0, NewCapacity));
	InventoryBackPackArray = MoveTemp(Compacted);
	InventoryBackPackSize = FMath::Max(0, NewCapacity);
}

bool UPlayerInventoryComponent::MoveInventoryItemToEquipment(
	const EInventoryItemLocation SourceLocation,
	const FGuid& InstanceId,
	const int32 IndexHint,
	const EOBEquipmentSlot EquipmentSlot)
{
	AActor* OwnerActor = GetOwner();
	FInventoryData* SourceItem = FindItemInInventory(
		SourceLocation,
		InstanceId,
		IndexHint);
	FEquipmentSlotEntry* TargetEntry = FindEquipmentSlot(EquipmentSlot);
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!SourceItem || !TargetEntry)
	{
		return false;
	}

	EOBEquipmentSlot RequiredSlot = EOBEquipmentSlot::None;
	if (!ResolveEquipmentSlot(*SourceItem, RequiredSlot) ||
		RequiredSlot != EquipmentSlot)
	{
		return false;
	}

	int32 SourceBackpackIndex = INDEX_NONE;
	if (SourceLocation == EInventoryItemLocation::Backpack)
	{
		SourceBackpackIndex = InventoryBackPackArray.IndexOfByPredicate(
			[&InstanceId](const FInventoryData& Item)
			{
				return Item.InstanceId == InstanceId;
			});
	}

	if (EquipmentSlot == EOBEquipmentSlot::Backpack)
	{
		const int32 NewCapacity = GetBackpackCapacity(*SourceItem);
		if (NewCapacity <= 0 ||
			!CanSetBackpackCapacity(NewCapacity, SourceBackpackIndex))
		{
			return false;
		}
	}

	if (TargetEntry->Item.InstanceId == ActiveWeaponInstanceId)
	{
		UnequipWeapon();
	}

	Swap(TargetEntry->Item, *SourceItem);
	if (EquipmentSlot == EOBEquipmentSlot::Backpack)
	{
		CompactBackpack(GetBackpackCapacity(TargetEntry->Item));
	}
	NotifyInventoryChanged();
	return true;
}

bool UPlayerInventoryComponent::MoveEquipmentItemToInventory(
	const EOBEquipmentSlot EquipmentSlot,
	const EInventoryItemLocation TargetLocation,
	int32 TargetIndex)
{
	AActor* OwnerActor = GetOwner();
	FEquipmentSlotEntry* SourceEntry = FindEquipmentSlot(EquipmentSlot);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !SourceEntry ||
		SourceEntry->IsEmpty() || EquipmentSlot == EOBEquipmentSlot::Backpack)
	{
		return false;
	}

	TArray<FInventoryData>* TargetArray = nullptr;
	if (TargetLocation == EInventoryItemLocation::Backpack)
	{
		TargetArray = &InventoryBackPackArray;
	}
	if (!TargetArray)
	{
		return false;
	}

	if (!TargetArray->IsValidIndex(TargetIndex))
	{
		TargetIndex = TargetArray->IndexOfByPredicate(
			[](const FInventoryData& Item)
			{
				return Item.ItemStack <= 0;
			});
	}
	if (!TargetArray->IsValidIndex(TargetIndex))
	{
		return false;
	}

	FInventoryData& TargetItem = (*TargetArray)[TargetIndex];
	if (TargetItem.ItemStack > 0)
	{
		EOBEquipmentSlot TargetRequiredSlot = EOBEquipmentSlot::None;
		if (!ResolveEquipmentSlot(TargetItem, TargetRequiredSlot) ||
			TargetRequiredSlot != EquipmentSlot)
		{
			return false;
		}
	}

	if (SourceEntry->Item.InstanceId == ActiveWeaponInstanceId)
	{
		UnequipWeapon();
	}
	Swap(SourceEntry->Item, TargetItem);
	NotifyInventoryChanged();
	return true;
}

bool UPlayerInventoryComponent::ClearEquipmentSlot(
	const EOBEquipmentSlot EquipmentSlot)
{
	AActor* OwnerActor = GetOwner();
	FEquipmentSlotEntry* Entry = FindEquipmentSlot(EquipmentSlot);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !Entry || Entry->IsEmpty())
	{
		return false;
	}

	if (Entry->Item.InstanceId == ActiveWeaponInstanceId)
	{
		UnequipWeapon();
	}
	Entry->Item = FInventoryData();
	NotifyInventoryChanged();
	return true;
}

void UPlayerInventoryComponent::MoveItem(
	const FInventoryItemHandle& Source,
	const FInventoryItemHandle& Target)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_MoveItem(Source, Target);
		return;
	}
	MoveItemInternal(Source, Target);
}

void UPlayerInventoryComponent::Server_MoveItem_Implementation(
	FInventoryItemHandle Source,
	FInventoryItemHandle Target)
{
	MoveItemInternal(Source, Target);
}

bool UPlayerInventoryComponent::MoveItemInternal(
	const FInventoryItemHandle& Source,
	const FInventoryItemHandle& Target)
{
	if (!Source.HasItem())
	{
		return false;
	}

	if (Target.Location == EInventoryItemLocation::QuickSlot)
	{
		if (!InventoryQuickSlotsArray.IsValidIndex(Target.SlotIndex))
		{
			return false;
		}

		if (Source.Location != EInventoryItemLocation::QuickSlot)
		{
			return AssignQuickSlotInternal(Target.SlotIndex, Source);
		}

		if (!InventoryQuickSlotsArray.IsValidIndex(Source.SlotIndex) ||
			InventoryQuickSlotsArray[Source.SlotIndex].ItemTag !=
				Source.ItemTag)
		{
			return false;
		}
		if (Source.SlotIndex == Target.SlotIndex)
		{
			return true;
		}

		Swap(
			InventoryQuickSlotsArray[Source.SlotIndex],
			InventoryQuickSlotsArray[Target.SlotIndex]);
		NotifyInventoryChanged();
		return true;
	}

	// A quick slot is only a type reference. It cannot replace a concrete
	// inventory/equipment slot item.
	if (Source.Location == EInventoryItemLocation::QuickSlot)
	{
		return false;
	}

	if (Target.Location == EInventoryItemLocation::Equipment)
	{
		if (Source.Location == EInventoryItemLocation::Equipment)
		{
			FEquipmentSlotEntry* SourceEntry =
				FindEquipmentSlot(Source.EquipmentSlot);
			FEquipmentSlotEntry* TargetEntry =
				FindEquipmentSlot(Target.EquipmentSlot);
			if (!SourceEntry || !TargetEntry ||
				SourceEntry->Item.InstanceId != Source.InstanceId)
			{
				return false;
			}
			if (SourceEntry == TargetEntry)
			{
				return true;
			}

			EOBEquipmentSlot SourceRequiredSlot = EOBEquipmentSlot::None;
			if (!ResolveEquipmentSlot(SourceEntry->Item, SourceRequiredSlot) ||
				SourceRequiredSlot != Target.EquipmentSlot)
			{
				return false;
			}
			if (!TargetEntry->IsEmpty())
			{
				EOBEquipmentSlot TargetRequiredSlot = EOBEquipmentSlot::None;
				if (!ResolveEquipmentSlot(TargetEntry->Item, TargetRequiredSlot) ||
					TargetRequiredSlot != Source.EquipmentSlot)
				{
					return false;
				}
			}

			if (SourceEntry->Item.InstanceId == ActiveWeaponInstanceId ||
				TargetEntry->Item.InstanceId == ActiveWeaponInstanceId)
			{
				UnequipWeapon();
			}
			Swap(SourceEntry->Item, TargetEntry->Item);
			NotifyInventoryChanged();
			return true;
		}

		if (Source.Location != EInventoryItemLocation::Backpack)
		{
			return false;
		}
		FInventoryData* Item = FindItemInInventory(
			Source.Location,
			Source.InstanceId,
			Source.SlotIndex);
		EOBEquipmentSlot RequiredSlot = EOBEquipmentSlot::None;
		if (!Item || !ResolveEquipmentSlot(*Item, RequiredSlot) ||
			RequiredSlot != Target.EquipmentSlot)
		{
			return false;
		}

		const bool bMoved = MoveInventoryItemToEquipment(
			Source.Location,
			Source.InstanceId,
			Source.SlotIndex,
			Target.EquipmentSlot);
		if (!bMoved)
		{
			return false;
		}

		const FEquipmentSlotEntry* Equipped =
			FindEquipmentSlot(Target.EquipmentSlot);
		if (Equipped)
		{
			TSubclassOf<AOBWeaponBase> WeaponClass;
			const UOBWeaponData* WeaponData = nullptr;
			if (ResolveWeaponDefinition(
				Equipped->Item.GetDefinition(),
				WeaponClass,
				WeaponData))
			{
				EquipSlot(WeaponData->WeaponSlot);
			}
		}
		return true;
	}

	if (Source.Location == EInventoryItemLocation::Equipment &&
		Target.Location == EInventoryItemLocation::Backpack)
	{
		const FEquipmentSlotEntry* Entry = FindEquipmentSlot(Source.EquipmentSlot);
		if (!Entry || Entry->Item.InstanceId != Source.InstanceId ||
			!MoveEquipmentItemToInventory(
				Source.EquipmentSlot,
				Target.Location,
				Target.SlotIndex))
		{
			return false;
		}

		Entry = FindEquipmentSlot(Source.EquipmentSlot);
		if (Entry && !Entry->IsEmpty())
		{
			TSubclassOf<AOBWeaponBase> WeaponClass;
			const UOBWeaponData* WeaponData = nullptr;
			if (ResolveWeaponDefinition(
				Entry->Item.GetDefinition(),
				WeaponClass,
				WeaponData))
			{
				EquipSlot(WeaponData->WeaponSlot);
			}
		}
		return true;
	}

	auto GetInventoryArray =
		[this](const EInventoryItemLocation Location) -> TArray<FInventoryData>*
		{
			if (Location == EInventoryItemLocation::Backpack)
			{
				return &InventoryBackPackArray;
			}
			return nullptr;
		};

	TArray<FInventoryData>* SourceArray = GetInventoryArray(Source.Location);
	TArray<FInventoryData>* TargetArray = GetInventoryArray(Target.Location);
	if (!SourceArray || !TargetArray || !TargetArray->IsValidIndex(Target.SlotIndex))
	{
		return false;
	}

	const int32 SourceIndex = SourceArray->IndexOfByPredicate(
		[&Source](const FInventoryData& Item)
		{
			return Item.InstanceId == Source.InstanceId;
		});
	if (!SourceArray->IsValidIndex(SourceIndex))
	{
		return false;
	}

	if (Source.Location == Target.Location && SourceIndex == Target.SlotIndex)
	{
		return true;
	}

	Swap((*SourceArray)[SourceIndex], (*TargetArray)[Target.SlotIndex]);
	NotifyInventoryChanged();
	return true;
}

void UPlayerInventoryComponent::DropItem(
	const FInventoryItemHandle& Source)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_DropItem(Source);
		return;
	}
	DropItemInternal(Source);
}

void UPlayerInventoryComponent::Server_DropItem_Implementation(
	FInventoryItemHandle Source)
{
	DropItemInternal(Source);
}

bool UPlayerInventoryComponent::DropItemInternal(
	const FInventoryItemHandle& Source)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Source.HasItem())
	{
		return false;
	}

	if (Source.Location == EInventoryItemLocation::QuickSlot)
	{
		if (!InventoryQuickSlotsArray.IsValidIndex(Source.SlotIndex) ||
			InventoryQuickSlotsArray[Source.SlotIndex].ItemTag !=
				Source.ItemTag)
		{
			return false;
		}
		return ClearQuickSlotInternal(Source.SlotIndex);
	}

	if (Source.Location == EInventoryItemLocation::Equipment)
	{
		const FEquipmentSlotEntry* Entry =
			FindEquipmentSlot(Source.EquipmentSlot);
		return Entry && Entry->Item.InstanceId == Source.InstanceId &&
			DropEquipmentSlotInternal(Source.EquipmentSlot);
	}

	FInventoryData* Item = FindItemInInventory(
		Source.Location,
		Source.InstanceId,
		Source.SlotIndex);
	if (!Item)
	{
		return false;
	}

	const TArray<FInventoryData> EmptyContents;
	if (!SpawnDroppedWorldItem(*Item, EmptyContents))
	{
		return false;
	}

	*Item = FInventoryData();
	NotifyInventoryChanged();
	return true;
}

void UPlayerInventoryComponent::UnequipItem(
	const EOBEquipmentSlot EquipmentSlot)
{
	if (EquipmentSlot == EOBEquipmentSlot::Backpack)
	{
		DropEquippedItem(EquipmentSlot);
		return;
	}
	FInventoryItemHandle Source = MakeEquipmentHandle(EquipmentSlot);
	FInventoryItemHandle Target;
	Target.Location = EInventoryItemLocation::Backpack;
	MoveItem(Source, Target);
}

void UPlayerInventoryComponent::DropEquippedItem(
	const EOBEquipmentSlot EquipmentSlot)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_DropEquippedItem(EquipmentSlot);
		return;
	}
	DropEquipmentSlotInternal(EquipmentSlot);
}

void UPlayerInventoryComponent::Server_DropEquippedItem_Implementation(
	const EOBEquipmentSlot EquipmentSlot)
{
	DropEquipmentSlotInternal(EquipmentSlot);
}

bool UPlayerInventoryComponent::DropEquipmentSlotInternal(
	const EOBEquipmentSlot EquipmentSlot)
{
	AActor* OwnerActor = GetOwner();
	FEquipmentSlotEntry* Entry = FindEquipmentSlot(EquipmentSlot);
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!Entry || Entry->IsEmpty())
	{
		return false;
	}

	if (Entry->Item.InstanceId == ActiveWeaponInstanceId)
	{
		SyncActiveMagazine();
	}

	const TArray<FInventoryData> EmptyContents;
	if (!SpawnDroppedWorldItem(
		Entry->Item,
		EquipmentSlot == EOBEquipmentSlot::Backpack
			? InventoryBackPackArray
			: EmptyContents))
	{
		return false;
	}

	if (Entry->Item.InstanceId == ActiveWeaponInstanceId)
	{
		UnequipWeapon();
	}
	Entry->Item = FInventoryData();
	if (EquipmentSlot == EOBEquipmentSlot::Backpack)
	{
		InventoryBackPackArray.Reset();
		InventoryBackPackSize = 0;
	}
	NotifyInventoryChanged();
	return true;
}

AWorldItem* UPlayerInventoryComponent::SpawnDroppedWorldItem(
	const FInventoryData& Item,
	const TArray<FInventoryData>& ContainedInventory)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	const FOBItemDefinitionRow* ItemRow = Item.GetDefinition();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !World ||
		!ItemRow || Item.ItemStack <= 0 ||
		ItemRow->WorldItemClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedWorldItemClass =
		ItemRow->WorldItemClass.LoadSynchronous();
	if (!LoadedWorldItemClass ||
		!LoadedWorldItemClass->IsChildOf(AWorldItem::StaticClass()))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const UInventorySystemSetting* Settings =
		GetDefault<UInventorySystemSetting>();
	const float DropDistance = Settings
		? FMath::Max(0.f, Settings->DroppedItemForwardDistance)
		: 100.f;
	const FVector DropLocation =
		OwnerActor->GetActorLocation() +
		OwnerActor->GetActorForwardVector() * DropDistance;
	AWorldItem* DroppedItem = World->SpawnActor<AWorldItem>(
		LoadedWorldItemClass,
		FTransform(OwnerActor->GetActorRotation(), DropLocation),
		SpawnParameters);
	if (DroppedItem)
	{
		DroppedItem->InitializeDroppedItem(Item, ContainedInventory);
	}
	return DroppedItem;
}

TSubclassOf<AOBWeaponBase> UPlayerInventoryComponent::GetWeaponInSlot(
	const EOBWeaponSlot Slot) const
{
	const FEquipmentSlotEntry* Entry =
		FindEquipmentSlot(ToEquipmentSlot(Slot));
	if (!Entry || Entry->IsEmpty())
	{
		return nullptr;
	}

	TSubclassOf<AOBWeaponBase> WeaponClass;
	const UOBWeaponData* WeaponData = nullptr;
	return ResolveWeaponDefinition(
		Entry->Item.GetDefinition(),
		WeaponClass,
		WeaponData) && WeaponData->WeaponSlot == Slot
		? WeaponClass
		: nullptr;
}

void UPlayerInventoryComponent::EquipSlot(const EOBWeaponSlot Slot)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_EquipSlot(Slot);
		return;
	}

	const FEquipmentSlotEntry* Entry =
		FindEquipmentSlot(ToEquipmentSlot(Slot));
	if (Entry && !Entry->IsEmpty())
	{
		EquipWeaponInstance(Entry->Item.InstanceId);
	}
}

void UPlayerInventoryComponent::Server_EquipSlot_Implementation(
	const EOBWeaponSlot Slot)
{
	EquipSlot(Slot);
}

void UPlayerInventoryComponent::EquipInventoryItem(
	const int32 BackpackIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_EquipInventoryItem(BackpackIndex);
		return;
	}

	EquipWeaponAtIndex(BackpackIndex);
}

void UPlayerInventoryComponent::Server_EquipInventoryItem_Implementation(
	const int32 BackpackIndex)
{
	EquipInventoryItem(BackpackIndex);
}

void UPlayerInventoryComponent::EquipWeaponAtIndex(
	const int32 BackpackIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!InventoryBackPackArray.IsValidIndex(BackpackIndex))
	{
		return;
	}

	FInventoryData& Item = InventoryBackPackArray[BackpackIndex];
	const FGuid ItemInstanceId = Item.InstanceId;
	EOBEquipmentSlot EquipmentSlot = EOBEquipmentSlot::None;
	if (!ResolveEquipmentSlot(Item, EquipmentSlot))
	{
		return;
	}

	TSubclassOf<AOBWeaponBase> WeaponClass;
	const UOBWeaponData* WeaponData = nullptr;
	const bool bWeapon = ResolveWeaponDefinition(
		Item.GetDefinition(),
		WeaponClass,
		WeaponData);
	if (bWeapon && bSwitching)
	{
		return;
	}

	if (!AssignItemToEquipmentSlot(ItemInstanceId, EquipmentSlot))
	{
		return;
	}
	if (!bWeapon)
	{
		return;
	}
	EquipWeaponInstance(ItemInstanceId);
}

void UPlayerInventoryComponent::EquipWeaponInstance(
	const FGuid& InstanceId)
{
	AActor* OwnerActor = GetOwner();
	FInventoryData* Item = FindEquippedItem(InstanceId);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !Item || bSwitching)
	{
		return;
	}

	TSubclassOf<AOBWeaponBase> WeaponClass;
	const UOBWeaponData* WeaponData = nullptr;
	if (!ResolveWeaponDefinition(Item->GetDefinition(), WeaponClass, WeaponData))
	{
		return;
	}

	UOBEquipmentComponent* Equipment =
		OwnerActor->FindComponentByClass<UOBEquipmentComponent>();
	if (!Equipment)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : Owner has no UOBEquipmentComponent."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}

	if (Item->InstanceId == ActiveWeaponInstanceId &&
		Equipment->GetCurrentWeapon())
	{
		return;
	}

	const bool bHadWeapon = Equipment->GetCurrentWeapon() != nullptr;
	SyncActiveMagazine();
	UnbindActiveWeapon();

	ActiveWeaponInstanceId = Item->InstanceId;
	ActiveWeaponSlot = WeaponData->WeaponSlot;
	if (bHadWeapon)
	{
		SetSwitching(true);
	}

	EquipActiveWeapon();
	if (!Equipment->GetCurrentWeapon())
	{
		ActiveWeaponInstanceId.Invalidate();
		SetSwitching(false);
		return;
	}

	NotifyInventoryChanged();
	if (bHadWeapon)
	{
		float DrawTime = DefaultDrawTime;
		if (WeaponData->EquipMontage)
		{
			DrawTime = WeaponData->EquipMontage->GetPlayLength();
		}

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SwapTimerHandle,
				this,
				&UPlayerInventoryComponent::EndSwitching,
				FMath::Max(0.01f, DrawTime),
				false);
		}
	}
}

void UPlayerInventoryComponent::EquipActiveWeapon()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	FInventoryData* Item = FindEquippedItem(ActiveWeaponInstanceId);
	if (!Item)
	{
		return;
	}

	TSubclassOf<AOBWeaponBase> WeaponClass;
	const UOBWeaponData* WeaponData = nullptr;
	if (!ResolveWeaponDefinition(Item->GetDefinition(), WeaponClass, WeaponData))
	{
		return;
	}

	UOBEquipmentComponent* Equipment =
		OwnerActor->FindComponentByClass<UOBEquipmentComponent>();
	if (!Equipment)
	{
		return;
	}

	Equipment->EquipWeapon(WeaponClass);
	AOBWeaponBase* Weapon = Equipment->GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}

	if (Item->MagazineAmmo >= 0)
	{
		Weapon->SetCurrentAmmo(Item->MagazineAmmo);
	}
	else
	{
		Item->MagazineAmmo = Weapon->GetCurrentAmmo();
	}

	WeaponAmmoHandle = Weapon->OnAmmoChanged.AddUObject(
		this,
		&UPlayerInventoryComponent::SyncActiveMagazine);
	BoundWeapon = Weapon;
}

void UPlayerInventoryComponent::SyncActiveMagazine()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	AOBWeaponBase* Weapon = BoundWeapon.Get();
	FInventoryData* Item = FindEquippedItem(ActiveWeaponInstanceId);
	if (Weapon && Item)
	{
		Item->MagazineAmmo = Weapon->GetCurrentAmmo();
	}
}

void UPlayerInventoryComponent::UnbindActiveWeapon()
{
	if (BoundWeapon.IsValid() && WeaponAmmoHandle.IsValid())
	{
		BoundWeapon->OnAmmoChanged.Remove(WeaponAmmoHandle);
	}
	WeaponAmmoHandle.Reset();
	BoundWeapon.Reset();
}

void UPlayerInventoryComponent::UnequipWeapon()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	if (!OwnerActor->HasAuthority())
	{
		Server_UnequipWeapon();
		return;
	}

	SyncActiveMagazine();
	UnbindActiveWeapon();
	if (UOBEquipmentComponent* Equipment =
		OwnerActor->FindComponentByClass<UOBEquipmentComponent>())
	{
		Equipment->UnequipWeapon();
	}

	ActiveWeaponInstanceId.Invalidate();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwapTimerHandle);
	}
	SetSwitching(false);
	NotifyInventoryChanged();
}

void UPlayerInventoryComponent::Server_UnequipWeapon_Implementation()
{
	UnequipWeapon();
}

void UPlayerInventoryComponent::EquipDefaultSlot()
{
	if (GetWeaponInSlot(EOBWeaponSlot::Primary))
	{
		EquipSlot(EOBWeaponSlot::Primary);
	}
	else if (GetWeaponInSlot(EOBWeaponSlot::Secondary))
	{
		EquipSlot(EOBWeaponSlot::Secondary);
	}
	else if (GetWeaponInSlot(EOBWeaponSlot::Melee))
	{
		EquipSlot(EOBWeaponSlot::Melee);
	}
}

void UPlayerInventoryComponent::SetSwitching(const bool bEnable)
{
	if (bSwitching == bEnable)
	{
		return;
	}

	bSwitching = bEnable;
	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (!ASC)
	{
		return;
	}

	if (bEnable)
	{
		FGameplayTagContainer ReloadTags;
		ReloadTags.AddTag(OBGameplayTags::State_Reloading);
		ASC->CancelAbilities(&ReloadTags);
		ASC->AddLooseGameplayTag(OBGameplayTags::State_Weapon_Switching);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(OBGameplayTags::State_Weapon_Switching);
	}
}

void UPlayerInventoryComponent::EndSwitching()
{
	SetSwitching(false);
}

void UPlayerInventoryComponent::NotifyInventoryChanged()
{
	UpdateInventoryWidget();
	OnInventoryChanged.Broadcast();
	OnInventoryUpdated.Broadcast();
}

void UPlayerInventoryComponent::OnRep_BackpackItems()
{
	InventoryBackPackSize = InventoryBackPackArray.Num();
	NotifyInventoryChanged();
	OnAmmoPoolChanged.Broadcast();
}

void UPlayerInventoryComponent::OnRep_ActiveWeapon()
{
	OnInventoryChanged.Broadcast();
}

void UPlayerInventoryComponent::OnRep_EquipmentSlots()
{
	NotifyInventoryChanged();
}

void UPlayerInventoryComponent::OnRep_QuickSlots()
{
	QuickSlotSize = InventoryQuickSlotsArray.Num();
	NotifyInventoryChanged();
}
