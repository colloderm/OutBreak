#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "GameplayTagContainer.h"
#include "Weapon/Data/OBWeaponDefinition.h"
#include "OBGameDataSubsystem.generated.h"

class UDataTable;
struct FOBAttachmentDefinitionRow;
struct FOBItemDefinitionRow;
struct FOBPlayerArchetypeRow;
struct FOBStatDisplayRow;

UCLASS()
class OUTBREAK_API UOBGameDataSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static UOBGameDataSubsystem* Get();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FOBItemDefinitionRow* FindItem(const FGameplayTag& ItemTag) const;
	const FOBWeaponDefinitionRow* FindWeapon(const FGameplayTag& ItemTag) const;
	const FOBAttachmentDefinitionRow* FindAttachment(const FGameplayTag& ItemTag) const;
	const FOBPlayerArchetypeRow* FindPlayerArchetype(FName RowName) const;
	const FOBStatDisplayRow* FindStatDisplay(const FGameplayTag& StatTag) const;
	FGameplayTag FindTagForWeaponClass(const UClass* WeaponClass) const;
	void GetAllItems(TArray<const FOBItemDefinitionRow*>& OutItems) const;

	UDataTable* GetLootTable() const { return LoadedLootTable; }
	UDataTable* GetWeaponTable() const { return LoadedWeaponTable; }

	UFUNCTION(Exec)
	void ReloadGameData();

	bool ValidateLoadedData(TArray<FString>* OutErrors = nullptr) const;

private:
	void LoadTables();
	void RebuildCaches();
	void BuildLegacyWeaponRows();

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedItemTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedWeaponTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedAttachmentTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedPlayerArchetypeTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedStatDisplayTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedLootTable;

	TMap<FGameplayTag, const FOBItemDefinitionRow*> ItemCache;
	TMap<FGameplayTag, const FOBWeaponDefinitionRow*> WeaponCache;
	TMap<FGameplayTag, const FOBAttachmentDefinitionRow*> AttachmentCache;
	TMap<FGameplayTag, const FOBStatDisplayRow*> StatDisplayCache;
	TMap<FSoftObjectPath, FGameplayTag> WeaponPathToTag;
	TMap<FGameplayTag, FOBWeaponDefinitionRow> LegacyWeaponRows;
};
