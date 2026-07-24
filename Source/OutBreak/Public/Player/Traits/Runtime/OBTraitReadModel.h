// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Traits/Conditions/OBTraitCondition.h"
#include "OBTraitReadModel.generated.h"

class UOBTraitTreeData;
class UOBTraitTreeLayoutData;
class UTexture2D;
struct FOBTraitExternalEvaluationSnapshot;
struct FOBTraitPlayerState;

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSpecialtyReadModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag SpecialtyId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTagContainer LinkedBranchIds;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTagContainer SignatureAbilityIds;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	bool bSelected = false;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitBranchReadModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag BranchId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 SortOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 InvestedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	bool bInitiallyVisible = true;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitNodeReadModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag BranchId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTagContainer SpecialtyScope;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 Tier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 CurrentRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 MaxRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 InvestedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 NextRankCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText NextRankEffectDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FText> RankEffectDescriptions;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitPrerequisite> Prerequisites;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTagContainer ExclusiveGroups;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitConditionResult Investability;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FVector2D LayoutPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 LayoutLayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 LayoutSortOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	bool bInitiallyUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	bool bCapstone = false;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSignatureReadModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag SignatureId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag InputTag;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag SlotTag;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitConditionResult UnlockState;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitConnectionReadModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag FromNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag ToNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 RequiredRank = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FVector2D> ControlPoints;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	bool bVisible = true;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitTreeReadModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 DefinitionVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 LayoutVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 StateRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 AvailablePoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 LifetimeEarnedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 TotalInvestedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag SelectedSpecialtyId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitSpecialtyReadModel> Specialties;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitBranchReadModel> Branches;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitNodeReadModel> Nodes;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitConnectionReadModel> Connections;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitSignatureReadModel> SignatureAbilities;
};

/** Builds normalized display data without registering a ViewModel or touching a widget. */
class OUTBREAK_API FOBTraitReadModelBuilder
{
public:
	static FOBTraitTreeReadModel Build(
		const UOBTraitTreeData& Tree,
		const UOBTraitTreeLayoutData* Layout,
		const FOBTraitPlayerState& State,
		const FOBTraitExternalEvaluationSnapshot& ExternalSnapshot);
};
