#include "UI/Tooltip/OBItemTooltipLibrary.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Data/OBGameDataSubsystem.h"
#include "Item/Data/OBItemDefinition.h"
#include "Item/OBItemRegistry.h"
#include "Player/Data/OBPlayerStatData.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Weapon/Data/OBWeaponDefinition.h"
#include "Weapon/Data/OBWeaponStatResolver.h"
#include "Weapon/OBWeaponBase.h"

namespace
{
	FText GetFallbackStatLabel(const FGameplayTag& Tag)
	{
		using namespace OBGameplayTags;
		if (Tag == Stat_Weapon_Damage) return NSLOCTEXT("OBTooltip", "Damage", "Damage");
		if (Tag == Stat_Weapon_Range) return NSLOCTEXT("OBTooltip", "Range", "Range");
		if (Tag == Stat_Weapon_Mobility) return NSLOCTEXT("OBTooltip", "Mobility", "Mobility");
		if (Tag == Stat_Weapon_MagazineSize) return NSLOCTEXT("OBTooltip", "Magazine", "Magazine");
		if (Tag == Stat_Weapon_RoundsPerMinute) return NSLOCTEXT("OBTooltip", "RPM", "RPM");
		if (Tag == Stat_Weapon_RecoilVertical) return NSLOCTEXT("OBTooltip", "VerticalRecoil", "Vertical recoil");
		if (Tag == Stat_Weapon_RecoilHorizontal) return NSLOCTEXT("OBTooltip", "HorizontalRecoil", "Horizontal recoil");
		if (Tag == Stat_Weapon_SpreadBase) return NSLOCTEXT("OBTooltip", "Spread", "Spread");
		if (Tag == Stat_Melee_Reach) return NSLOCTEXT("OBTooltip", "Reach", "Reach");
		if (Tag == Stat_Melee_SweepRadius) return NSLOCTEXT("OBTooltip", "Sweep", "Sweep radius");
		if (Tag == Stat_Melee_Arc) return NSLOCTEXT("OBTooltip", "Arc", "Attack arc");
		if (Tag == Stat_Melee_AttackDuration) return NSLOCTEXT("OBTooltip", "AttackDuration", "Attack duration");
		if (Tag == Stat_Melee_StaminaCost) return NSLOCTEXT("OBTooltip", "StaminaCost", "Stamina cost");
		return FText::FromName(Tag.GetTagName());
	}

	void AddStat(
		FOBItemTooltipViewModel& Tooltip,
		const FGameplayTag& Tag,
		const float Value,
		const float ComparedValue,
		const bool bHasComparison,
		const FText& DefaultUnit,
		const int32 DefaultOrder,
		const bool bDefaultHigherIsBetter = true)
	{
		FOBTooltipStatLine& Line = Tooltip.StatLines.AddDefaulted_GetRef();
		Line.StatTag = Tag;
		Line.Value = Value;
		Line.ComparisonDelta = bHasComparison ? Value - ComparedValue : 0.f;
		Line.Label = GetFallbackStatLabel(Tag);
		Line.UnitText = DefaultUnit;
		Line.SortOrder = DefaultOrder;
		Line.bHigherIsBetter = bDefaultHigherIsBetter;

		if (const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get())
		{
			if (const FOBStatDisplayRow* Display = GameData->FindStatDisplay(Tag))
			{
				Line.Label = Display->DisplayName.IsEmpty() ? Line.Label : Display->DisplayName;
				Line.UnitText = Display->UnitText.IsEmpty() ? Line.UnitText : Display->UnitText;
				Line.DecimalPlaces = Display->DecimalPlaces;
				Line.SortOrder = Display->SortOrder;
				Line.bHigherIsBetter = Display->Polarity != EOBStatValuePolarity::LowerIsBetter;
			}
		}
	}
}

bool UOBItemTooltipLibrary::BuildItemTooltip(
	const FInventoryData& ItemInstance,
	const UAbilitySystemComponent* OwnerAbilitySystem,
	FOBItemTooltipViewModel& OutTooltip)
{
	return BuildInternal(ItemInstance, nullptr, OwnerAbilitySystem, OutTooltip);
}

bool UOBItemTooltipLibrary::BuildItemComparisonTooltip(
	const FInventoryData& ItemInstance,
	const FInventoryData& ComparedInstance,
	const UAbilitySystemComponent* OwnerAbilitySystem,
	FOBItemTooltipViewModel& OutTooltip)
{
	return BuildInternal(ItemInstance, &ComparedInstance, OwnerAbilitySystem, OutTooltip);
}

bool UOBItemTooltipLibrary::BuildInternal(
	const FInventoryData& ItemInstance,
	const FInventoryData* ComparedInstance,
	const UAbilitySystemComponent* OwnerAbilitySystem,
	FOBItemTooltipViewModel& OutTooltip)
{
	OutTooltip = FOBItemTooltipViewModel();
	const FOBItemDefinitionRow* ItemRow = ItemInstance.GetDefinition();
	if (!ItemRow)
	{
		return false;
	}

	OutTooltip.ItemTag = ItemInstance.ItemTag;
	OutTooltip.InstanceId = ItemInstance.InstanceId;
	OutTooltip.Description = ItemRow->Description;
	OutTooltip.Category = ItemRow->Category;
	OutTooltip.StackCount = ItemInstance.ItemStack;
	OutTooltip.UnitWeight = ItemRow->Weight;
	OutTooltip.TotalWeight = ItemRow->Weight * ItemInstance.ItemStack;
	OutTooltip.BuyPrice = ItemRow->BuyPrice;
	OutTooltip.SellPrice = ItemRow->SellPrice;
	OutTooltip.Durability = ItemInstance.Durability;
	OutTooltip.QualityTier = ItemInstance.QualityTier;
	OutTooltip.bFoundInRaid = ItemInstance.bFoundInRaid;
	UTexture2D* Icon = nullptr;
	UOBItemRegistry::GetItemDisplay(ItemInstance.ItemTag, OutTooltip.DisplayName, Icon);
	OutTooltip.Icon = Icon;

	for (const FOBInstalledAttachment& Installed : ItemInstance.Attachments)
	{
		FText AttachmentName;
		UTexture2D* AttachmentIcon = nullptr;
		UOBItemRegistry::GetItemDisplay(Installed.ItemTag, AttachmentName, AttachmentIcon);
		OutTooltip.AttachmentNames.Add(AttachmentName);
	}

	if (ItemRow->Category != EOBItemCategory::Weapon)
	{
		return true;
	}

	FOBResolvedWeaponStats Stats;
	if (!UOBWeaponStatResolver::ResolveWeaponStats(ItemInstance, OwnerAbilitySystem, Stats))
	{
		return true;
	}
	FOBResolvedWeaponStats Compared;
	const bool bHasComparison = ComparedInstance &&
		UOBWeaponStatResolver::ResolveWeaponStats(*ComparedInstance, OwnerAbilitySystem, Compared);
	using namespace OBGameplayTags;
	AddStat(OutTooltip, Stat_Weapon_Damage, Stats.Damage, Compared.Damage, bHasComparison, FText(), 10);
	AddStat(OutTooltip, Stat_Weapon_Range, Stats.Range, Compared.Range, bHasComparison, NSLOCTEXT("OBTooltip", "Centimeters", "cm"), 20);
	AddStat(OutTooltip, Stat_Weapon_Mobility, Stats.MobilityMultiplier, Compared.MobilityMultiplier, bHasComparison, FText(), 30);
	if (Stats.WeaponType == EOBWeaponType::Ranged)
	{
		AddStat(OutTooltip, Stat_Weapon_MagazineSize, Stats.MagazineSize, Compared.MagazineSize, bHasComparison, FText(), 40);
		AddStat(OutTooltip, Stat_Weapon_RoundsPerMinute, Stats.RoundsPerMinute, Compared.RoundsPerMinute, bHasComparison, FText(), 50);
		AddStat(OutTooltip, Stat_Weapon_RecoilVertical, Stats.VerticalRecoil, Compared.VerticalRecoil, bHasComparison, FText(), 60, false);
		AddStat(OutTooltip, Stat_Weapon_RecoilHorizontal, Stats.HorizontalRecoil, Compared.HorizontalRecoil, bHasComparison, FText(), 70, false);
		AddStat(OutTooltip, Stat_Weapon_SpreadBase, Stats.BaseSpreadDegrees, Compared.BaseSpreadDegrees, bHasComparison, NSLOCTEXT("OBTooltip", "Degrees", "deg"), 80, false);
	}
	else
	{
		AddStat(OutTooltip, Stat_Melee_Reach, Stats.MeleeReach, Compared.MeleeReach, bHasComparison, NSLOCTEXT("OBTooltip", "Centimeters", "cm"), 40);
		AddStat(OutTooltip, Stat_Melee_SweepRadius, Stats.MeleeSweepRadius, Compared.MeleeSweepRadius, bHasComparison, NSLOCTEXT("OBTooltip", "Centimeters", "cm"), 50);
		AddStat(OutTooltip, Stat_Melee_Arc, Stats.MeleeArcDegrees, Compared.MeleeArcDegrees, bHasComparison, NSLOCTEXT("OBTooltip", "Degrees", "deg"), 60);
		AddStat(OutTooltip, Stat_Melee_AttackDuration, Stats.MeleeAttackDuration, Compared.MeleeAttackDuration, bHasComparison, NSLOCTEXT("OBTooltip", "Seconds", "s"), 70, false);
		AddStat(OutTooltip, Stat_Melee_StaminaCost, Stats.MeleeStaminaCost, Compared.MeleeStaminaCost, bHasComparison, FText(), 80, false);
	}
	OutTooltip.StatLines.Sort([](const FOBTooltipStatLine& A, const FOBTooltipStatLine& B)
	{
		return A.SortOrder < B.SortOrder;
	});
	return true;
}

FText UOBItemTooltipLibrary::BuildFallbackTooltipText(const FInventoryData& ItemInstance)
{
	FOBItemTooltipViewModel Tooltip;
	if (!BuildItemTooltip(ItemInstance, nullptr, Tooltip))
	{
		return FText::FromName(ItemInstance.ItemTag.GetTagName());
	}

	FString Result = Tooltip.DisplayName.ToString();
	if (!Tooltip.Description.IsEmpty())
	{
		Result += TEXT("\n") + Tooltip.Description.ToString();
	}
	if (Tooltip.Category == EOBItemCategory::Weapon ||
		Tooltip.Category == EOBItemCategory::Equipment ||
		Tooltip.Category == EOBItemCategory::Attachment)
	{
		Result += FString::Printf(TEXT("\nCondition: %.0f%%"), Tooltip.Durability * 100.f);
	}
	if (Tooltip.bFoundInRaid)
	{
		Result += TEXT("\nFound in raid");
	}
	for (const FOBTooltipStatLine& Line : Tooltip.StatLines)
	{
		Result += FString::Printf(
			TEXT("\n%s: %.*f%s"),
			*Line.Label.ToString(),
			Line.DecimalPlaces,
			Line.Value,
			*Line.UnitText.ToString());
	}
	if (!Tooltip.AttachmentNames.IsEmpty())
	{
		Result += TEXT("\nAttachments: ");
		for (int32 Index = 0; Index < Tooltip.AttachmentNames.Num(); ++Index)
		{
			if (Index > 0) Result += TEXT(", ");
			Result += Tooltip.AttachmentNames[Index].ToString();
		}
	}
	return FText::FromString(Result);
}
