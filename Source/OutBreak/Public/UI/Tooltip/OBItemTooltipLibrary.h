#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/Data/InventoryData.h"
#include "Item/Data/OBItemTypes.h"
#include "OBItemTooltipLibrary.generated.h"

class UAbilitySystemComponent;
class UTexture2D;

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTooltipStatLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag StatTag;

	UPROPERTY(BlueprintReadOnly)
	FText Label;

	UPROPERTY(BlueprintReadOnly)
	float Value = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float ComparisonDelta = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FText UnitText;

	UPROPERTY(BlueprintReadOnly)
	int32 DecimalPlaces = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 SortOrder = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bHigherIsBetter = true;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBItemTooltipViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly)
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FText Description;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly)
	EOBItemCategory Category = EOBItemCategory::Material;

	UPROPERTY(BlueprintReadOnly)
	int32 StackCount = 0;

	UPROPERTY(BlueprintReadOnly)
	float UnitWeight = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float TotalWeight = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 BuyPrice = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SellPrice = 0;

	UPROPERTY(BlueprintReadOnly)
	float Durability = 1.f;

	UPROPERTY(BlueprintReadOnly)
	int32 QualityTier = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bFoundInRaid = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FText> AttachmentNames;

	UPROPERTY(BlueprintReadOnly)
	TArray<FOBTooltipStatLine> StatLines;
};

UCLASS()
class OUTBREAK_API UOBItemTooltipLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OutBreak|Item Tooltip")
	static bool BuildItemTooltip(
		const FInventoryData& ItemInstance,
		const UAbilitySystemComponent* OwnerAbilitySystem,
		FOBItemTooltipViewModel& OutTooltip);

	UFUNCTION(BlueprintPure, Category = "OutBreak|Item Tooltip")
	static bool BuildItemComparisonTooltip(
		const FInventoryData& ItemInstance,
		const FInventoryData& ComparedInstance,
		const UAbilitySystemComponent* OwnerAbilitySystem,
		FOBItemTooltipViewModel& OutTooltip);

	UFUNCTION(BlueprintPure, Category = "OutBreak|Item Tooltip")
	static FText BuildFallbackTooltipText(const FInventoryData& ItemInstance);

private:
	static bool BuildInternal(
		const FInventoryData& ItemInstance,
		const FInventoryData* ComparedInstance,
		const UAbilitySystemComponent* OwnerAbilitySystem,
		FOBItemTooltipViewModel& OutTooltip);
};
