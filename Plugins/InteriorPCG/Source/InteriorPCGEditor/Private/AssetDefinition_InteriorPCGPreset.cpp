#include "AssetDefinition_InteriorPCGPreset.h"

#include "InteriorPCGPreset.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_InteriorPCGPreset"

FText UAssetDefinition_InteriorPCGPreset::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "PCG Preset");
}

FLinearColor UAssetDefinition_InteriorPCGPreset::GetAssetColor() const
{
	return FLinearColor(0.15f, 0.75f, 0.55f);
}

TSoftClassPtr<UObject> UAssetDefinition_InteriorPCGPreset::GetAssetClass() const
{
	return UInteriorPCGPreset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_InteriorPCGPreset::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::Misc };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
