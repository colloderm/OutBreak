#include "OBGenerateDataTablesCommandlet.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/OBGameDataSubsystem.h"
#include "Engine/DataTable.h"
#include "Item/Data/OBItemDefinition.h"
#include "Misc/PackageName.h"
#include "Player/Data/OBPlayerStatData.h"
#include "UObject/SavePackage.h"
#include "Weapon/Data/OBWeaponDefinition.h"

namespace
{
	UDataTable* CreateOrResetTable(
		const TCHAR* PackageName,
		const TCHAR* AssetName,
		UScriptStruct* RowStruct)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), PackageName, AssetName);
		UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath);
		if (!Table)
		{
			UPackage* Package = CreatePackage(PackageName);
			Table = NewObject<UDataTable>(
				Package,
				AssetName,
				RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Table);
		}
		if (Table->RowStruct)
		{
			Table->EmptyTable();
		}
		Table->RowStruct = RowStruct;
		Table->MarkPackageDirty();
		return Table;
	}

	bool SaveTable(UDataTable* Table)
	{
		if (!Table) return false;
		UPackage* Package = Table->GetPackage();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.Error = GError;
		return UPackage::SavePackage(Package, Table, *Filename, SaveArgs);
	}

	FName MakeRowName(const FGameplayTag& Tag)
	{
		FString Name = Tag.ToString();
		Name.ReplaceInline(TEXT("."), TEXT("_"));
		return FName(Name);
	}
}

UOBGenerateDataTablesCommandlet::UOBGenerateDataTablesCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UOBGenerateDataTablesCommandlet::Main(const FString& Params)
{
	constexpr const TCHAR* DataPath = TEXT("/Game/GameAbilitySystem/DataAssets/Data");
	UDataTable* ExistingWeapons = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_Weapons.DT_Weapons"));
	UDataTable* ExistingAttachments = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_Attachments.DT_Attachments"));
	UDataTable* ExistingPlayers = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_PlayerArchetypes.DT_PlayerArchetypes"));
	UDataTable* ExistingStats = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_StatDisplay.DT_StatDisplay"));
	if (ExistingWeapons && ExistingAttachments && ExistingPlayers && ExistingStats)
	{
		const bool bCorrectSchemas =
			ExistingWeapons->GetRowStruct() == FOBWeaponDefinitionRow::StaticStruct() &&
			ExistingAttachments->GetRowStruct() == FOBAttachmentDefinitionRow::StaticStruct() &&
			ExistingPlayers->GetRowStruct() == FOBPlayerArchetypeRow::StaticStruct() &&
			ExistingStats->GetRowStruct() == FOBStatDisplayRow::StaticStruct();
		bool bWeaponRowsValid = bCorrectSchemas && !ExistingWeapons->GetRowMap().IsEmpty();
		if (bWeaponRowsValid)
		{
			for (const TPair<FName, uint8*>& Pair : ExistingWeapons->GetRowMap())
			{
				const FOBWeaponDefinitionRow* Row = reinterpret_cast<const FOBWeaponDefinitionRow*>(Pair.Value);
				if (!Row || !Row->ItemTag.IsValid() || Row->ActorClass.IsNull())
				{
					bWeaponRowsValid = false;
					break;
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[OBDataGen] Existing tables are valid. Weapons=%d Players=%d Stats=%d"),
			ExistingWeapons->GetRowMap().Num(),
			ExistingPlayers->GetRowMap().Num(),
			ExistingStats->GetRowMap().Num());
		return bWeaponRowsValid ? 0 : 3;
	}

	UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get();
	if (!GameData)
	{
		UE_LOG(LogTemp, Error, TEXT("[OBDataGen] Game data subsystem is unavailable."));
		return 1;
	}
	GameData->ReloadGameData();

	UDataTable* WeaponTable = CreateOrResetTable(
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_Weapons"),
		TEXT("DT_Weapons"),
		FOBWeaponDefinitionRow::StaticStruct());
	TArray<const FOBItemDefinitionRow*> Items;
	GameData->GetAllItems(Items);
	for (const FOBItemDefinitionRow* Item : Items)
	{
		if (!Item || Item->Category != EOBItemCategory::Weapon) continue;
		if (const FOBWeaponDefinitionRow* Weapon = GameData->FindWeapon(Item->ItemTag))
		{
			WeaponTable->AddRow(MakeRowName(Item->ItemTag), *Weapon);
		}
	}

	UDataTable* AttachmentTable = CreateOrResetTable(
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_Attachments"),
		TEXT("DT_Attachments"),
		FOBAttachmentDefinitionRow::StaticStruct());

	UDataTable* PlayerTable = CreateOrResetTable(
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_PlayerArchetypes"),
		TEXT("DT_PlayerArchetypes"),
		FOBPlayerArchetypeRow::StaticStruct());
	FOBPlayerArchetypeRow DefaultPlayer;
	DefaultPlayer.DisplayName = NSLOCTEXT("OBPlayerData", "DefaultPlayer", "Default");
	PlayerTable->AddRow(TEXT("Default"), DefaultPlayer);

	UDataTable* StatTable = CreateOrResetTable(
		TEXT("/Game/GameAbilitySystem/DataAssets/Data/DT_StatDisplay"),
		TEXT("DT_StatDisplay"),
		FOBStatDisplayRow::StaticStruct());
	int32 SortOrder = 0;
	auto AddStat = [StatTable, &SortOrder](
		const FGameplayTag& Tag,
		const TCHAR* Label,
		const TCHAR* Unit,
		const EOBStatValuePolarity Polarity)
	{
		FOBStatDisplayRow Row;
		Row.StatTag = Tag;
		Row.DisplayName = FText::FromString(Label);
		Row.UnitText = FText::FromString(Unit);
		Row.SortOrder = SortOrder;
		Row.Polarity = Polarity;
		StatTable->AddRow(MakeRowName(Tag), Row);
		SortOrder += 10;
	};
	using namespace OBGameplayTags;
	AddStat(Stat_Weapon_Damage, TEXT("Damage"), TEXT(""), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Weapon_Range, TEXT("Range"), TEXT("cm"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Weapon_Mobility, TEXT("Mobility"), TEXT("x"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Weapon_MagazineSize, TEXT("Magazine"), TEXT(""), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Weapon_RoundsPerMinute, TEXT("RPM"), TEXT(""), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Weapon_RecoilVertical, TEXT("Vertical recoil"), TEXT("deg"), EOBStatValuePolarity::LowerIsBetter);
	AddStat(Stat_Weapon_RecoilHorizontal, TEXT("Horizontal recoil"), TEXT("deg"), EOBStatValuePolarity::LowerIsBetter);
	AddStat(Stat_Weapon_SpreadBase, TEXT("Spread"), TEXT("deg"), EOBStatValuePolarity::LowerIsBetter);
	AddStat(Stat_Melee_Reach, TEXT("Reach"), TEXT("cm"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Melee_SweepRadius, TEXT("Sweep radius"), TEXT("cm"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Melee_Arc, TEXT("Attack arc"), TEXT("deg"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Melee_AttackDuration, TEXT("Attack duration"), TEXT("s"), EOBStatValuePolarity::LowerIsBetter);
	AddStat(Stat_Melee_StaminaCost, TEXT("Stamina cost"), TEXT(""), EOBStatValuePolarity::LowerIsBetter);
	AddStat(Stat_Player_MaxHealth, TEXT("Max health"), TEXT(""), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Player_MaxStamina, TEXT("Max stamina"), TEXT(""), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Player_MoveSpeed, TEXT("Move speed"), TEXT("x"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Player_CarryCapacity, TEXT("Carry capacity"), TEXT("kg"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Player_RecoilControl, TEXT("Recoil control"), TEXT("x"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Player_AimStability, TEXT("Aim stability"), TEXT("x"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Player_MeleePower, TEXT("Melee power"), TEXT("x"), EOBStatValuePolarity::HigherIsBetter);
	AddStat(Stat_Player_Armor, TEXT("Armor"), TEXT(""), EOBStatValuePolarity::HigherIsBetter);

	const bool bSaved = SaveTable(WeaponTable) &&
		SaveTable(AttachmentTable) &&
		SaveTable(PlayerTable) &&
		SaveTable(StatTable);
	UE_LOG(LogTemp, Display, TEXT("[OBDataGen] Weapons=%d Saved=%s"),
		WeaponTable ? WeaponTable->GetRowMap().Num() : 0,
		bSaved ? TEXT("true") : TEXT("false"));
	return bSaved ? 0 : 2;
}
