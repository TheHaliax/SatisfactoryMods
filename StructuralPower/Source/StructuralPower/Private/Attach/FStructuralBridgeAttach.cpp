// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralBridgeAttach.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralGraphSession.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Graph/FStructuralPoleWireUtil.h"
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
		&& Tracked->MountParentId.IsValid();
}

namespace
{
bool TryIdleSkip(
	FStructuralGraphSession& Session,
	AFGBuildable* Host,
	EStructuralEndpointKind Kind,
	const EAttachContext AttachContext)
{
	if (Kind != EStructuralEndpointKind::Pole
		|| Session.IsBulkLoadDrainActive()
		|| AttachContext == EAttachContext::WireDelta)
	{
		return false;
	}

	AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host);
	if (!IsValid(Pole))
	{
		return false;
	}

	const FStructuralNodeId EndpointId = Session.MakeNodeId(Host);
	const FTrackedEndpoint* Existing = Session.TrackedEndpoints().Find(EndpointId);
	if (!Existing
		|| Existing->Kind != EStructuralEndpointKind::Pole
		|| !IsValid(Session.FindBusConnector(Host))
		|| Existing->MountParentId.IsValid()
		|| Existing->bAwaitingStructuralSite
		|| FStructuralPoleWireUtil::HasVanillaWire(Pole))
	{
		return false;
	}

	return true;
}

FStructuralBridgeAttachOutcome RefreshWireDeltaMembership(
	FStructuralPowerContext& Ctx,
	const FStructuralBridgeAttachRequest& Request)
{
	FStructuralBridgeAttachOutcome Outcome;
	AFGBuildable* Host = Request.Host;
	FStructuralGraphSession& Session = Ctx.Session();
	const FStructuralNodeId EndpointId = Session.MakeNodeId(Host);
	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(EndpointId);
	FStructuralSiteContext& Site = Outcome.Site;

	UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Host);
	Outcome.OutletBus = OutletBus;
	if (!OutletBus)
	{
		return Outcome;
	}

	if (!FStructuralSiteMembership::ResolveSiteContext(
			Session,
			Host,
			Site,
			Request.bUsePoleRootResolver))
	{
		if (Request.bUsePoleRootResolver)
		{
			Site.bAnchored = false;
		}
		else if (Tracked.MountParentId.IsValid())
		{
			Site.MountParentId = Tracked.MountParentId;
			Site.SiteRoot = Session.FindRootForTrackedEndpoint(Tracked);
			Site.bAnchored = Site.SiteRoot != INDEX_NONE;
		}
	}

	FStructuralSiteMembershipParams Params;
	Params.bLinkVisibleConnections = true;
	Params.bBridgePeersOnly = true;
	Params.bMeshOnlyLinks = false;
	Params.bSkipEndpointIndexDirty = Site.SiteRoot != INDEX_NONE;

	if (Site.bAnchored && Site.SiteRoot != INDEX_NONE)
	{
		FStructuralSiteMembership::IntegrateOnPlace(Session,
			Host,
			OutletBus,
			EndpointId,
			Request.Kind,
			Tracked,
			Site,
			Params);

		if (!Session.Circuit().HasBridgeBusPeerMesh(OutletBus)
			&& FStructuralCircuitPromotionUtil::ComponentOnCircuit(OutletBus))
		{
			Session.Circuit().PromoteDirectHiddenLinks(OutletBus);
		}

		Session.AddEndpointToRootIndex(Site.SiteRoot, Request.Kind, EndpointId);
		Outcome.bAttached = true;
	}
	else
	{
		Session.Circuit().LinkBusToVisibleConnectionsLocal(Host, OutletBus);
		if (FStructuralCircuitPromotionUtil::ComponentOnCircuit(OutletBus))
		{
			Session.Circuit().PromoteDirectHiddenLinks(OutletBus);
		}
		Outcome.bAttached = true;
	}

	return Outcome;
}
} // namespace

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
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const bool bBulk = Session.IsBulkLoadDrainActive();
	const FStructuralNodeId EndpointId = Session.MakeNodeId(Host);
	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(EndpointId);
	FStructuralSiteContext& Site = Outcome.Site;

	if (TryIdleSkip(Session, Host, Request.Kind, AttachContext))
	{
		return Outcome;
	}

	if (AttachContext == EAttachContext::WireDelta
		&& HasPlacementMembership(Session, Host, Request.Kind))
	{
		return RefreshWireDeltaMembership(Ctx, Request);
	}

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
			Session.ResolveEndpointComponentRoot(Host, ParentAnchor, Site.MountParentId);
			Tracked.MountParentId = Site.MountParentId;
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
		Tracked.MountParentId = Site.MountParentId;
		Tracked.bAwaitingStructuralSite = true;
		Session.RegisterBuildableActor(Host);

		if (!bBulk)
		{
			UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Host);
			Outcome.OutletBus = OutletBus;
			if (OutletBus)
			{
				Session.Circuit().LinkBusToVisibleConnectionsLocal(Host, OutletBus);
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
		if (Site.SiteRoot != INDEX_NONE && Site.MountParentId.IsValid())
		{
			Session.AddEndpointToRootIndex(Site.SiteRoot, Request.Kind, EndpointId);
		}
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

	if (!Session.Circuit().HasBridgeBusPeerMesh(OutletBus))
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
		Tracked.MountParentId = Site.MountParentId;
		Tracked.Kind = Request.Kind;
		Tracked.bStructuralPowerTransferActive = Site.bAnchored;
		Session.RegisterBuildableActor(Host);
		Session.Circuit().LinkBusToVisibleConnectionsLocal(Host, OutletBus);
	}

	if (Site.bAnchored && Site.SiteRoot != INDEX_NONE && Site.MountParentId.IsValid())
	{
		Session.AddEndpointToRootIndex(Site.SiteRoot, Request.Kind, EndpointId);
	}

	Outcome.bAttached = true;
	return Outcome;
}
