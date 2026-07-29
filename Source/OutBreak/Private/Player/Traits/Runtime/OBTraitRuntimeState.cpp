// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Runtime/OBTraitRuntimeState.h"

#include "Player/Traits/Data/OBTraitTreeData.h"

void FOBTraitPointLedger::InitializeBalances(int32 InLifetimeEarnedPoints, int32 InTotalSpentPoints)
{
	LifetimeEarnedPoints = FMath::Max(0, InLifetimeEarnedPoints);
	TotalSpentPoints = FMath::Clamp(InTotalSpentPoints, 0, LifetimeEarnedPoints);
	AvailablePoints = LifetimeEarnedPoints - TotalSpentPoints;
	Entries.Reset();
}

bool FOBTraitPointLedger::ContainsTransaction(const FGuid& TransactionId) const
{
	return TransactionId.IsValid() && Entries.ContainsByPredicate([&TransactionId](const FOBTraitPointLedgerEntry& Entry)
	{
		return Entry.TransactionId == TransactionId;
	});
}

bool FOBTraitPointLedger::EarnPoints(int32 Amount, const FGuid& TransactionId)
{
	if (Amount <= 0 || !TransactionId.IsValid() || ContainsTransaction(TransactionId))
	{
		return false;
	}

	LifetimeEarnedPoints += Amount;
	AvailablePoints += Amount;
	AddEntry(EOBTraitPointTransactionType::Earn, Amount, FGameplayTag(), TransactionId);
	return true;
}

bool FOBTraitPointLedger::SpendPoints(int32 Amount, FGameplayTag NodeId, const FGuid& TransactionId)
{
	if (Amount < 0 || AvailablePoints < Amount || !TransactionId.IsValid() || ContainsTransaction(TransactionId))
	{
		return false;
	}

	AvailablePoints -= Amount;
	TotalSpentPoints += Amount;
	AddEntry(EOBTraitPointTransactionType::Spend, Amount, NodeId, TransactionId);
	return true;
}

bool FOBTraitPointLedger::RefundPoints(int32 Amount, FGameplayTag NodeId, const FGuid& TransactionId)
{
	if (Amount < 0 || Amount > TotalSpentPoints || !TransactionId.IsValid() || ContainsTransaction(TransactionId))
	{
		return false;
	}

	TotalSpentPoints -= Amount;
	AvailablePoints = FMath::Min(LifetimeEarnedPoints, AvailablePoints + Amount);
	AddEntry(EOBTraitPointTransactionType::Refund, Amount, NodeId, TransactionId);
	return true;
}

void FOBTraitPointLedger::AddEntry(
	EOBTraitPointTransactionType TransactionType,
	int32 Amount,
	FGameplayTag NodeId,
	const FGuid& TransactionId)
{
	FOBTraitPointLedgerEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.TransactionId = TransactionId;
	Entry.TransactionType = TransactionType;
	Entry.NodeId = NodeId;
	Entry.Amount = Amount;
	Entry.BalanceAfter = AvailablePoints;
}

int32 FOBTraitNodeState::GetInvestedPoints() const
{
	int32 Total = 0;
	for (const int32 PaidCost : PaidCostsByRank)
	{
		Total += FMath::Max(0, PaidCost);
	}
	return Total;
}

void FOBTraitNodeState::AddRank(int32 PaidCost)
{
	PaidCostsByRank.Add(FMath::Max(0, PaidCost));
}

const FOBTraitNodeState* FOBTraitPlayerState::FindNodeState(FGameplayTag NodeId) const
{
	return NodeStates.FindByPredicate([NodeId](const FOBTraitNodeState& State)
	{
		return State.NodeId == NodeId;
	});
}

FOBTraitNodeState* FOBTraitPlayerState::FindMutableNodeState(FGameplayTag NodeId)
{
	return NodeStates.FindByPredicate([NodeId](const FOBTraitNodeState& State)
	{
		return State.NodeId == NodeId;
	});
}

int32 FOBTraitPlayerState::GetRank(FGameplayTag NodeId) const
{
	const FOBTraitNodeState* State = FindNodeState(NodeId);
	return State ? State->GetRank() : 0;
}

int32 FOBTraitPlayerState::GetNodeInvestedPoints(FGameplayTag NodeId) const
{
	const FOBTraitNodeState* State = FindNodeState(NodeId);
	return State ? State->GetInvestedPoints() : 0;
}

int32 FOBTraitPlayerState::GetTotalInvestedPoints() const
{
	int32 Total = 0;
	for (const FOBTraitNodeState& State : NodeStates)
	{
		Total += State.GetInvestedPoints();
	}
	return Total;
}

int32 FOBTraitPlayerState::GetBranchInvestedPoints(
	const UOBTraitTreeData& Tree,
	FGameplayTag BranchId) const
{
	int32 Total = 0;
	for (const FOBTraitNodeState& State : NodeStates)
	{
		const UOBTraitNodeDefinition* Node = Tree.FindNode(State.NodeId);
		if (Node && Node->GetBranchId() == BranchId)
		{
			Total += State.GetInvestedPoints();
		}
	}
	return Total;
}

bool FOBTraitPlayerState::AddInvestment(
	FGameplayTag NodeId,
	int32 PointCost,
	const FGuid& TransactionId)
{
	if (!NodeId.IsValid() || PointCost < 0 || !PointLedger.SpendPoints(PointCost, NodeId, TransactionId))
	{
		return false;
	}

	FOBTraitNodeState* State = FindMutableNodeState(NodeId);
	if (!State)
	{
		FOBTraitNodeState& NewState = NodeStates.AddDefaulted_GetRef();
		NewState.NodeId = NodeId;
		State = &NewState;
	}

	State->AddRank(PointCost);
	++StateRevision;
	return true;
}

int32 FOBTraitPlayerState::ResetAllInvestments(const FGuid& TransactionId)
{
	const int32 Refund = GetTotalInvestedPoints();
	if (Refund <= 0)
	{
		NodeStates.Reset();
		return 0;
	}

	if (!PointLedger.RefundPoints(Refund, FGameplayTag(), TransactionId))
	{
		return 0;
	}

	NodeStates.Reset();
	++StateRevision;
	return Refund;
}

bool FOBTraitPlayerState::HasProcessedRequest(const FGuid& RequestId) const
{
	return RequestId.IsValid() && RecentProcessedRequestIds.Contains(RequestId);
}

void FOBTraitPlayerState::RecordProcessedRequest(const FGuid& RequestId, int32 MaxRememberedRequests)
{
	if (!RequestId.IsValid() || RecentProcessedRequestIds.Contains(RequestId))
	{
		return;
	}

	RecentProcessedRequestIds.Add(RequestId);
	const int32 SafeLimit = FMath::Max(1, MaxRememberedRequests);
	if (RecentProcessedRequestIds.Num() > SafeLimit)
	{
		RecentProcessedRequestIds.RemoveAt(0, RecentProcessedRequestIds.Num() - SafeLimit);
	}
}
