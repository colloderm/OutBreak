// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Data/OBTraitTreeLayoutData.h"

FPrimaryAssetId UOBTraitTreeLayoutData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("OBTraitTreeLayout")), GetFName());
}

const FOBTraitNodeLayout* UOBTraitTreeLayoutData::FindNodeLayout(FGameplayTag NodeId) const
{
	return NodeLayouts.FindByPredicate([NodeId](const FOBTraitNodeLayout& Layout)
	{
		return Layout.NodeId == NodeId;
	});
}

const FOBTraitConnectionLayout* UOBTraitTreeLayoutData::FindConnectionLayout(
	FGameplayTag FromNodeId,
	FGameplayTag ToNodeId) const
{
	return ConnectionLayouts.FindByPredicate([FromNodeId, ToNodeId](const FOBTraitConnectionLayout& Layout)
	{
		return Layout.FromNodeId == FromNodeId && Layout.ToNodeId == ToNodeId;
	});
}
