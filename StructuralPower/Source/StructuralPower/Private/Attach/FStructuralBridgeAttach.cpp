// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralBridgeAttach.h"

#include "Buildables/FGBuildable.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

bool FStructuralBridgeAttach::HasPlacementMembership(
	FStructuralGraphSession& Session,
	AFGBuildable* Host,
	EStructuralEndpointKind ExpectedKind)
{
	if (!IsValid(Host))
	{
		return false;
	}

	const FStructuralNodeId EndpointId = Session.MakeNodeId(Host);
	const FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(EndpointId);
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

	FStructuralGraphSession& Session = Ctx.Session();
	const bool bBulk = Session.IsBulkLoadDrainActive();
	const FStructuralNodeId EndpointId = Session.MakeNodeId(Host);
	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(EndpointId);
	FStructuralSiteContext& Site = Outcome.Site;

	if (!FStructuralSiteMembership::ResolveSiteContext(Session,
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
			const FStructuralWallAnchor ParentAnchor = Session.ResolveOutletAnchor(Host);
			Session.ResolveEndpointComponentRoot(Host, ParentAnchor, Site.ParentId);
			Tracked.ParentId = Site.ParentId;
			Site.SiteRoot = Session.FindRootForTrackedEndpoint(Tracked);
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
		Tracked.bAwaitingStructuralSite = true;
		Session.RegisterBuildableActor(Host);

		if (!bBulk)
		{
			UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Host);
			Outcome.OutletBus = OutletBus;
			if (OutletBus)
			{
				Session.LinkBusToVisibleConnectionsLocal(Host, OutletBus);
			}
		}

		return Outcome;
	}

	Tracked.bAwaitingStructuralSite = false;

	if (bBulk)
	{
		FStructuralSiteMembership::RegisterOnBulkLoad(
			Session,
			Host,
			Request.Kind,
			Tracked,
			Site);
		Outcome.bAttached = true;
		return Outcome;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Host);
	Outcome.OutletBus = OutletBus;
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(
			Host,
			TEXT("bridge_bus_create_failed"),
			ELogVerbosity::Warning);
		return Outcome;
	}

	FStructuralSiteMembershipParams Params;
	Params.bLinkVisibleConnections = true;
	Params.bBridgePeersOnly = true;
	Params.bMeshOnlyLinks = false;
	Params.bSkipEndpointIndexDirty = Site.SiteRoot != INDEX_NONE;

	if (!Session.HasBridgeBusPeerMesh(OutletBus))
	{
		FStructuralSiteMembership::IntegrateOnPlace(Session,
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
		Session.RegisterBuildableActor(Host);
		Session.LinkBusToVisibleConnectionsLocal(Host, OutletBus);
	}

	Outcome.bAttached = true;
	return Outcome;
}
