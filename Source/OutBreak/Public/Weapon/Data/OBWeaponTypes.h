#pragma once

#include "CoreMinimal.h"
#include "OBWeaponTypes.generated.h"

// 무기 카테고리(슬롯/분류).
UENUM(BlueprintType)
enum class EOBWeaponCategory : uint8
{
	AssaultRifle UMETA(DisplayName = "Assault Rifle"),
	SniperRifle  UMETA(DisplayName = "Sniper Rifle"),
	SMG          UMETA(DisplayName = "SMG"),
	Shotgun      UMETA(DisplayName = "Shotgun"),
	Pistol       UMETA(DisplayName = "Pistol"),
	Melee        UMETA(DisplayName = "Melee"),
};

UENUM(BlueprintType)
enum class EOBWeaponFireMode : uint8
{
	Single   UMETA(DisplayName = "Single"),
	Burst    UMETA(DisplayName = "Burst"),
	FullAuto UMETA(DisplayName = "Full Auto")
};

UENUM(BlueprintType)
enum class EOBWeaponType : uint8
{
	Ranged UMETA(DisplayName = "Ranged"),
	Melee  UMETA(DisplayName = "Melee")
};

UENUM(BlueprintType)
enum class EOBWeaponSlot : uint8
{
	Primary   UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary"),
	Melee     UMETA(DisplayName = "Melee")
};
