#pragma once

#include "CoreMinimal.h"
#include "OBEquipmentTypes.generated.h"

// Logical equipment positions shared by inventory, UI and equipment data assets.
// Weapon items derive their position from UOBWeaponData::WeaponSlot.
UENUM(BlueprintType)
enum class EOBEquipmentSlot : uint8
{
	None,
	PrimaryWeapon,
	SecondaryWeapon,
	MeleeWeapon,
	Backpack,
	Head,
	Chest,
	Hands,
	Legs,
	Feet,
	MAX UMETA(Hidden)
};
