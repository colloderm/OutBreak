#include "Data/OBGameDataSubsystem.h"

#include "Data/OBGameDataSettings.h"
#include "Ability/Data/OBAbilitySet.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Item/Data/OBItemDefinition.h"
#include "Item/OBItemRegistry.h"
#include "NiagaraSystem.h"
#include "Player/Data/OBPlayerStatData.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Weapon/Data/OBWeaponDefinition.h"
#include "Weapon/OBWeaponBase.h"

namespace
{
	template <typename RowType>
	void CacheRowsByTag(UDataTable* Table, TMap<FGameplayTag, const RowType*>& Cache)
	{
		Cache.Reset();
		if (!Table)
		{
			return;
		}

		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const RowType* Row = reinterpret_cast<const RowType*>(Pair.Value);
			if (Row && Row->ItemTag.IsValid() && !Cache.Contains(Row->ItemTag))
			{
				Cache.Add(Row->ItemTag, Row);
			}
		}
	}
}

UOBGameDataSubsystem* UOBGameDataSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UOBGameDataSubsystem>() : nullptr;
}

void UOBGameDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadTables();
	RebuildCaches();
}

void UOBGameDataSubsystem::Deinitialize()
{
	ItemCache.Reset();
	WeaponCache.Reset();
	AttachmentCache.Reset();
	StatDisplayCache.Reset();
	WeaponPathToTag.Reset();
	LegacyWeaponRows.Reset();
	LoadedItemTable = nullptr;
	LoadedWeaponTable = nullptr;
	LoadedAttachmentTable = nullptr;
	LoadedPlayerArchetypeTable = nullptr;
	LoadedStatDisplayTable = nullptr;
	LoadedLootTable = nullptr;
	Super::Deinitialize();
}

void UOBGameDataSubsystem::LoadTables()
{
	const UOBGameDataSettings* Settings = GetDefault<UOBGameDataSettings>();
	const UOBItemRegistry* LegacySettings = GetDefault<UOBItemRegistry>();

	LoadedItemTable = Settings ? Settings->ItemTable.LoadSynchronous() : nullptr;
	LoadedLootTable = Settings ? Settings->LootTable.LoadSynchronous() : nullptr;
	if (!LoadedItemTable && LegacySettings)
	{
		LoadedItemTable = LegacySettings->ItemTable.LoadSynchronous();
	}
	if (!LoadedLootTable && LegacySettings)
	{
		LoadedLootTable = LegacySettings->LootTable.LoadSynchronous();
	}

	LoadedWeaponTable = Settings ? Settings->WeaponTable.LoadSynchronous() : nullptr;
	LoadedAttachmentTable = Settings ? Settings->AttachmentTable.LoadSynchronous() : nullptr;
	LoadedPlayerArchetypeTable = Settings ? Settings->PlayerArchetypeTable.LoadSynchronous() : nullptr;
	LoadedStatDisplayTable = Settings ? Settings->StatDisplayTable.LoadSynchronous() : nullptr;
}

void UOBGameDataSubsystem::RebuildCaches()
{
	CacheRowsByTag(LoadedItemTable, ItemCache);
	CacheRowsByTag(LoadedWeaponTable, WeaponCache);
	CacheRowsByTag(LoadedAttachmentTable, AttachmentCache);

	StatDisplayCache.Reset();
	if (LoadedStatDisplayTable)
	{
		for (const TPair<FName, uint8*>& Pair : LoadedStatDisplayTable->GetRowMap())
		{
			const FOBStatDisplayRow* Row = reinterpret_cast<const FOBStatDisplayRow*>(Pair.Value);
			if (Row && Row->StatTag.IsValid() && !StatDisplayCache.Contains(Row->StatTag))
			{
				StatDisplayCache.Add(Row->StatTag, Row);
			}
		}
	}

	BuildLegacyWeaponRows();

	WeaponPathToTag.Reset();
	for (const TPair<FGameplayTag, const FOBWeaponDefinitionRow*>& Pair : WeaponCache)
	{
		if (Pair.Value && !Pair.Value->ActorClass.IsNull())
		{
			WeaponPathToTag.Add(Pair.Value->ActorClass.ToSoftObjectPath(), Pair.Key);
		}
	}

	TArray<FString> Errors;
	ValidateLoadedData(&Errors);
	for (const FString& Error : Errors)
	{
		UE_LOG(LogTemp, Error, TEXT("[OBGameData] %s"), *Error);
	}
	UE_LOG(LogTemp, Log, TEXT("[OBGameData] Items=%d Weapons=%d Attachments=%d PlayerProfiles=%d"),
		ItemCache.Num(), WeaponCache.Num(), AttachmentCache.Num(),
		LoadedPlayerArchetypeTable ? LoadedPlayerArchetypeTable->GetRowMap().Num() : 0);
}

void UOBGameDataSubsystem::BuildLegacyWeaponRows()
{
	LegacyWeaponRows.Reset();

	for (const TPair<FGameplayTag, const FOBItemDefinitionRow*>& Pair : ItemCache)
	{
		const FOBItemDefinitionRow* Item = Pair.Value;
		if (!Item || Item->Category != EOBItemCategory::Weapon || WeaponCache.Contains(Pair.Key) || Item->WeaponClass.IsNull())
		{
			continue;
		}

		UClass* WeaponClass = Item->WeaponClass.LoadSynchronous();
		const AOBWeaponBase* CDO = WeaponClass ? WeaponClass->GetDefaultObject<AOBWeaponBase>() : nullptr;
		const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
		if (!Data)
		{
			continue;
		}

		FOBWeaponDefinitionRow& Row = LegacyWeaponRows.Add(Pair.Key);
		Row.ItemTag = Pair.Key;
		Row.WeaponType = Data->WeaponType;
		Row.WeaponSlot = Data->WeaponSlot;
		Row.WeaponCategory = Data->WeaponCategory;
		Row.ActorClass = WeaponClass;
		Row.Common.BaseDamage = Data->BaseDamage;
		Row.Common.EffectiveRange = Data->Range;
		Row.Common.MobilityMultiplier = Data->MobilityMultiplier;
		Row.Common.HeadshotMultiplier = Data->HeadshotMultiplier;
		Row.Common.DamageEffect = Data->DamageEffect;
		Row.Common.AbilitySet = Data->AbilitySet;
		Row.Visual.WeaponMesh = Data->WeaponMesh;
		Row.Visual.AttachSocket = Data->AttachSocket;
		Row.Visual.OverlayLocomotion = Data->OverlayLocomotion;
		Row.Visual.AimOffset = Data->AimOffset;
		Row.Visual.ADSPose = Data->ADSPose;
		Row.Visual.SprintPose = Data->SprintPose;
		Row.Visual.AttackMontage = Data->AttackMontage;
		Row.Visual.EquipMontage = Data->EquipMontage;
		Row.Visual.ReloadMontage = Data->ReloadMontage;
		Row.Ranged.AmmoType = Data->AmmoType;
		Row.Ranged.MagazineSize = Data->MagazineSize;
		Row.Ranged.MaxReserveAmmo = Data->MaxReserveAmmo;
		Row.Ranged.RoundsPerMinute = Data->RoundsPerMinute;
		Row.Ranged.FireMode = Data->FireMode;
		Row.Ranged.BurstCount = Data->BurstCount;
		Row.Ranged.PelletsPerShot = Data->PelletsPerShot;
		Row.Ranged.FireSound = Data->FireSound;
		Row.Ranged.VerticalRecoil = Data->VerticalRecoil;
		Row.Ranged.HorizontalRecoil = Data->HorizontalRecoil;
		Row.Ranged.RecoilRecoverySpeed = Data->RecoilRecoverySpeed;
		Row.Ranged.FireCameraShake = Data->FireCameraShake;
		Row.Ranged.FireCameraShakeScale = Data->FireCameraShakeScale;
		Row.Ranged.ADSFOV = Data->ADSFOV;
		Row.Ranged.ADSSpeedMultiplier = Data->ADSSpeedMultiplier;
		Row.Ranged.ADSBlendSpeed = Data->ADSBlendSpeed;
		Row.Ranged.ADSRecoilMultiplier = Data->ADSRecoilMultiplier;
		Row.Ranged.BaseSpreadDegrees = Data->BaseSpreadDegrees;
		Row.Ranged.ADSSpreadMultiplier = Data->ADSSpreadMultiplier;
		Row.Ranged.MovingSpreadMultiplier = Data->MovingSpreadMultiplier;
		Row.Ranged.FireFocusPulse = Data->FireFocusPulse;
		Row.Melee.Reach = Data->Range;
		Row.Melee.SwingSound = Data->SwingSound;
		Row.Melee.SwingTrailVFX = Data->SwingTrailVFX;
		Row.Melee.TrailSocketName = Data->TrailSocketName;

		auto AddSlot = [&Row](const FGameplayTag& SlotTag, const FName Socket)
		{
			FOBAttachmentSlotSpec& Slot = Row.AttachmentSlots.AddDefaulted_GetRef();
			Slot.SlotTag = SlotTag;
			Slot.MeshSocket = Socket;
		};
		if (Data->WeaponType == EOBWeaponType::Ranged)
		{
			AddSlot(OBGameplayTags::AttachmentSlot_Optic, TEXT("OpticSocket"));
			AddSlot(OBGameplayTags::AttachmentSlot_Muzzle, TEXT("Muzzle"));
			AddSlot(OBGameplayTags::AttachmentSlot_Magazine, TEXT("MagazineSocket"));
			AddSlot(OBGameplayTags::AttachmentSlot_Stock, TEXT("StockSocket"));
			AddSlot(OBGameplayTags::AttachmentSlot_Grip, TEXT("GripSocket"));
		}
		else
		{
			AddSlot(OBGameplayTags::AttachmentSlot_Melee_Mod, TEXT("ModSocket"));
		}
	}

	for (TPair<FGameplayTag, FOBWeaponDefinitionRow>& Pair : LegacyWeaponRows)
	{
		WeaponCache.Add(Pair.Key, &Pair.Value);
	}
}

const FOBItemDefinitionRow* UOBGameDataSubsystem::FindItem(const FGameplayTag& ItemTag) const
{
	const FOBItemDefinitionRow* const* Found = ItemCache.Find(ItemTag);
	return Found ? *Found : nullptr;
}

const FOBWeaponDefinitionRow* UOBGameDataSubsystem::FindWeapon(const FGameplayTag& ItemTag) const
{
	const FOBWeaponDefinitionRow* const* Found = WeaponCache.Find(ItemTag);
	return Found ? *Found : nullptr;
}

const FOBAttachmentDefinitionRow* UOBGameDataSubsystem::FindAttachment(const FGameplayTag& ItemTag) const
{
	const FOBAttachmentDefinitionRow* const* Found = AttachmentCache.Find(ItemTag);
	return Found ? *Found : nullptr;
}

const FOBPlayerArchetypeRow* UOBGameDataSubsystem::FindPlayerArchetype(const FName RowName) const
{
	return LoadedPlayerArchetypeTable
		? LoadedPlayerArchetypeTable->FindRow<FOBPlayerArchetypeRow>(RowName, TEXT("OBPlayerArchetype"), false)
		: nullptr;
}

const FOBStatDisplayRow* UOBGameDataSubsystem::FindStatDisplay(const FGameplayTag& StatTag) const
{
	const FOBStatDisplayRow* const* Found = StatDisplayCache.Find(StatTag);
	return Found ? *Found : nullptr;
}

FGameplayTag UOBGameDataSubsystem::FindTagForWeaponClass(const UClass* WeaponClass) const
{
	const FGameplayTag* Found = WeaponClass ? WeaponPathToTag.Find(FSoftObjectPath(WeaponClass)) : nullptr;
	return Found ? *Found : FGameplayTag();
}

void UOBGameDataSubsystem::GetAllItems(TArray<const FOBItemDefinitionRow*>& OutItems) const
{
	OutItems.Reset(ItemCache.Num());
	for (const TPair<FGameplayTag, const FOBItemDefinitionRow*>& Pair : ItemCache)
	{
		if (Pair.Value)
		{
			OutItems.Add(Pair.Value);
		}
	}
}

void UOBGameDataSubsystem::ReloadGameData()
{
	LoadTables();
	RebuildCaches();
}

bool UOBGameDataSubsystem::ValidateLoadedData(TArray<FString>* OutErrors) const
{
	TArray<FString> LocalErrors;
	if (LoadedItemTable && LoadedItemTable->GetRowStruct() != FOBItemDefinitionRow::StaticStruct())
	{
		LocalErrors.Add(TEXT("ItemTable has the wrong row struct."));
	}
	if (LoadedWeaponTable && LoadedWeaponTable->GetRowStruct() != FOBWeaponDefinitionRow::StaticStruct())
	{
		LocalErrors.Add(TEXT("WeaponTable has the wrong row struct."));
	}
	for (const TPair<FGameplayTag, const FOBWeaponDefinitionRow*>& Pair : WeaponCache)
	{
		const FOBWeaponDefinitionRow* Weapon = Pair.Value;
		if (!ItemCache.Contains(Pair.Key))
		{
			LocalErrors.Add(FString::Printf(TEXT("Weapon %s has no DT_Items row."), *Pair.Key.ToString()));
		}
		if (!Weapon || Weapon->ActorClass.IsNull())
		{
			LocalErrors.Add(FString::Printf(TEXT("Weapon %s has no ActorClass."), *Pair.Key.ToString()));
			continue;
		}
		if (Weapon->WeaponType == EOBWeaponType::Ranged &&
			(!Weapon->Ranged.AmmoType.IsValid() || Weapon->Ranged.MagazineSize <= 0 || Weapon->Ranged.RoundsPerMinute <= 0.f))
		{
			LocalErrors.Add(FString::Printf(TEXT("Ranged weapon %s has invalid ammo/RPM data."), *Pair.Key.ToString()));
		}
		if (Weapon->WeaponType == EOBWeaponType::Melee && Weapon->Melee.Reach <= 0.f)
		{
			LocalErrors.Add(FString::Printf(TEXT("Melee weapon %s has invalid reach."), *Pair.Key.ToString()));
		}
	}

	for (const TPair<FGameplayTag, const FOBAttachmentDefinitionRow*>& Pair : AttachmentCache)
	{
		if (!ItemCache.Contains(Pair.Key))
		{
			LocalErrors.Add(FString::Printf(TEXT("Attachment %s has no DT_Items row."), *Pair.Key.ToString()));
		}
	}

	if (LoadedPlayerArchetypeTable)
	{
		const UOBGameDataSettings* Settings = GetDefault<UOBGameDataSettings>();
		const FName DefaultRow = Settings ? Settings->DefaultPlayerArchetype : FName(TEXT("Default"));
		const FOBPlayerArchetypeRow* Player = FindPlayerArchetype(DefaultRow);
		if (!Player)
		{
			LocalErrors.Add(FString::Printf(TEXT("Default player archetype '%s' is missing."), *DefaultRow.ToString()));
		}
		else if (Player->BaseStats.MaxHealth <= 0.f || Player->BaseStats.MoveSpeedMultiplier <= 0.f)
		{
			LocalErrors.Add(FString::Printf(TEXT("Player archetype '%s' has invalid base stats."), *DefaultRow.ToString()));
		}
	}

	if (OutErrors)
	{
		*OutErrors = MoveTemp(LocalErrors);
		return OutErrors->IsEmpty();
	}
	return LocalErrors.IsEmpty();
}
