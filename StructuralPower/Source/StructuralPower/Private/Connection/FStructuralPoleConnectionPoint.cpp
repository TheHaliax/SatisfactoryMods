// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralPoleConnectionPoint.h"

#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/EAttachContext.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
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

void FStructuralPoleConnectionPoint::OnWireOrGateChanged(EAttachContext AttachContext)
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

	FStructuralSiteContext Site;
	if (!FStructuralSiteMembership::ResolveSiteContext(
			Graph,
			PolePtr,
			Site,
			/*bUsePoleRootResolver=*/true))
	{
		Site.bAnchored = false;
	}

	if (AttachContext == EAttachContext::WireDelta)
	{
		// FG pole OnPowerConnectionChanged already rebuilt wire circuits — delta only.
		Graph.LinkBusToVisibleConnectionsLocal(PolePtr, OutletBus);

		if (Site.bAnchored && Site.SiteRoot != INDEX_NONE && !Graph.HasBridgeBusPeerMesh(OutletBus))
		{
			Graph.TryMeshPeerBusOnComponent(
				PolePtr,
				OutletBus,
				Site.SiteRoot,
				PoleId,
				/*bBridgePeersOnly=*/true,
				/*bMeshOnlyLinks=*/true);
		}
		else if (FStructuralCircuitPromotionUtil::ComponentOnCircuit(OutletBus))
		{
			Graph.PromoteDirectHiddenLinks(OutletBus);
		}

		if (Site.bAnchored && Site.SiteRoot != INDEX_NONE)
		{
			Tracked.ParentId = Site.ParentId;
			Graph.AddEndpointToRootIndex(
				Site.SiteRoot,
				EStructuralEndpointKind::Pole,
				PoleId);
		}

		const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(PolePtr);
		const float ParentDistCm = Site.ParentAnchor.IsValid()
			? FVector::Dist(AnchorLocation, Site.ParentAnchor.WorldLocation)
			: -1.0f;
		const TCHAR* PowerPath = Site.bAnchored ? TEXT("structural_mesh") : TEXT("vanilla_wire");

		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] pole wire delta %s root=%d structural=%d distCm=%.0f"
				" path=%s busCircuit=%d powered=%d"),
			*PolePtr->GetName(),
			Site.SiteRoot,
			Site.bAnchored ? 1 : 0,
			ParentDistCm,
			PowerPath,
			OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(OutletBus) ? 1 : 0);
		return;
	}

	if (Graph.HasBridgeBusPeerMesh(OutletBus))
	{
		Graph.LinkBusToVisibleConnectionsLocal(PolePtr, OutletBus);
	}
	else
	{
		FStructuralSiteMembershipParams Params;
		Params.bLinkVisibleConnections = true;
		Params.bBridgePeersOnly = true;
		Params.bSkipEndpointIndexDirty = Site.SiteRoot != INDEX_NONE;
		FStructuralSiteMembership::IntegrateOnPlace(
			Graph,
			PolePtr,
			OutletBus,
			PoleId,
			EStructuralEndpointKind::Pole,
			Tracked,
			Site,
			Params);

		if (Site.bAnchored && Site.SiteRoot != INDEX_NONE)
		{
			Graph.AddEndpointToRootIndex(
				Site.SiteRoot,
				EStructuralEndpointKind::Pole,
				PoleId);
		}
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(PolePtr);
	const float ParentDistCm = Site.ParentAnchor.IsValid()
		? FVector::Dist(AnchorLocation, Site.ParentAnchor.WorldLocation)
		: -1.0f;
	const TCHAR* PowerPath = Site.bAnchored ? TEXT("structural_mesh") : TEXT("vanilla_wire");

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] pole wire delta %s root=%d structural=%d distCm=%.0f"
			" path=%s busCircuit=%d powered=%d"),
		*PolePtr->GetName(),
		Site.SiteRoot,
		Site.bAnchored ? 1 : 0,
		ParentDistCm,
		PowerPath,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(OutletBus) ? 1 : 0);
}
