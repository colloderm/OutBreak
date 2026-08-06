#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "OBPlayerStatData.generated.h"

class UGameplayEffect;
class UOBAbilitySet;

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBPlayerBaseStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vital", Meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vital", Meta = (ClampMin = "0.0"))
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vital", Meta = (ClampMin = "0.0"))
	float HealthRegen = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vital", Meta = (ClampMin = "0.0"))
	float StaminaRegen = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mobility", Meta = (ClampMin = "0.05"))
	float MoveSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", Meta = (ClampMin = "0.0", Units = "kg"))
	float CarryCapacity = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.05"))
	float RecoilControl = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.05"))
	float AimStability = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.0"))
	float MeleePower = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense", Meta = (ClampMin = "0.0"))
	float Armor = 0.f;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBPlayerArchetypeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	FOBPlayerBaseStats BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bootstrap")
	TSubclassOf<UGameplayEffect> InitialStatsEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bootstrap")
	TSoftObjectPtr<UOBAbilitySet> DefaultAbilitySet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bootstrap", Meta = (Categories = "Item"))
	TArray<FGameplayTag> DefaultLoadoutItems;
};

UENUM(BlueprintType)
enum class EOBStatValuePolarity : uint8
{
	Neutral,
	HigherIsBetter,
	LowerIsBetter
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBStatDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText UnitText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display", Meta = (ClampMin = "0", ClampMax = "4"))
	int32 DecimalPlaces = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	EOBStatValuePolarity Polarity = EOBStatValuePolarity::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FGameplayTagContainer ContextTags;
};

