// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Traits/Data/OBTraitCoreTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "OBTraitSaveTypes.generated.h"

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSaveNode
{
	GENERATED_BODY()

	// FName preserves an orphaned or redirected ID even when the Gameplay Tag is no longer registered.
	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	FName NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	TArray<int32> PaidCostsByRank;

	// Compatibility field for an older aggregate-only schema.
	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	int32 LegacyInvestedPoints = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSaveData
{
	GENERATED_BODY()

	static constexpr int32 LatestSchemaVersion = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	int32 SchemaVersion = LatestSchemaVersion;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	FPrimaryAssetId TraitTreeId;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	int32 TraitDefinitionVersion = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	FName SelectedSpecialtyId;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	int32 LifetimeEarnedPoints = 0;

	// Audit value only. The normalized runtime value is recomputed from valid node records.
	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	int32 SavedTotalSpentPoints = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Trait|Save")
	TArray<FOBTraitSaveNode> Nodes;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSaveMigrationIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Save")
	EOBTraitDefinitionIssueSeverity Severity = EOBTraitDefinitionIssueSeverity::Warning;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Save")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Save")
	FName ResolvedId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Save")
	FText Message;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSaveMigrationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Save")
	TArray<FOBTraitSaveMigrationIssue> Issues;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Save")
	int32 RefundedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait|Save")
	bool bRequiresExternalConditionRevalidation = false;

	void AddIssue(
		EOBTraitDefinitionIssueSeverity Severity,
		FName SourceId,
		FName ResolvedId,
		const FText& Message)
	{
		FOBTraitSaveMigrationIssue& Issue = Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.SourceId = SourceId;
		Issue.ResolvedId = ResolvedId;
		Issue.Message = Message;
	}
};
