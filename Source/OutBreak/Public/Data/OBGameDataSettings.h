#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OBGameDataSettings.generated.h"

class UDataTable;

UCLASS(Config = Game, DefaultConfig, Meta = (DisplayName = "OutBreak Game Data"))
class OUTBREAK_API UOBGameDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditDefaultsOnly, Category = "Tables", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> ItemTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Tables", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> WeaponTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Tables", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> AttachmentTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Tables", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> PlayerArchetypeTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Tables", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> StatDisplayTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Tables", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> LootTable;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Player")
	FName DefaultPlayerArchetype = TEXT("Default");

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};

