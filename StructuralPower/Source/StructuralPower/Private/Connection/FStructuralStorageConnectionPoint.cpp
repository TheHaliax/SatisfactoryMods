// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralStorageConnectionPoint.h"

#include "Buildables/FGBuildablePowerStorage.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

FStructuralStorageConnectionPoint::FStructuralStorageConnectionPoint(
	AStructuralPowerGraphSubsystem& InGraph,
	AFGBuildablePowerStorage* InStorage)
	: Graph(InGraph)
	, Storage(InStorage)
{
}

UFGPowerConnectionComponent* FStructuralStorageConnectionPoint::GetStructuralConnector()
{
	AFGBuildablePowerStorage* StoragePtr = Storage.Get();
	if (!IsValid(StoragePtr))
	{
		return nullptr;
	}

	return Graph.GetOrCreateBusConnector(StoragePtr);
}

FStructuralWallAnchor FStructuralStorageConnectionPoint::GetStructureAnchor() const
{
	AFGBuildablePowerStorage* StoragePtr = Storage.Get();
	if (!IsValid(StoragePtr))
	{
		return {};
	}

	return Graph.ResolveOutletAnchor(StoragePtr);
}

void FStructuralStorageConnectionPoint::OnWireOrGateChanged(EAttachContext /*AttachContext*/)
{
	AFGBuildablePowerStorage* StoragePtr = Storage.Get();
	if (!IsValid(StoragePtr))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(GetStructuralConnector());
	if (!OutletBus)
	{
		return;
	}

	const FStructuralNodeId StorageId = Graph.MakeNodeId(StoragePtr);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(StorageId);

	FStructuralSiteContext Site;
	if (!FStructuralSiteMembership::ResolveSiteContext(Graph, StoragePtr, Site))
	{
		if (Tracked.ParentId.IsValid())
		{
			Site.ParentId = Tracked.ParentId;
			Site.SiteRoot = Graph.FindRootForTrackedEndpoint(Tracked);
			Site.bAnchored = Site.SiteRoot != INDEX_NONE;
		}
	}

	FStructuralSiteMembershipParams Params;
	Params.bLinkVisibleConnections = true;
	Params.bBridgePeersOnly = true;
	FStructuralSiteMembership::IntegrateOnPlace(
		Graph,
		StoragePtr,
		OutletBus,
		StorageId,
		EStructuralEndpointKind::Storage,
		Tracked,
		Site,
		Params);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] storage wire delta %s root=%d busCircuit=%d powered=%d"),
		*StoragePtr->GetName(),
		Site.SiteRoot,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);
}
