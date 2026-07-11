// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralBridgeAttach.h"

#include "Buildables/FGBuildable.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

bool FStructuralBridgeAttach::HasPlacementMembership(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildable* Host,
	EStructuralEndpointKind ExpectedKind)
{
	if (!IsValid(Host))
	{
		return false;
	}

	const FStructuralNodeId EndpointId = Graph.MakeNodeId(Host);
	const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(EndpointId);
	return Tracked
		&& Tracked->Kind == ExpectedKind
		&& Tracked->ParentId.IsValid();
}

FStructuralBridgeAttachOutcome FStructuralBridgeAttach::AttachOnPlace(
	FStructuralPowerContext& Ctx,
	const FStructuralBridgeAttachRequest& Request)
{
	FStructuralBridgeAttachOutcome Outcome;
	AFGBuildable* Host = Request.Host;
	if (!IsValid(Host))
	{
		return Outcome;
	}

	AStructuralPowerGraphSubsystem& Graph = Ctx.Graph();
	const bool bBulk = Graph.IsBulkLoadDrainActive();
	const FStructuralNodeId EndpointId = Graph.MakeNodeId(Host);

	UFGStructuralPowerConnectionComponent* OutletBus = Graph.GetOrCreateBusConnector(Host);
	Outcome.OutletBus = OutletBus;
	if (!OutletBus)
	{
		if (!bBulk)
		{
			FStructuralPowerTrace::LogPlacementSkip(
				Host,
				TEXT("bridge_bus_create_failed"),
				ELogVerbosity::Warning);
		}
		return Outcome;
	}

	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(EndpointId);
	FStructuralSiteContext& Site = Outcome.Site;

	if (!FStructuralSiteMembership::ResolveSiteContext(
			Graph,
			Host,
			Site,
			Request.bUsePoleRootResolver))
	{
		if (Request.bUsePoleRootResolver)
		{
			Site.bAnchored = false;
		}
		else
		{
			const FStructuralWallAnchor ParentAnchor = Graph.ResolveOutletAnchor(Host);
			Graph.ResolveEndpointComponentRoot(Host, ParentAnchor, Site.ParentId);
			Tracked.ParentId = Site.ParentId;
			Site.SiteRoot = Graph.FindRootForTrackedEndpoint(Tracked);
			Site.bAnchored = Site.SiteRoot != INDEX_NONE;
		}
	}

	const bool bSiteReady = Site.bAnchored && Site.SiteRoot != INDEX_NONE;
	if (!bSiteReady)
	{
		Outcome.bSiteNotReady = !bBulk;
		Tracked.Actor = Host;
		Tracked.Kind = Request.Kind;
		Tracked.ParentId = Site.ParentId;
		Graph.RegisterBuildableActor(Host);
		Graph.LinkBusToVisibleConnectionsLocal(Host, OutletBus);

		if (!bBulk && Request.bUsePoleRootResolver)
		{
			Graph.EnqueuePlacement(Host, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		}

		return Outcome;
	}

	FStructuralSiteMembershipParams Params;
	Params.bLinkVisibleConnections = true;
	Params.bBridgePeersOnly = true;
	Params.bMeshOnlyLinks = false;
	Params.bSkipEndpointIndexDirty = !bBulk && Site.SiteRoot != INDEX_NONE;

	if (bBulk)
	{
		Graph.LinkBusToVisibleConnections(Host, OutletBus);
	}

	if (bBulk || !Graph.HasBridgeBusPeerMesh(OutletBus))
	{
		FStructuralSiteMembership::IntegrateOnPlace(
			Graph,
			Host,
			OutletBus,
			EndpointId,
			Request.Kind,
			Tracked,
			Site,
			Params);
	}
	else
	{
		Tracked.Actor = Host;
		Tracked.ParentId = Site.ParentId;
		Tracked.Kind = Request.Kind;
		Tracked.bStructuralPowerTransferActive = Site.bAnchored;
		Graph.RegisterBuildableActor(Host);
		Graph.LinkBusToVisibleConnectionsLocal(Host, OutletBus);
	}

	Outcome.bAttached = true;
	return Outcome;
}
