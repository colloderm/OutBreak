// Copyright OutBreak. All Rights Reserved.

#include "InteriorPCGDataAssets.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InteriorPCGDataAssets)

const FInteriorPCGBuildingModuleDefinition* UInteriorPCGBuildingModuleSet::FindModule(const EInteriorPCGModuleType ModuleType,
	const EInteriorPCGFloorBand FloorBand) const
{
	if (FloorBand != EInteriorPCGFloorBand::Any)
	{
		if (const FInteriorPCGBuildingModuleDefinition* Exact = Modules.FindByPredicate([ModuleType, FloorBand](const FInteriorPCGBuildingModuleDefinition& Definition)
		{
			return Definition.ModuleType == ModuleType && Definition.FloorBands.Contains(FloorBand);
		}))
		{
			return Exact;
		}
	}

	return Modules.FindByPredicate([ModuleType](const FInteriorPCGBuildingModuleDefinition& Definition)
	{
		return Definition.ModuleType == ModuleType &&
			(Definition.FloorBands.IsEmpty() || Definition.FloorBands.Contains(EInteriorPCGFloorBand::Any));
	});
}

const FInteriorPCGPropDefinition* UInteriorPCGPropSet::FindProp(const EInteriorPCGPropType PropType) const
{
	return Props.FindByPredicate([PropType](const FInteriorPCGPropDefinition& Definition)
	{
		return Definition.PropType == PropType;
	});
}
