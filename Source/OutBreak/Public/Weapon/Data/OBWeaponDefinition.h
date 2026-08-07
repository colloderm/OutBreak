#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Weapon/Data/OBWeaponTypes.h"
#include "OBWeaponDefinition.generated.h"

class AOBWeaponBase;
class UAnimMontage;
class UAnimSequence;
class UBlendSpace;
class UCameraShakeBase;
class UGameplayEffect;
class UNiagaraSystem;
class UOBAbilitySet;
class USkeletalMesh;
class USoundBase;
class USoundCue;
class UStaticMesh;

UENUM(BlueprintType)
enum class EOBStatModifierOperation : uint8
{
	Add,
	Multiply,
	Override,
	ClampMin,
	ClampMax
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	EOBStatModifierOperation Operation = EOBStatModifierOperation::Add;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	float Magnitude = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBAttachmentSlotSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FName MeshSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FGameplayTagContainer AllowedAttachmentTags;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBWeaponCommonSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.0"))
	float BaseDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.0", Units = "cm"))
	float EffectiveRange = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.05"))
	float MobilityMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TSoftObjectPtr<UOBAbilitySet> AbilitySet;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBWeaponVisualSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FName AttachSocket = TEXT("hand_r_Socket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UBlendSpace> OverlayLocomotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UBlendSpace> AimOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> ADSPose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> SprintPose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> ReloadMontage;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBRangedWeaponSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", Meta = (Categories = "Ammo"))
	FGameplayTag AmmoType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", Meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", Meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 120;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1.0"))
	float RoundsPerMinute = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	EOBWeaponFireMode FireMode = EOBWeaponFireMode::Single;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1"))
	int32 BurstCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1"))
	int32 PelletsPerShot = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundCue> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float VerticalRecoil = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float HorizontalRecoil = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilRecoverySpeed = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	TSubclassOf<UCameraShakeBase> FireCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", Meta = (ClampMin = "0.0"))
	float FireCameraShakeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS")
	float ADSFOV = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS")
	float ADSSpeedMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS")
	float ADSBlendSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS")
	float ADSRecoilMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread", Meta = (ClampMin = "0.0"))
	float BaseSpreadDegrees = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread", Meta = (ClampMin = "0.0"))
	float ADSSpreadMultiplier = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread", Meta = (ClampMin = "0.0"))
	float MovingSpreadMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", Meta = (ClampMin = "0.0"))
	float FireFocusPulse = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	FName MuzzleSocket = TEXT("Muzzle");
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBMeleeWeaponSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", Meta = (ClampMin = "0.0", Units = "cm"))
	float Reach = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", Meta = (ClampMin = "0.0", Units = "cm"))
	float SweepRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", Meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float ArcDegrees = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", Meta = (ClampMin = "0.0", Units = "s"))
	float HitTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", Meta = (ClampMin = "0.01", Units = "s"))
	float AttackDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", Meta = (Units = "cm"))
	float TraceHeight = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", Meta = (ClampMin = "0.0"))
	float StaminaCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	int32 MaxTargets = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FName TraceStartSocket = TEXT("TraceStart");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FName TraceEndSocket = TEXT("TraceEnd");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	TSoftObjectPtr<USoundBase> SwingSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	TSoftObjectPtr<UNiagaraSystem> SwingTrailVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FName TrailSocketName = TEXT("TrailSocket");
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBWeaponDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EOBWeaponType WeaponType = EOBWeaponType::Ranged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EOBWeaponSlot WeaponSlot = EOBWeaponSlot::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EOBWeaponCategory WeaponCategory = EOBWeaponCategory::AssaultRifle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TSoftClassPtr<AOBWeaponBase> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FOBWeaponCommonSpec Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FOBWeaponVisualSpec Visual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	FOBRangedWeaponSpec Ranged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", Meta = (EditCondition = "WeaponType == EOBWeaponType::Melee", EditConditionHides))
	FOBMeleeWeaponSpec Melee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	TArray<FOBAttachmentSlotSpec> AttachmentSlots;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBAttachmentDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FGameplayTagContainer RequiredWeaponTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FGameplayTagContainer BlockedWeaponTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	TArray<FOBStatModifier> Modifiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FName AttachSocket = NAME_None;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBResolvedWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly)
	EOBWeaponType WeaponType = EOBWeaponType::Ranged;

	UPROPERTY(BlueprintReadOnly)
	EOBWeaponSlot WeaponSlot = EOBWeaponSlot::Primary;

	UPROPERTY(BlueprintReadOnly)
	EOBWeaponCategory WeaponCategory = EOBWeaponCategory::AssaultRifle;

	UPROPERTY(BlueprintReadOnly)
	float Damage = 20.f;

	UPROPERTY(BlueprintReadOnly)
	float Range = 10000.f;

	UPROPERTY(BlueprintReadOnly)
	float MobilityMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly)
	float HeadshotMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag AmmoType;

	UPROPERTY(BlueprintReadOnly)
	int32 MagazineSize = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly)
	float RoundsPerMinute = 0.f;

	UPROPERTY(BlueprintReadOnly)
	EOBWeaponFireMode FireMode = EOBWeaponFireMode::Single;

	UPROPERTY(BlueprintReadOnly)
	int32 BurstCount = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 PelletsPerShot = 1;

	UPROPERTY(BlueprintReadOnly)
	float VerticalRecoil = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float HorizontalRecoil = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float RecoilRecoverySpeed = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float ADSFOV = 90.f;

	UPROPERTY(BlueprintReadOnly)
	float ADSSpeedMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly)
	float ADSBlendSpeed = 12.f;

	UPROPERTY(BlueprintReadOnly)
	float ADSRecoilMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly)
	float BaseSpreadDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float ADSSpreadMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly)
	float MovingSpreadMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly)
	float FireFocusPulse = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float MeleeReach = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float MeleeSweepRadius = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float MeleeArcDegrees = 90.f;

	UPROPERTY(BlueprintReadOnly)
	float MeleeHitTime = 0.2f;

	UPROPERTY(BlueprintReadOnly)
	float MeleeAttackDuration = 0.6f;

	UPROPERTY(BlueprintReadOnly)
	float MeleeTraceHeight = 40.f;

	UPROPERTY(BlueprintReadOnly)
	float MeleeStaminaCost = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 MeleeMaxTargets = 0;

	UPROPERTY(BlueprintReadOnly)
	float Durability = 1.f;
};

