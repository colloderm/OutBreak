// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Loot/OBLootTable.h"

#include "Data/OBGameDataSubsystem.h"
#include "Item/Data/OBItemDefinition.h"
#include "Weapon/Data/OBWeaponDefinition.h"

namespace
{
	FGuid MakeDeterministicGuid(FRandomStream& Stream)
	{
		return FGuid(
			static_cast<uint32>(Stream.RandHelper(MAX_int32)),
			static_cast<uint32>(Stream.RandHelper(MAX_int32)),
			static_cast<uint32>(Stream.RandHelper(MAX_int32)),
			static_cast<uint32>(Stream.RandHelper(MAX_int32)));
	}
}

void FOBLootTableRow::Roll(FRandomStream& Stream, TArray<FOBItemStack>& OutItems) const
{
	OutItems.Reset();
	if (Entries.IsEmpty()) return;
	
	float TotalWeight = 0.f;
	for (const FOBLootEntry& Entry : Entries)
	{
		if (Entry.ItemTag.IsValid())
		{
			TotalWeight += FMath::Max(0.f, Entry.Weight);
		}
	}
	if (TotalWeight <= 0.f) return; // 전부 가중치 0 -> 나올게 없다
	
	const int32 Rolls = Stream.RandRange(FMath::Min(MinRolls, MaxRolls), FMath::Max(MinRolls, MaxRolls));
	
	for (int32 i = 0; i < Rolls; ++i)
	{
		float Pick = Stream.FRandRange(0.f, TotalWeight);
		
		for (const FOBLootEntry& Entry : Entries)
		{
			const float W = Entry.ItemTag.IsValid() ? FMath::Max(0.f, Entry.Weight) : 0.f;
			if (W <= 0.f) continue;
			
			Pick -= W;
			if (Pick > 0.f) continue;
			
			const int32 CountLo = FMath::Min(Entry.MinCount, Entry.MaxCount);
			const int32 CountHi = FMath::Max(Entry.MinCount, Entry.MaxCount);
			OBItemStacks::Add(OutItems, Entry.ItemTag, Stream.RandRange(CountLo, CountHi));
			break;
		}
	}
}

void FOBLootTableRow::RollInstances(
	FRandomStream& Stream,
	TArray<FInventoryData>& OutItems) const
{
	OutItems.Reset();
	float TotalWeight = 0.f;
	for (const FOBLootEntry& Entry : Entries)
	{
		if (Entry.ItemTag.IsValid())
		{
			TotalWeight += FMath::Max(0.f, Entry.Weight);
		}
	}
	if (TotalWeight <= 0.f)
	{
		return;
	}

	const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get();
	const int32 Rolls = Stream.RandRange(
		FMath::Min(MinRolls, MaxRolls),
		FMath::Max(MinRolls, MaxRolls));
	for (int32 RollIndex = 0; RollIndex < Rolls; ++RollIndex)
	{
		float Pick = Stream.FRandRange(0.f, TotalWeight);
		const FOBLootEntry* Selected = nullptr;
		for (const FOBLootEntry& Entry : Entries)
		{
			const float EntryWeight = Entry.ItemTag.IsValid()
				? FMath::Max(0.f, Entry.Weight)
				: 0.f;
			if (EntryWeight <= 0.f)
			{
				continue;
			}
			Pick -= EntryWeight;
			if (Pick <= 0.f)
			{
				Selected = &Entry;
				break;
			}
		}
		if (!Selected)
		{
			continue;
		}

		const FOBItemDefinitionRow* ItemRow = GameData
			? GameData->FindItem(Selected->ItemTag)
			: nullptr;
		if (!ItemRow)
		{
			continue;
		}
		const bool bUnique =
			ItemRow->Category == EOBItemCategory::Weapon ||
			ItemRow->Category == EOBItemCategory::Equipment ||
			ItemRow->Category == EOBItemCategory::Attachment;
		int32 Remaining = Stream.RandRange(
			FMath::Min(Selected->MinCount, Selected->MaxCount),
			FMath::Max(Selected->MinCount, Selected->MaxCount));
		const int32 MaxStack = bUnique ? 1 : FMath::Max(1, ItemRow->MaxStack);
		while (Remaining > 0)
		{
			FInventoryData& Instance = OutItems.AddDefaulted_GetRef();
			Instance.ItemTag = Selected->ItemTag;
			Instance.ItemStack = FMath::Min(MaxStack, Remaining);
			Instance.InstanceId = MakeDeterministicGuid(Stream);
			Instance.Durability = Stream.FRandRange(
				FMath::Min(Selected->MinDurability, Selected->MaxDurability),
				FMath::Max(Selected->MinDurability, Selected->MaxDurability));
			Instance.QualityTier = Stream.RandRange(
				FMath::Min(Selected->MinQualityTier, Selected->MaxQualityTier),
				FMath::Max(Selected->MinQualityTier, Selected->MaxQualityTier));
			Instance.bFoundInRaid = Selected->bFoundInRaid;
			Instance.RaidAcquisitionId = Selected->bFoundInRaid
				? MakeDeterministicGuid(Stream)
				: FGuid();

			if (ItemRow->Category == EOBItemCategory::Weapon && GameData)
			{
				const FOBWeaponDefinitionRow* Weapon = GameData->FindWeapon(Selected->ItemTag);
				for (const FGameplayTag& AttachmentTag : Selected->GuaranteedAttachments)
				{
					const FOBAttachmentDefinitionRow* Attachment = GameData->FindAttachment(AttachmentTag);
					if (!Weapon || !Attachment ||
						Instance.Attachments.ContainsByPredicate(
							[Attachment](const FOBInstalledAttachment& Existing)
							{
								return Existing.SlotTag == Attachment->SlotTag;
							}))
					{
						continue;
					}
					const bool bHasSlot = Weapon->AttachmentSlots.ContainsByPredicate(
						[Attachment](const FOBAttachmentSlotSpec& Slot)
						{
							return Slot.SlotTag == Attachment->SlotTag;
						});
					if (!bHasSlot)
					{
						continue;
					}
					FOBInstalledAttachment& Installed = Instance.Attachments.AddDefaulted_GetRef();
					Installed.SlotTag = Attachment->SlotTag;
					Installed.ItemTag = AttachmentTag;
					Installed.InstanceId = MakeDeterministicGuid(Stream);
					Installed.bFoundInRaid = Selected->bFoundInRaid;
					Installed.RaidAcquisitionId = Instance.RaidAcquisitionId;
				}
			}
			Remaining -= Instance.ItemStack;
		}
	}
}
