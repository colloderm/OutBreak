// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Player/Traits/Data/OBTraitCoreTypes.h"
#include "OBTraitTreeData.generated.h"

class UGameplayAbility;
class UOBTraitConditionDefinition;
class UOBTraitEffectDefinition;
class UOBTraitTreeLayoutData;
class UTexture2D;

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitNodeDefinition : public UObject
{
	GENERATED_BODY()

public:
	FGameplayTag GetNodeId() const { return NodeId; }
	FGameplayTag GetBranchId() const { return BranchId; }
	const FGameplayTagContainer& GetSpecialtyScope() const { return SpecialtyScope; }
	const FGameplayTagContainer& GetExclusiveGroups() const { return ExclusiveGroups; }
	const TArray<FOBTraitPrerequisite>& GetPrerequisites() const { return Prerequisites; }
	const TArray<FOBTraitRankDefinition>& GetRanks() const { return Ranks; }
	const TArray<TObjectPtr<UOBTraitConditionDefinition>>& GetUnlockConditions() const { return UnlockConditions; }
	const TArray<TObjectPtr<UOBTraitEffectDefinition>>& GetEffects() const { return Effects; }
	const FText& GetDisplayName() const { return DisplayName; }
	const FText& GetDescription() const { return Description; }
	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }
	int32 GetTier() const { return Tier; }
	bool IsInitiallyUnlocked() const { return bInitiallyUnlocked; }
	bool IsCapstone() const { return bCapstone; }
	int32 GetMaxRank() const { return Ranks.Num(); }
	int32 GetPointCostForRank(int32 Rank) const;
	int32 GetCumulativePointCost(int32 Rank) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Identity", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Identity", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag BranchId;

	// Empty means every specialty linked to BranchId may use this node.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Identity", Meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer SpecialtyScope;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Progression", Meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	int32 Tier = 0;

	// Metadata only in this isolated phase. Whether this seeds rank 1 or only bypasses a gate
	// is a progression-policy decision for the future ownership integration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Progression", Meta = (AllowPrivateAccess = "true"))
	bool bInitiallyUnlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Progression", Meta = (AllowPrivateAccess = "true"))
	bool bCapstone = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Display", Meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Display", Meta = (MultiLine = true, AllowPrivateAccess = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Display", Meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Progression", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitRankDefinition> Ranks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Conditions", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitPrerequisite> Prerequisites;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait|Conditions", Meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOBTraitConditionDefinition>> UnlockConditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Conditions", Meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ExclusiveGroups;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait|Effects", Meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOBTraitEffectDefinition>> Effects;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, Const)
class OUTBREAK_API UOBTraitSignatureAbilityDefinition : public UObject
{
	GENERATED_BODY()

public:
	FGameplayTag GetSignatureId() const { return SignatureId; }
	TSoftClassPtr<UGameplayAbility> GetAbilityClass() const { return AbilityClass; }
	FGameplayTag GetInputTag() const { return InputTag; }
	FGameplayTag GetSlotTag() const { return SlotTag; }
	int32 GetAbilityLevel() const { return AbilityLevel; }
	const FText& GetDisplayName() const { return DisplayName; }
	const FText& GetDescription() const { return Description; }
	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }
	const TArray<TObjectPtr<UOBTraitConditionDefinition>>& GetUnlockConditions() const { return UnlockConditions; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag SignatureId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (AllowPrivateAccess = "true"))
	TSoftClassPtr<UGameplayAbility> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (AllowPrivateAccess = "true"))
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 AbilityLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (MultiLine = true, AllowPrivateAccess = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Signature", Meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait|Signature", Meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOBTraitConditionDefinition>> UnlockConditions;
};

UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBTraitTreeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	int32 GetDefinitionVersion() const { return DefinitionVersion; }
	const TArray<FOBTraitSpecialtyDefinition>& GetSpecialties() const { return Specialties; }
	const TArray<FOBTraitBranchDefinition>& GetBranches() const { return Branches; }
	const TArray<TObjectPtr<UOBTraitNodeDefinition>>& GetNodes() const { return Nodes; }
	const TArray<TObjectPtr<UOBTraitSignatureAbilityDefinition>>& GetSignatureAbilities() const { return SignatureAbilities; }
	TSoftObjectPtr<UOBTraitTreeLayoutData> GetDefaultLayout() const { return DefaultLayout; }

	const UOBTraitNodeDefinition* FindNode(FGameplayTag NodeId) const;
	const UOBTraitNodeDefinition* FindNodeByName(FName NodeId) const;
	const FOBTraitBranchDefinition* FindBranch(FGameplayTag BranchId) const;
	const FOBTraitBranchDefinition* FindBranchByName(FName BranchId) const;
	const FOBTraitSpecialtyDefinition* FindSpecialty(FGameplayTag SpecialtyId) const;
	const FOBTraitSpecialtyDefinition* FindSpecialtyByName(FName SpecialtyId) const;
	const UOBTraitSignatureAbilityDefinition* FindSignatureAbility(FGameplayTag SignatureId) const;

	FName ResolveLegacyId(FName CandidateId, bool* bOutRedirectCycle = nullptr) const;
	FOBTraitDefinitionValidationReport ValidateDefinition() const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 DefinitionVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitSpecialtyDefinition> Specialties;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitBranchDefinition> Branches;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOBTraitNodeDefinition>> Nodes;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trait", Meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UOBTraitSignatureAbilityDefinition>> SignatureAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Migration", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitLegacyIdRedirect> LegacyIdRedirects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Display", Meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UOBTraitTreeLayoutData> DefaultLayout;
};
