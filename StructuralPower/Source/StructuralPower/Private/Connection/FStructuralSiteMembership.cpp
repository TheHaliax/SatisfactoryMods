// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralSiteMembership.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralHostAttachAdapter.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "StructuralPowerLog.h"

namespace
{
static void StripSwitchVanillaPortHiddenLinks(
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* Bus)
{
	if (!IsValid(Switch) || !IsValid(Bus))
	{
		return;
	}

	auto OpenLeg = [&](UFGCircuitConnectionComponent* Leg)
	{
		if (!IsValid(Leg) || Bus == Leg)
		{
			return;
		}

		if (Bus->HasHiddenConnection(Leg))
		{
			Bus->RemoveHiddenConnection(Leg);
		}
	};

	OpenLeg(Switch->GetConnection0());
	OpenLeg(Switch->GetConnection1());
}
} // namespace

bool FStructuralSiteMembership::ResolveSiteContext(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildable* Endpoint,
	FStructuralSiteContext& OutSite,
	bool bUsePoleRootResolver)
{
	OutSite = FStructuralSiteContext{};
	if (!IsValid(Endpoint))
	{
		return false;
	}

	const FStructuralOutletParentResolveParams Params = Graph.MakeOutletParentResolveParams();
	const FStructuralOutletParentResolveResult ParentResolve =
		FStructuralOutletParentResolver::ResolveDetailed(Endpoint, Graph.GetWorld(), Params);

	OutSite.ParentAnchor = ParentResolve.Anchor;
	OutSite.ParentMethod = ParentResolve.Method;
	OutSite.HostKind = FStructuralHostAttachAdapter::ClassifyHost(ParentResolve.Anchor);
	OutSite.bAnchored = FStructuralHostAttachAdapter::ConfirmSiteAttach(ParentResolve, Endpoint);

	if (!OutSite.bAnchored)
	{
		return false;
	}

	if (bUsePoleRootResolver)
	{
		AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Endpoint);
		if (IsValid(Pole))
		{
			OutSite.SiteRoot = Graph.ResolvePoleComponentRoot(
				Pole,
				OutSite.ParentAnchor,
				OutSite.ParentId);
		}
	}
	else
	{
		OutSite.SiteRoot = Graph.ResolveEndpointComponentRoot(
			Endpoint,
			OutSite.ParentAnchor,
			OutSite.ParentId);
	}

	return OutSite.SiteRoot != INDEX_NONE;
}

void FStructuralSiteMembership::IntegrateOnPlace(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* OutletBus,
	const FStructuralNodeId& EndpointId,
	EStructuralEndpointKind Kind,
	FTrackedEndpoint& Tracked,
	const FStructuralSiteContext& Site,
	const FStructuralSiteMembershipParams& Params)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("site.IntegrateOnPlace"),
		IsValid(Host) ? Host->GetName() : TEXT("null"));

	if (!IsValid(Host) || !IsValid(OutletBus))
	{
		return;
	}

	Tracked.Actor = Host;
	Tracked.Kind = Kind;
	Tracked.ParentId = Site.ParentId;
	Graph.RegisterBuildableActor(Host);
	if (!Params.bSkipEndpointIndexDirty)
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	if (Kind != EStructuralEndpointKind::Switch)
	{
		Tracked.bStructuralPowerTransferActive = Site.bAnchored && Site.SiteRoot != INDEX_NONE;
	}

	if (Params.bStripSwitchVanillaPortLinks)
	{
		if (AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Host))
		{
			StripSwitchVanillaPortHiddenLinks(Switch, OutletBus);
		}
	}

	if (Params.bLinkVisibleConnections)
	{
		Graph.LinkBusToVisibleConnectionsLocal(Host, OutletBus, Params.bMeshOnlyLinks);
	}

	if (Site.bAnchored && Site.SiteRoot != INDEX_NONE && !Graph.HasBridgeBusPeerMesh(OutletBus))
	{
		Graph.TryMeshPeerBusOnComponent(
			Host,
			OutletBus,
			Site.SiteRoot,
			EndpointId,
			Params.bBridgePeersOnly,
			Params.bMeshOnlyLinks);
	}

	if (Site.bAnchored && !Params.bMeshOnlyLinks)
	{
		Graph.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Host);
	const float ParentDistCm = Site.ParentAnchor.IsValid()
		? FVector::Dist(AnchorLocation, Site.ParentAnchor.WorldLocation)
		: -1.0f;

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] site integrate %s kind=%s host=%d site=%d anchored=%d"
			" parentMethod=%s distCm=%.0f busCircuit=%d powered=%d"),
		*Host->GetName(),
		StructuralEndpointKindToString(Kind),
		static_cast<int32>(Site.HostKind),
		Site.SiteRoot,
		Site.bAnchored ? 1 : 0,
		FStructuralOutletParentResolver::FormatParentMethod(Site.ParentMethod),
		ParentDistCm,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);
}
