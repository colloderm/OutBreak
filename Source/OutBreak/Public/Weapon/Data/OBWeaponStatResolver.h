#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/Data/InventoryData.h"
#include "Weapon/Data/OBWeaponDefinition.h"
#include "OBWeaponStatResolver.generated.h"

class UAbilitySystemComponent;

UCLASS()
class OUTBREAK_API UOBWeaponStatResolver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Produces the authoritative runtime view from static weapon/attachment rows
	// plus the unique item instance. Call again whenever Revision changes.
	UFUNCTION(BlueprintPure, Category = "OutBreak|Weapon Data")
	static bool ResolveWeaponStats(
		const FInventoryData& ItemInstance,
		const UAbilitySystemComponent* OwnerAbilitySystem,
		FOBResolvedWeaponStats& OutStats);

private:
	static void ApplyModifier(
		FOBResolvedWeaponStats& Stats,
		const FOBStatModifier& Modifier);
	static float ApplyOperation(
		float Current,
		EOBStatModifierOperation Operation,
		float Magnitude);
};
