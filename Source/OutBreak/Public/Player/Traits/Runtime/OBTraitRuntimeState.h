// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Player/Traits/Data/OBTraitCoreTypes.h"
#include "OBTraitRuntimeState.generated.h"

class UOBTraitTreeData;

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitPointLedgerEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGuid TransactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	EOBTraitPointTransactionType TransactionType = EOBTraitPointTransactionType::Earn;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 BalanceAfter = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitPointLedger
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 LifetimeEarnedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 AvailablePoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 TotalSpentPoints = 0;

	// Audit history is runtime/profile data, not a replicated per-node property list.
	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitPointLedgerEntry> Entries;

	void InitializeBalances(int32 InLifetimeEarnedPoints, int32 InTotalSpentPoints);
	bool ContainsTransaction(const FGuid& TransactionId) const;
	bool EarnPoints(int32 Amount, const FGuid& TransactionId);
	bool SpendPoints(int32 Amount, FGameplayTag NodeId, const FGuid& TransactionId);
	bool RefundPoints(int32 Amount, FGameplayTag NodeId, const FGuid& TransactionId);

private:
	void AddEntry(
		EOBTraitPointTransactionType TransactionType,
		int32 Amount,
		FGameplayTag NodeId,
		const FGuid& TransactionId);
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitNodeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag NodeId;

	// Historical costs are kept per rank so respec and migration can refund the amount actually paid.
	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<int32> PaidCostsByRank;

	int32 GetRank() const { return PaidCostsByRank.Num(); }
	int32 GetInvestedPoints() const;
	void AddRank(int32 PaidCost);
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitExternalEvaluationSnapshot
{
	GENERATED_BODY()

	// External systems provide a normalized tag snapshot. The trait core does not query an ASC.
	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTagContainer OwnedGameplayTags;

	// Supplied by the future server-owned player progression adapter.
	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 PlayerLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	bool bChangesAllowed = true;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitPlayerState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FGameplayTag SelectedSpecialtyId;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	TArray<FOBTraitNodeState> NodeStates;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	FOBTraitPointLedger PointLedger;

	UPROPERTY(BlueprintReadOnly, Category = "Trait")
	int32 StateRevision = 0;

	// Short-lived idempotency cache. Persistence and per-connection ownership are integration concerns.
	UPROPERTY(Transient)
	TArray<FGuid> RecentProcessedRequestIds;

	const FOBTraitNodeState* FindNodeState(FGameplayTag NodeId) const;
	FOBTraitNodeState* FindMutableNodeState(FGameplayTag NodeId);
	int32 GetRank(FGameplayTag NodeId) const;
	int32 GetNodeInvestedPoints(FGameplayTag NodeId) const;
	int32 GetTotalInvestedPoints() const;
	int32 GetBranchInvestedPoints(const UOBTraitTreeData& Tree, FGameplayTag BranchId) const;
	bool AddInvestment(FGameplayTag NodeId, int32 PointCost, const FGuid& TransactionId);
	int32 ResetAllInvestments(const FGuid& TransactionId);
	bool HasProcessedRequest(const FGuid& RequestId) const;
	void RecordProcessedRequest(const FGuid& RequestId, int32 MaxRememberedRequests = 64);
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitInvestmentRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Trait")
	FGuid RequestId;

	UPROPERTY(BlueprintReadWrite, Category = "Trait")
	FGameplayTag NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "Trait", Meta = (ClampMin = "1"))
	int32 InvestmentCount = 1;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitResetRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Trait")
	FGuid RequestId;

	// Invalid means a full reset. Scoped reset is intentionally deferred to integration policy.
	UPROPERTY(BlueprintReadWrite, Category = "Trait")
	FGameplayTag ScopeId;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitSpecialtySelectionRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Trait")
	FGuid RequestId;

	UPROPERTY(BlueprintReadWrite, Category = "Trait")
	FGameplayTag SpecialtyId;
};
