// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "Inventory/Data/InventoryData.h"
#include "Weapon/Data/OBWeaponTypes.h"

#include "PlayerInventoryComponent.generated.h"


class AWorldItem;
class AOBWeaponBase;
class UOBEquipmentComponent;
class UOBWeaponData;
class UInventoryWindow;
struct FOBItemDefinitionRow;

DECLARE_MULTICAST_DELEGATE(FOnPlayerInventoryChanged);
DECLARE_MULTICAST_DELEGATE(FOnPlayerAmmoPoolChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerInventoryUpdated);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UPlayerInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	
	void PickUpWorldItem(AWorldItem* WorldItem);

	// 아이템 정적 스펙의 원본은 DT_Items(FOBItemDefinitionRow)다.
	// BP에서도 태그 드롭다운으로 고를 수 있어 에셋 참조보다 다루기 쉽다.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 TryAddItem(const FGameplayTag& ItemTag, int32 RequestedAmount);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 TryAddItemInstance(const FInventoryData& ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryExtractItemInstance(
		const FGuid& InstanceId,
		FInventoryData& OutItemInstance);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 TryRemoveItem(const FGameplayTag& ItemTag, int32 RequestedAmount);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const FGameplayTag& ItemTag) const;

	void GetLootableItemInstances(TArray<FInventoryData>& OutItems);
	void ClearLootableItemInstances();

	// Ammo is stored as ordinary item stacks whose ItemTag matches WeaponData::AmmoType.
	UFUNCTION(BlueprintPure, Category = "Inventory|Ammo")
	int32 GetAmmo(const FGameplayTag& AmmoType) const;
	void AddAmmo(const FGameplayTag& AmmoType, int32 Amount);
	int32 ConsumeAmmoFromPool(const FGameplayTag& AmmoType, int32 Amount);

	// Weapon inventory owns selection and magazine state. Equipment only spawns/attaches.
	bool AddWeapon(TSubclassOf<AOBWeaponBase> WeaponClass);
	bool EquipStartingBackpack(const FGameplayTag& BackpackItemTag);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Weapon")
	void EquipSlot(EOBWeaponSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void EquipInventoryItem(int32 BackpackIndex);

	// Widget drag/drop entry point. Source identity is re-resolved by InstanceId
	// on the server, so stale UI array indices cannot move the wrong item.
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void MoveItem(const FInventoryItemHandle& Source, const FInventoryItemHandle& Target);

	UFUNCTION(Server, Reliable)
	void Server_MoveItem(FInventoryItemHandle Source, FInventoryItemHandle Target);

	// A quick slot owns only a type reference, so dropping it clears the
	// assignment. All other locations drop their concrete item instance.
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void DropItem(const FInventoryItemHandle& Source);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(FInventoryItemHandle Source);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void UnequipItem(EOBEquipmentSlot EquipmentSlot);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void DropEquippedItem(EOBEquipmentSlot EquipmentSlot);

	UFUNCTION(Server, Reliable)
	void Server_DropEquippedItem(EOBEquipmentSlot EquipmentSlot);

	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	FInventoryItemHandle MakeBackpackHandle(int32 BackpackIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	FInventoryItemHandle MakeEquipmentHandle(EOBEquipmentSlot EquipmentSlot) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	FInventoryItemHandle MakeQuickSlotHandle(int32 QuickSlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	bool GetItemFromHandle(const FInventoryItemHandle& Handle, FInventoryData& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	TArray<FEquipmentSlotEntry> GetEquipmentSlotsForUI() const { return EquipmentSlots; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	bool GetEquippedItem(EOBEquipmentSlot EquipmentSlot, FInventoryData& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	bool IsItemEquipped(FGuid InstanceId, EOBEquipmentSlot& OutEquipmentSlot) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryData> GetBackpackItemsForUI() const { return InventoryBackPackArray; }

	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	TArray<FQuickSlotData> GetQuickSlotsForUI() const { return InventoryQuickSlotsArray; }

	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	bool GetQuickSlot(int32 QuickSlotIndex, FQuickSlotData& OutQuickSlot) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	int32 GetQuickSlotItemCount(int32 QuickSlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void AssignQuickSlot(int32 QuickSlotIndex, const FInventoryItemHandle& Source);

	UFUNCTION(Server, Reliable)
	void Server_AssignQuickSlot(int32 QuickSlotIndex, FInventoryItemHandle Source);

	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void ClearQuickSlot(int32 QuickSlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_ClearQuickSlot(int32 QuickSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void UseQuickSlot(int32 QuickSlotIndex);

	// Local UI lifetime. Inventory state itself remains server authoritative.
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	UInventoryWindow* OpenInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void CloseInventory();

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	bool IsInventoryOpen() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|Weapon")
	void UnequipWeapon();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Weapon")
	void EquipDefaultSlot();

	UFUNCTION(Server, Reliable)
	void Server_EquipSlot(EOBWeaponSlot Slot);

	UFUNCTION(Server, Reliable)
	void Server_EquipInventoryItem(int32 BackpackIndex);

	UFUNCTION(Server, Reliable)
	void Server_UnequipWeapon();

	UFUNCTION(BlueprintPure, Category = "Inventory|Weapon")
	EOBWeaponSlot GetActiveSlot() const { return ActiveWeaponSlot; }

	TSubclassOf<AOBWeaponBase> GetWeaponInSlot(EOBWeaponSlot Slot) const;

	FOnPlayerInventoryChanged OnInventoryChanged;
	FOnPlayerAmmoPoolChanged OnAmmoPoolChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnPlayerInventoryUpdated OnInventoryUpdated;


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	
	void UpdateInventory();
	void UpdateInventoryWidget();
	
	UFUNCTION()
	void OnRep_BackpackItems();

	UFUNCTION()
	void OnRep_ActiveWeapon();

	UFUNCTION()
	void OnRep_EquipmentSlots();

	UFUNCTION()
	void OnRep_QuickSlots();

private:
	int32 AddItemRowInternal(
		const FOBItemDefinitionRow* ItemRow,
		int32 RequestedAmount,
		FGuid* OutFirstAddedInstanceId = nullptr,
		const FInventoryData* SourceInstance = nullptr);
	bool ResolveWeaponDefinition(
		const FOBItemDefinitionRow* ItemRow,
		TSubclassOf<AOBWeaponBase>& OutWeaponClass,
		const UOBWeaponData*& OutWeaponData) const;
	bool ResolveEquipmentSlot(
		const FInventoryData& Item,
		EOBEquipmentSlot& OutEquipmentSlot) const;
	bool MoveItemInternal(
		const FInventoryItemHandle& Source,
		const FInventoryItemHandle& Target);
	bool AssignQuickSlotInternal(
		int32 QuickSlotIndex,
		const FInventoryItemHandle& Source);
	bool ClearQuickSlotInternal(int32 QuickSlotIndex);
	bool UseQuickSlotInternal(int32 QuickSlotIndex);
	bool DropItemInternal(const FInventoryItemHandle& Source);
	bool AssignItemToEquipmentSlot(
		const FGuid& InstanceId,
		EOBEquipmentSlot EquipmentSlot);
	bool MoveInventoryItemToEquipment(
		EInventoryItemLocation SourceLocation,
		const FGuid& InstanceId,
		int32 IndexHint,
		EOBEquipmentSlot EquipmentSlot);
	bool MoveEquipmentItemToInventory(
		EOBEquipmentSlot EquipmentSlot,
		EInventoryItemLocation TargetLocation,
		int32 TargetIndex);
	bool ClearEquipmentSlot(EOBEquipmentSlot EquipmentSlot);
	bool DropEquipmentSlotInternal(EOBEquipmentSlot EquipmentSlot);
	AWorldItem* SpawnDroppedWorldItem(
		const FInventoryData& Item,
		const TArray<FInventoryData>& ContainedInventory);
	bool PickUpDroppedItemInstance(AWorldItem* WorldItem);
	bool PickUpDroppedBackpack(AWorldItem* WorldItem);
	bool CanSetBackpackCapacity(int32 NewCapacity, int32 ReplacedItemIndex) const;
	void CompactBackpack(int32 NewCapacity);
	int32 GetBackpackCapacity(const FInventoryData& BackpackItem) const;
	void InitializeEquipmentSlots();
	FEquipmentSlotEntry* FindEquipmentSlot(EOBEquipmentSlot EquipmentSlot);
	const FEquipmentSlotEntry* FindEquipmentSlot(EOBEquipmentSlot EquipmentSlot) const;
	FEquipmentSlotEntry* FindEquipmentSlotByItem(const FGuid& InstanceId);
	FInventoryData* FindEquippedItem(const FGuid& InstanceId);
	const FInventoryData* FindEquippedItem(const FGuid& InstanceId) const;
	const FInventoryData* FindItemInInventory(
		EInventoryItemLocation Location,
		const FGuid& InstanceId,
		int32 IndexHint = INDEX_NONE) const;
	FInventoryData* FindItemInInventory(
		EInventoryItemLocation Location,
		const FGuid& InstanceId,
		int32 IndexHint = INDEX_NONE);
	static EOBEquipmentSlot ToEquipmentSlot(EOBWeaponSlot WeaponSlot);
	FInventoryData* FindBackpackItem(const FGuid& InstanceId);
	const FInventoryData* FindBackpackItem(const FGuid& InstanceId) const;
	void EquipWeaponAtIndex(int32 BackpackIndex);
	void EquipWeaponInstance(const FGuid& InstanceId);
	void EquipActiveWeapon();
	void SyncActiveMagazine();
	void UnbindActiveWeapon();
	void SetSwitching(bool bEnable);
	void EndSwitching();
	void NotifyInventoryChanged();
	UPROPERTY(Transient)
	TObjectPtr<UInventoryWindow> InventoryWidget;
	
	int32 InventoryBackPackSize = 0;
	
	
	/* 고정 값 */
	int32 QuickSlotSize = 0;
	
	UPROPERTY(ReplicatedUsing = OnRep_BackpackItems, meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryData> InventoryBackPackArray;
	
	UPROPERTY(ReplicatedUsing = OnRep_QuickSlots,
		meta = (AllowPrivateAccess = "true"))
	TArray<FQuickSlotData> InventoryQuickSlotsArray;

	UPROPERTY(ReplicatedUsing = OnRep_EquipmentSlots, BlueprintReadOnly,
		Category = "Inventory|Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FEquipmentSlotEntry> EquipmentSlots;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeapon)
	FGuid ActiveWeaponInstanceId;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeapon)
	EOBWeaponSlot ActiveWeaponSlot = EOBWeaponSlot::Primary;

	TWeakObjectPtr<AOBWeaponBase> BoundWeapon;
	FDelegateHandle WeaponAmmoHandle;
	FTimerHandle SwapTimerHandle;
	bool bSwitching = false;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Weapon")
	float DefaultDrawTime = 0.5f;
	
};
