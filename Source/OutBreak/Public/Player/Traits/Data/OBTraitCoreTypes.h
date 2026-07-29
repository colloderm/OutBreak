// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OBTraitCoreTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EOBTraitValidationFailure : uint8
{
	None,
	InvalidRequest,
	DefinitionsUnavailable,
	ChangesNotAllowed,
	NodeNotFound,
	InvalidInvestmentCount,
	MaxRankReached,
	InsufficientPoints,
	PrerequisiteNotMet,
	BranchPointsNotMet,
	SpecialtyMismatch,
	ExclusiveConflict,
	GameplayTagRequirementNotMet,
	RequiredLevelNotMet,
	ConditionFailed,
	DefinitionInvalid,
	DuplicateRequest
};

UENUM(BlueprintType)
enum class EOBTraitDefinitionIssueSeverity : uint8
{
	Warning,
	Error
};

UENUM(BlueprintType)
enum class EOBTraitPointTransactionType : uint8
{
	Earn,
	Spend,
	Refund,
	MigrationAdjustment
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitPrerequisite
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayTag RequiredNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "1"))
	int32 RequiredRank = 1;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitRankDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "0"))
	int32 PointCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FText EffectDescription;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSpecialtyDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayTag SpecialtyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayTagContainer LinkedBranchIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayTagContainer SignatureAbilityIds;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitBranchDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayTag BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	bool bInitiallyVisible = true;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitLegacyIdRedirect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FName OldId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FName NewId;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitDefinitionIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	EOBTraitDefinitionIssueSeverity Severity = EOBTraitDefinitionIssueSeverity::Error;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag PrimaryId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag SecondaryId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText Message;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitDefinitionValidationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitDefinitionIssue> Issues;

	bool HasErrors() const
	{
		return Issues.ContainsByPredicate([](const FOBTraitDefinitionIssue& Issue)
		{
			return Issue.Severity == EOBTraitDefinitionIssueSeverity::Error;
		});
	}

	void AddIssue(
		EOBTraitDefinitionIssueSeverity Severity,
		const FText& Message,
		FGameplayTag PrimaryId = FGameplayTag(),
		FGameplayTag SecondaryId = FGameplayTag())
	{
		FOBTraitDefinitionIssue& Issue = Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.Message = Message;
		Issue.PrimaryId = PrimaryId;
		Issue.SecondaryId = SecondaryId;
	}
};
