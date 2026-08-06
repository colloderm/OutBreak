#include "Weapon/Data/OBWeaponStatResolver.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Ability/Attributes/OBAttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "Data/OBGameDataSubsystem.h"

namespace
{
	struct FOBPendingModifier
	{
		FGameplayTag AttachmentTag;
		const FOBStatModifier* Modifier = nullptr;
	};
}

bool UOBWeaponStatResolver::ResolveWeaponStats(
	const FInventoryData& ItemInstance,
	const UAbilitySystemComponent* OwnerAbilitySystem,
	FOBResolvedWeaponStats& OutStats)
{
	OutStats = FOBResolvedWeaponStats();
	const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get();
	const FOBWeaponDefinitionRow* Row = GameData
		? GameData->FindWeapon(ItemInstance.ItemTag)
		: nullptr;
	if (!Row)
	{
		return false;
	}

	OutStats.ItemTag = Row->ItemTag;
	OutStats.WeaponType = Row->WeaponType;
	OutStats.WeaponSlot = Row->WeaponSlot;
	OutStats.WeaponCategory = Row->WeaponCategory;
	OutStats.Damage = Row->Common.BaseDamage;
	OutStats.Range = Row->Common.EffectiveRange;
	OutStats.MobilityMultiplier = Row->Common.MobilityMultiplier;
	OutStats.HeadshotMultiplier = Row->Common.HeadshotMultiplier;
	OutStats.AmmoType = Row->Ranged.AmmoType;
	OutStats.MagazineSize = Row->WeaponType == EOBWeaponType::Ranged ? Row->Ranged.MagazineSize : 0;
	OutStats.MaxReserveAmmo = Row->WeaponType == EOBWeaponType::Ranged ? Row->Ranged.MaxReserveAmmo : 0;
	OutStats.RoundsPerMinute = Row->Ranged.RoundsPerMinute;
	OutStats.FireMode = Row->Ranged.FireMode;
	OutStats.BurstCount = Row->Ranged.BurstCount;
	OutStats.PelletsPerShot = Row->Ranged.PelletsPerShot;
	OutStats.VerticalRecoil = Row->Ranged.VerticalRecoil;
	OutStats.HorizontalRecoil = Row->Ranged.HorizontalRecoil;
	OutStats.RecoilRecoverySpeed = Row->Ranged.RecoilRecoverySpeed;
	OutStats.ADSFOV = Row->Ranged.ADSFOV;
	OutStats.ADSSpeedMultiplier = Row->Ranged.ADSSpeedMultiplier;
	OutStats.ADSBlendSpeed = Row->Ranged.ADSBlendSpeed;
	OutStats.ADSRecoilMultiplier = Row->Ranged.ADSRecoilMultiplier;
	OutStats.BaseSpreadDegrees = Row->Ranged.BaseSpreadDegrees;
	OutStats.ADSSpreadMultiplier = Row->Ranged.ADSSpreadMultiplier;
	OutStats.MovingSpreadMultiplier = Row->Ranged.MovingSpreadMultiplier;
	OutStats.FireFocusPulse = Row->Ranged.FireFocusPulse;
	OutStats.MeleeReach = Row->Melee.Reach;
	OutStats.MeleeSweepRadius = Row->Melee.SweepRadius;
	OutStats.MeleeArcDegrees = Row->Melee.ArcDegrees;
	OutStats.MeleeHitTime = Row->Melee.HitTime;
	OutStats.MeleeAttackDuration = Row->Melee.AttackDuration;
	OutStats.MeleeTraceHeight = Row->Melee.TraceHeight;
	OutStats.MeleeStaminaCost = Row->Melee.StaminaCost;
	OutStats.MeleeMaxTargets = Row->Melee.MaxTargets;
	OutStats.Durability = FMath::Clamp(ItemInstance.Durability, 0.f, 1.f);

	TArray<FOBPendingModifier> Pending;
	for (const FOBInstalledAttachment& Installed : ItemInstance.Attachments)
	{
		const FOBAttachmentDefinitionRow* Attachment = GameData->FindAttachment(Installed.ItemTag);
		if (!Attachment || Attachment->SlotTag != Installed.SlotTag)
		{
			continue;
		}
		for (const FOBStatModifier& Modifier : Attachment->Modifiers)
		{
			Pending.Add({Installed.ItemTag, &Modifier});
		}
	}
	Pending.Sort([](const FOBPendingModifier& A, const FOBPendingModifier& B)
	{
		const int32 APriority = A.Modifier ? A.Modifier->Priority : 0;
		const int32 BPriority = B.Modifier ? B.Modifier->Priority : 0;
		if (APriority != BPriority)
		{
			return APriority < BPriority;
		}
		return A.AttachmentTag.ToString() < B.AttachmentTag.ToString();
	});
	for (const FOBPendingModifier& Entry : Pending)
	{
		if (Entry.Modifier)
		{
			ApplyModifier(OutStats, *Entry.Modifier);
		}
	}

	if (OwnerAbilitySystem)
	{
		if (const UOBAttributeSetBase* Attributes = OwnerAbilitySystem->GetSet<UOBAttributeSetBase>())
		{
			const float RecoilControl = FMath::Max(0.05f, Attributes->GetRecoilControl());
			const float AimStability = FMath::Max(0.05f, Attributes->GetAimStability());
			OutStats.VerticalRecoil /= RecoilControl;
			OutStats.HorizontalRecoil /= RecoilControl;
			OutStats.BaseSpreadDegrees /= AimStability;
			if (OutStats.WeaponType == EOBWeaponType::Melee)
			{
				OutStats.Damage *= FMath::Max(0.f, Attributes->GetMeleePower());
			}
		}
	}

	OutStats.Damage = FMath::Max(0.f, OutStats.Damage);
	OutStats.Range = FMath::Max(0.f, OutStats.Range);
	OutStats.MobilityMultiplier = FMath::Max(0.05f, OutStats.MobilityMultiplier);
	OutStats.HeadshotMultiplier = FMath::Max(1.f, OutStats.HeadshotMultiplier);
	OutStats.MagazineSize = Row->WeaponType == EOBWeaponType::Ranged ? FMath::Max(1, OutStats.MagazineSize) : 0;
	OutStats.MaxReserveAmmo = FMath::Max(0, OutStats.MaxReserveAmmo);
	OutStats.RoundsPerMinute = FMath::Max(1.f, OutStats.RoundsPerMinute);
	OutStats.BurstCount = FMath::Max(1, OutStats.BurstCount);
	OutStats.PelletsPerShot = FMath::Max(1, OutStats.PelletsPerShot);
	OutStats.MeleeReach = FMath::Max(0.f, OutStats.MeleeReach);
	OutStats.MeleeSweepRadius = FMath::Max(0.f, OutStats.MeleeSweepRadius);
	OutStats.MeleeArcDegrees = FMath::Clamp(OutStats.MeleeArcDegrees, 0.f, 180.f);

	return true;
}

float UOBWeaponStatResolver::ApplyOperation(
	const float Current,
	const EOBStatModifierOperation Operation,
	const float Magnitude)
{
	switch (Operation)
	{
	case EOBStatModifierOperation::Add:      return Current + Magnitude;
	case EOBStatModifierOperation::Multiply: return Current * Magnitude;
	case EOBStatModifierOperation::Override: return Magnitude;
	case EOBStatModifierOperation::ClampMin: return FMath::Max(Current, Magnitude);
	case EOBStatModifierOperation::ClampMax: return FMath::Min(Current, Magnitude);
	default:                                 return Current;
	}
}

void UOBWeaponStatResolver::ApplyModifier(
	FOBResolvedWeaponStats& Stats,
	const FOBStatModifier& Modifier)
{
	using namespace OBGameplayTags;
	auto Apply = [&Modifier](float& Value)
	{
		Value = ApplyOperation(Value, Modifier.Operation, Modifier.Magnitude);
	};
	auto ApplyInt = [&Modifier](int32& Value)
	{
		Value = FMath::RoundToInt(ApplyOperation(
			static_cast<float>(Value), Modifier.Operation, Modifier.Magnitude));
	};

	if (Modifier.StatTag == Stat_Weapon_Damage) Apply(Stats.Damage);
	else if (Modifier.StatTag == Stat_Weapon_Range) Apply(Stats.Range);
	else if (Modifier.StatTag == Stat_Weapon_Mobility) Apply(Stats.MobilityMultiplier);
	else if (Modifier.StatTag == Stat_Weapon_HeadshotMultiplier) Apply(Stats.HeadshotMultiplier);
	else if (Modifier.StatTag == Stat_Weapon_MagazineSize) ApplyInt(Stats.MagazineSize);
	else if (Modifier.StatTag == Stat_Weapon_RoundsPerMinute) Apply(Stats.RoundsPerMinute);
	else if (Modifier.StatTag == Stat_Weapon_RecoilVertical) Apply(Stats.VerticalRecoil);
	else if (Modifier.StatTag == Stat_Weapon_RecoilHorizontal) Apply(Stats.HorizontalRecoil);
	else if (Modifier.StatTag == Stat_Weapon_RecoilRecovery) Apply(Stats.RecoilRecoverySpeed);
	else if (Modifier.StatTag == Stat_Weapon_ADSFOV) Apply(Stats.ADSFOV);
	else if (Modifier.StatTag == Stat_Weapon_ADSSpeed) Apply(Stats.ADSSpeedMultiplier);
	else if (Modifier.StatTag == Stat_Weapon_ADSRecoil) Apply(Stats.ADSRecoilMultiplier);
	else if (Modifier.StatTag == Stat_Weapon_SpreadBase) Apply(Stats.BaseSpreadDegrees);
	else if (Modifier.StatTag == Stat_Weapon_SpreadADS) Apply(Stats.ADSSpreadMultiplier);
	else if (Modifier.StatTag == Stat_Weapon_SpreadMoving) Apply(Stats.MovingSpreadMultiplier);
	else if (Modifier.StatTag == Stat_Melee_Reach) Apply(Stats.MeleeReach);
	else if (Modifier.StatTag == Stat_Melee_SweepRadius) Apply(Stats.MeleeSweepRadius);
	else if (Modifier.StatTag == Stat_Melee_Arc) Apply(Stats.MeleeArcDegrees);
	else if (Modifier.StatTag == Stat_Melee_HitTime) Apply(Stats.MeleeHitTime);
	else if (Modifier.StatTag == Stat_Melee_AttackDuration) Apply(Stats.MeleeAttackDuration);
	else if (Modifier.StatTag == Stat_Melee_StaminaCost) Apply(Stats.MeleeStaminaCost);
}
