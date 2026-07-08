// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerPoleProcessor.h"

#include "Buildables/FGBuildablePowerPole.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Connection/FStructuralPoleConnectionPoint.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

void FStructuralPowerPoleProcessor::ResolvePoleStructuralSite(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildablePowerPole* Pole,
	FStructuralNodeId& OutParentId,
	int32& OutRoot,
	bool& bStructurallyAnchored,
	FStructuralOutletParentResolveResult* OutParentResolve)
{
	OutParentId = FStructuralNodeId();
	OutRoot = INDEX_NONE;
	bStructurallyAnchored = false;

	if (!IsValid(Pole))
	{
		return;
	}

	const FStructuralOutletParentResolveParams Params = Graph.MakeOutletParentResolveParams();
	FStructuralSiteContext Site;
	if (FStructuralSiteMembership::ResolveSiteContext(
			Graph,
			Pole,
			Site,
			/*bUsePoleRootResolver=*/true))
	{
		bStructurallyAnchored = Site.bAnchored;
		OutRoot = Site.SiteRoot;
		OutParentId = Site.ParentId;
	}

	if (OutParentResolve)
	{
		OutParentResolve->Anchor = Site.ParentAnchor;
		OutParentResolve->Method = Site.ParentMethod;
	}
}

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
	if (!bBulk && Ctx.GetAttachContext() == EAttachContext::WireDelta)
	{
		if (const FTrackedEndpoint* Existing = Graph.TrackedEndpoints.Find(PoleId))
		{
			if (Existing->Kind == EStructuralEndpointKind::Pole
				&& Existing->ParentId.IsValid()
				&& Existing->bStructuralPowerTransferActive)
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

	FStructuralNodeId ParentId;
	int32 Root = INDEX_NONE;
	bool bStructurallyAnchored = false;
	FStructuralOutletParentResolveResult ParentResolve;
	ResolvePoleStructuralSite(
		Graph,
		Pole,
		ParentId,
		Root,
		bStructurallyAnchored,
		&ParentResolve);

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Pole);
	const float ParentDistCm = ParentResolve.Anchor.IsValid()
		? FVector::Dist(AnchorLocation, ParentResolve.Anchor.WorldLocation)
		: -1.0f;
	const TCHAR* PowerPath = bStructurallyAnchored ? TEXT("structural_mesh") : TEXT("vanilla_wire");

	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(PoleId);

	FStructuralSiteContext Site;
	Site.ParentAnchor = ParentResolve.Anchor;
	Site.ParentMethod = ParentResolve.Method;
	Site.ParentId = ParentId;
	Site.SiteRoot = Root;
	Site.bAnchored = bStructurallyAnchored;

	FStructuralSiteMembershipParams Params;
	Params.bLinkVisibleConnections = !bBulk;
	if (bBulk)
	{
		Graph.LinkBusToVisibleConnections(Pole, OutletBus);
	}
	else
	{
		Params.bLinkVisibleConnections = true;
	}

	Params.bBridgePeersOnly = bBulk;
	Params.bSkipEndpointIndexDirty = !bBulk && Root != INDEX_NONE;
	if (bStructurallyAnchored && Root != INDEX_NONE && (bBulk || !Graph.HasBridgeBusPeerMesh(OutletBus)))
	{
		FStructuralSiteMembership::IntegrateOnPlace(
			Graph,
			Pole,
			OutletBus,
			PoleId,
			EStructuralEndpointKind::Pole,
			Tracked,
			Site,
			Params);
	}
	else
	{
		Tracked.Actor = Pole;
		Tracked.ParentId = ParentId;
		Tracked.Kind = EStructuralEndpointKind::Pole;
		Tracked.bStructuralPowerTransferActive = bStructurallyAnchored;
		Graph.RegisterBuildableActor(Pole);
		if (Params.bLinkVisibleConnections)
		{
			Graph.LinkBusToVisibleConnectionsLocal(Pole, OutletBus);
		}
	}

	if (bStructurallyAnchored && Root != INDEX_NONE && ParentId.IsValid())
	{
		Graph.AddEndpointToRootIndex(Root, EStructuralEndpointKind::Pole, PoleId);
	}
	else if (!bBulk)
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	if (bBulk)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] outlet %s root=%d parentValid=%d parentMethod=%s distCm=%.0f"
				" path=%s busCircuit=%d powered=%d tag=%s id=%s"),
			*Pole->GetName(),
			Root,
			bStructurallyAnchored ? 1 : 0,
			FStructuralOutletParentResolver::FormatParentMethod(ParentResolve.Method),
			ParentDistCm,
			PowerPath,
			OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] outlet %s root=%d parentValid=%d parentMethod=%s distCm=%.0f"
				" path=%s busCircuit=%d powered=%d tag=%s id=%s"),
			*Pole->GetName(),
			Root,
			bStructurallyAnchored ? 1 : 0,
			FStructuralOutletParentResolver::FormatParentMethod(ParentResolve.Method),
			ParentDistCm,
			PowerPath,
			OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
}
