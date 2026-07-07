// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralPoleConnectionPoint.h"

#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Core/EAttachContext.h"
#include "FGPowerConnectionComponent.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

FStructuralPoleConnectionPoint::FStructuralPoleConnectionPoint(
	AStructuralPowerGraphSubsystem& InGraph,
	AFGBuildablePowerPole* InPole)
	: Graph(InGraph)
	, Pole(InPole)
{
}

UFGPowerConnectionComponent* FStructuralPoleConnectionPoint::GetStructuralConnector()
{
	AFGBuildablePowerPole* PolePtr = Pole.Get();
	if (!IsValid(PolePtr))
	{
		return nullptr;
	}

	return Graph.GetOrCreateBusConnector(PolePtr);
}

FStructuralWallAnchor FStructuralPoleConnectionPoint::GetStructureAnchor() const
{
	AFGBuildablePowerPole* PolePtr = Pole.Get();
	if (!IsValid(PolePtr))
	{
		return {};
	}

	return Graph.ResolveOutletAnchor(PolePtr);
}

void FStructuralPoleConnectionPoint::OnWireOrGateChanged()
{
	AFGBuildablePowerPole* PolePtr = Pole.Get();
	if (!IsValid(PolePtr))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(GetStructuralConnector());
	if (!OutletBus)
	{
		return;
	}

	const FStructuralNodeId PoleId = Graph.MakeNodeId(PolePtr);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(PoleId);
	Tracked.Actor = PolePtr;
	Tracked.Kind = EStructuralEndpointKind::Pole;
	Graph.RegisterBuildableActor(PolePtr);

	if (!Tracked.ParentId.IsValid())
	{
		const FStructuralWallAnchor ParentAnchor = GetStructureAnchor();
		FStructuralNodeId ParentId;
		Graph.ResolvePoleComponentRoot(PolePtr, ParentAnchor, ParentId);
		Tracked.ParentId = ParentId;
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	const int32 Root = Graph.FindRootForTrackedEndpoint(Tracked);

	Graph.LinkBusToVisibleConnectionsLocal(PolePtr, OutletBus);

	if (Root != INDEX_NONE && !Graph.HasBridgeBusPeerMesh(OutletBus))
	{
		Graph.TryMeshPeerBusOnComponent(
			PolePtr,
			OutletBus,
			Root,
			PoleId,
			/*bBridgePeersOnly=*/true);
	}

	Graph.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] pole wire delta %s root=%d busCircuit=%d powered=%d"),
		*PolePtr->GetName(),
		Root,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);

	if (Root != INDEX_NONE)
	{
		Graph.RestitchKeyedSubnetsAfterMeshFeedRefresh(Root, EAttachContext::WireDelta);
	}
}
