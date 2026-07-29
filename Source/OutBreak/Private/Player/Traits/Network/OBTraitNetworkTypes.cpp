// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Traits/Network/OBTraitNetworkTypes.h"

#include "Player/Traits/Runtime/OBTraitRuntimeState.h"

FOBTraitStateSnapshot FOBTraitStateSnapshot::FromRuntimeState(const FOBTraitPlayerState& State)
{
	FOBTraitStateSnapshot Snapshot;
	Snapshot.StateRevision = State.StateRevision;
	Snapshot.SelectedSpecialtyId = State.SelectedSpecialtyId;
	Snapshot.LifetimeEarnedPoints = State.PointLedger.LifetimeEarnedPoints;
	Snapshot.AvailablePoints = State.PointLedger.AvailablePoints;
	Snapshot.TotalInvestedPoints = State.GetTotalInvestedPoints();

	for (const FOBTraitNodeState& NodeState : State.NodeStates)
	{
		if (NodeState.GetRank() <= 0)
		{
			continue;
		}

		FOBTraitNodeStateSnapshot& NodeSnapshot = Snapshot.Nodes.AddDefaulted_GetRef();
		NodeSnapshot.NodeId = NodeState.NodeId;
		NodeSnapshot.Rank = NodeState.GetRank();
		NodeSnapshot.InvestedPoints = NodeState.GetInvestedPoints();
	}

	Snapshot.Nodes.Sort([](const FOBTraitNodeStateSnapshot& Left, const FOBTraitNodeStateSnapshot& Right)
	{
		return Left.NodeId.ToString() < Right.NodeId.ToString();
	});
	return Snapshot;
}
