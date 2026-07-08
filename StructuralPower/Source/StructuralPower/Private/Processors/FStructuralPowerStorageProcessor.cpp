// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerStorageProcessor.h"

#include "Buildables/FGBuildablePowerStorage.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Connection/FStructuralStorageConnectionPoint.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Network/UStructuralPowerStorageListener.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

namespace
{
void EnsureStorageListener(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Storage))
	{
		return;
	}

	TInlineComponentArray<UStructuralPowerStorageListener*> Listeners;
	Storage->GetComponents(Listeners);
	if (Listeners.Num() > 0)
	{
		return;
	}

	UStructuralPowerStorageListener* Listener =
		NewObject<UStructuralPowerStorageListener>(Storage, NAME_None, RF_Transient);
	if (!Listener)
	{
		return;
	}

	Storage->AddInstanceComponent(Listener);
	Listener->RegisterComponent();
	Listener->BindSubsystem(&Graph, Storage);
}
} // namespace

void FStructuralPowerStorageProcessor::TearDown(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Storage))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* Bus = Ctx.Graph().FindBusConnector(Storage);
	if (!IsValid(Bus))
	{
		return;
	}

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Bus->GetHiddenConnections(HiddenLinks);
	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		Bus->RemoveHiddenConnection(OtherRaw);
		if (IsValid(OtherRaw))
		{
			OtherRaw->RemoveHiddenConnection(Bus);
		}
	}
}

void FStructuralPowerStorageProcessor::Process(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Storage))
	{
		return;
	}

	AStructuralPowerGraphSubsystem& Graph = Ctx.Graph();
	const bool bBulk = Graph.IsBulkLoadDrainActive();
	FStructuralStorageConnectionPoint Connection(Graph, Storage);
	const FStructuralNodeId StorageId = Graph.MakeNodeId(Storage);
	if (!bBulk)
	{
		if (const FTrackedEndpoint* Existing = Graph.TrackedEndpoints.Find(StorageId))
		{
			if (Existing->Kind == EStructuralEndpointKind::Storage && Existing->ParentId.IsValid())
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
		FStructuralPowerTrace::LogPlacementSkip(Storage, TEXT("storage_bus_create_failed"));
		return;
	}

	FStructuralSiteContext Site;
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(StorageId);
	if (!FStructuralSiteMembership::ResolveSiteContext(Graph, Storage, Site))
	{
		const FStructuralWallAnchor ParentAnchor = Connection.GetStructureAnchor();
		Graph.ResolveEndpointComponentRoot(Storage, ParentAnchor, Site.ParentId);
		Tracked.ParentId = Site.ParentId;
		Site.SiteRoot = Graph.FindRootForTrackedEndpoint(Tracked);
		Site.bAnchored = Site.SiteRoot != INDEX_NONE;
	}

	FStructuralSiteMembershipParams Params;
	Params.bLinkVisibleConnections = !bBulk;
	if (bBulk)
	{
		Graph.LinkBusToVisibleConnections(Storage, OutletBus);
	}
	else
	{
		Params.bLinkVisibleConnections = true;
	}
	Params.bBridgePeersOnly = bBulk;

	if (Site.SiteRoot != INDEX_NONE && (bBulk || !Graph.HasBridgeBusPeerMesh(OutletBus)))
	{
		FStructuralSiteMembership::IntegrateOnPlace(
			Graph,
			Storage,
			OutletBus,
			StorageId,
			EStructuralEndpointKind::Storage,
			Tracked,
			Site,
			Params);
	}
	else
	{
		Tracked.Actor = Storage;
		Tracked.ParentId = Site.ParentId;
		Tracked.Kind = EStructuralEndpointKind::Storage;
		Tracked.bStructuralPowerTransferActive = Site.bAnchored;
		Graph.RegisterBuildableActor(Storage);
		if (Params.bLinkVisibleConnections)
		{
			Graph.LinkBusToVisibleConnectionsLocal(Storage, OutletBus);
		}
	}

	if (bBulk && Site.SiteRoot != INDEX_NONE && Site.ParentId.IsValid())
	{
		Graph.AddEndpointToRootIndex(Site.SiteRoot, EStructuralEndpointKind::Storage, StorageId);
	}
	else if (!bBulk)
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	EnsureStorageListener(Graph, Storage);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] storage %s root=%d parentValid=%d busCircuit=%d powered=%d"),
		*Storage->GetName(),
		Site.SiteRoot,
		Site.bAnchored ? 1 : 0,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);
}
