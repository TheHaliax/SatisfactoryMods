// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralSiteMembership.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/FStructuralGraphSession.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralHostAttachAdapter.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Routing/EStructuralChannel.h"
#include "Save/FStructuralEndpointIdRegistry.h"
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
	FStructuralGraphSession& Session,
	AFGBuildable* Endpoint,
	FStructuralSiteContext& OutSite,
	bool bUsePoleRootResolver)
{
	OutSite = FStructuralSiteContext{};
	if (!IsValid(Endpoint))
	{
		return false;
	}

	const FStructuralOutletParentResolveParams Params = Session.MakeOutletParentResolveParams();
	const FStructuralOutletParentResolveResult ParentResolve =
		FStructuralOutletParentResolver::ResolveDetailed(Endpoint, Session.GetWorld(), Params);

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
			OutSite.SiteRoot = Session.ResolvePoleComponentRoot(
				Pole,
				OutSite.ParentAnchor,
				OutSite.MountParentId);
		}
	}
	else
	{
		OutSite.SiteRoot = Session.ResolveEndpointComponentRoot(
			Endpoint,
			OutSite.ParentAnchor,
			OutSite.MountParentId);
	}

	return OutSite.SiteRoot != INDEX_NONE;
}

void FStructuralSiteMembership::RegisterOnBulkLoad(
	FStructuralGraphSession& Session,
	AFGBuildable* Host,
	EStructuralEndpointKind Kind,
	FTrackedEndpoint& Tracked,
	const FStructuralSiteContext& Site)
{
	if (!IsValid(Host))
	{
		return;
	}

	Tracked.Actor = Host;
	Tracked.Kind = Kind;
	Tracked.MountParentId = Site.MountParentId;
	Tracked.bAwaitingStructuralSite = false;
	Session.RegisterBuildableActor(Host);

	if (Kind != EStructuralEndpointKind::Switch)
	{
		Tracked.bStructuralPowerTransferActive = Site.bAnchored && Site.SiteRoot != INDEX_NONE;
	}
}

void FStructuralSiteMembership::IntegrateOnPlace(
	FStructuralGraphSession& Session,
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
	Tracked.MountParentId = Site.MountParentId;
	Tracked.bAwaitingStructuralSite = !(Site.bAnchored && Site.SiteRoot != INDEX_NONE);
	Session.RegisterBuildableActor(Host);
	if (!Params.bSkipEndpointIndexDirty)
	{
		Session.MarkBridgeEndpointRootIndexDirty();
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
		Session.LinkBusToVisibleConnectionsLocal(Host, OutletBus, Params.bMeshOnlyLinks);
	}

	if (Site.bAnchored && Site.SiteRoot != INDEX_NONE && !Session.HasBridgeBusPeerMesh(OutletBus)
		&& !Session.IsBulkLoadDrainActive())
	{
		Session.TryMeshPeerBusOnComponent(
			Host,
			OutletBus,
			Site.SiteRoot,
			EndpointId,
			Params.bBridgePeersOnly,
			Params.bMeshOnlyLinks);
	}

	if (Site.bAnchored && !Params.bMeshOnlyLinks)
	{
		Session.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Host);
	const float ParentDistCm = Site.ParentAnchor.IsValid()
		? FVector::Dist(AnchorLocation, Site.ParentAnchor.WorldLocation)
		: -1.0f;
	if (!Session.IsBulkLoadDrainActive())
	{
		FName StructureName = NAME_None;
		const FStructuralComponentKey CompKey = Session.MakeComponentKeyForRoot(Site.SiteRoot);
		if (CompKey.IsValid())
		{
			Session.IdRegistry().TryGetComponentDefaultId(CompKey, StructureName);
		}
		const FString MountLabel = Site.MountParentId.IsLightweight()
			? FString::Printf(TEXT("LW%d"), Site.MountParentId.LightweightIndex)
			: Site.MountParentId.ActorName.ToString();

		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] site integrate %s kind=%s structure=%s site=%d mount=%s host=%d"
				" anchored=%d parentMethod=%s distCm=%.0f busCircuit=%d powered=%d"),
			*Host->GetName(),
			StructuralEndpointKindToString(Kind),
			StructureName.IsNone() ? TEXT("-") : *StructureName.ToString(),
			Site.SiteRoot,
			*MountLabel,
			static_cast<int32>(Site.HostKind),
			Site.bAnchored ? 1 : 0,
			FStructuralOutletParentResolver::FormatParentMethod(Site.ParentMethod),
			ParentDistCm,
			OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);
	}
}

int32 FStructuralSiteMembership::SiteRootFromMount(
	FStructuralGraphSession& Session,
	const FStructuralNodeId& MountParentId)
{
	if (!MountParentId.IsValid())
	{
		return INDEX_NONE;
	}

	return Session.StructureGraph().FindRoot(MountParentId);
}

bool FStructuralSiteMembership::ReaffirmMountParent(
	FStructuralGraphSession& Session,
	AFGBuildable* Host,
	FTrackedEndpoint& Tracked,
	const bool bUsePoleRootResolver)
{
	if (!IsValid(Host))
	{
		return false;
	}

	if (Tracked.MountParentId.IsValid())
	{
		const int32 Root = SiteRootFromMount(Session, Tracked.MountParentId);
		if (Root != INDEX_NONE)
		{
			Tracked.bAwaitingStructuralSite = false;
			return true;
		}
	}

	FStructuralSiteContext Site;
	if (!ResolveSiteContext(Session, Host, Site, bUsePoleRootResolver)
		|| !Site.MountParentId.IsValid()
		|| Site.SiteRoot == INDEX_NONE)
	{
		Tracked.bAwaitingStructuralSite = true;
		return false;
	}

	Tracked.MountParentId = Site.MountParentId;
	Tracked.bAwaitingStructuralSite = false;
	return true;
}
