// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Traits/Save/OBTraitSaveTypes.h"

class UOBTraitTreeData;
struct FOBTraitPlayerState;

/** Structural Save DTO conversion only. No disk IO or USaveGame ownership is performed here. */
class OUTBREAK_API FOBTraitSaveNormalizer
{
public:
	static FOBTraitSaveData MakeSaveData(
		const UOBTraitTreeData& Tree,
		const FOBTraitPlayerState& State);

	static bool Normalize(
		const UOBTraitTreeData& Tree,
		const FOBTraitSaveData& SaveData,
		FOBTraitPlayerState& OutState,
		FOBTraitSaveMigrationReport& OutReport);
};
