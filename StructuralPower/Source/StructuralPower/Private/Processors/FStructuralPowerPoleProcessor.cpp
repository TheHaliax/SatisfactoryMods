// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerPoleProcessor.h"

#include "Buildables/FGBuildablePowerPole.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Connection/FStructuralPoleConnectionPoint.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

void FStructuralPowerPoleProcessor::Process(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole))
	{
		return;
	}

	AStructuralPowerGraphSubsystem& Graph = Ctx.Graph();
	const bool bBulk = Graph.IsBulkLoadDrainActive();
	FStructuralPoleConnectionPoint Connection(Graph, Pole);
	const FStructuralNodeId PoleId = Graph.MakeNodeId(Pole);
	if (!bBulk)
	{
		if (const FTrackedEndpoint* Existing = Graph.TrackedEndpoints.Find(PoleId))
		{
			if (Existing->Kind == EStructuralEndpointKind::Pole && Existing->ParentId.IsValid())
			{
				Connection.OnWireOrGateChanged(Ctx.GetAttachContext());
				return;
			}
		}
	}

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(Connection.GetStructuralConnector());
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Pole, TEXT("outlet_bus_create_failed"));
		return;
	}

	const FStructuralWallAnchor ParentAnchor = Connection.GetStructureAnchor();
	FStructuralNodeId ParentId;
	const int32 Root = Graph.ResolvePoleComponentRoot(Pole, ParentAnchor, ParentId);

	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(PoleId);
	Tracked.Actor = Pole;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Pole;
	Tracked.bStructuralPowerTransferActive = true;
	Graph.RegisterBuildableActor(Pole);
	if (bBulk)
	{
		if (Root != INDEX_NONE && ParentId.IsValid())
		{
			Graph.AddEndpointToRootIndex(Root, EStructuralEndpointKind::Pole, PoleId);
		}
	}
	else
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	Graph.LinkBusToVisibleConnections(Pole, OutletBus);

	if (Root != INDEX_NONE && (bBulk || !Graph.HasBridgeBusPeerMesh(OutletBus)))
	{
		Graph.TryMeshPeerBusOnComponent(Pole, OutletBus, Root, PoleId, bBulk);
	}

	Graph.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);

	if (bBulk)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s id=%s"),
			*Pole->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s id=%s"),
			*Pole->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
}
