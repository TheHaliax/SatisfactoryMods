// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerPoleProcessor.h"

#include "Attach/FStructuralBridgeAttach.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPoleWireUtil.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

void FStructuralPowerPoleProcessor::ResolvePoleStructuralSite(
	FStructuralGraphSession& Session,
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

	FStructuralSiteContext Site;
	if (FStructuralSiteMembership::ResolveSiteContext(
			Session,
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
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("pole.Process"),
		IsValid(Pole) ? Pole->GetName() : TEXT("null"));

	if (!IsValid(Pole))
	{
		return;
	}

	FStructuralGraphSession& Session = Ctx.Session();
	const bool bBulk = Session.IsBulkLoadDrainActive();
	const FStructuralNodeId PoleId = Session.MakeNodeId(Pole);

	if (!bBulk && Ctx.GetAttachContext() != EAttachContext::WireDelta)
	{
		if (const FTrackedEndpoint* Existing = Session.TrackedEndpoints().Find(PoleId))
		{
			if (Existing->Kind == EStructuralEndpointKind::Pole
				&& IsValid(Session.FindBusConnector(Pole))
				&& !Existing->ParentId.IsValid()
				&& !Existing->bAwaitingStructuralSite
				&& !FStructuralPoleWireUtil::HasVanillaWire(Pole))
			{
				return;
			}
		}
	}

	if (!bBulk && Ctx.GetAttachContext() == EAttachContext::WireDelta)
	{
		if (FStructuralBridgeAttach::HasPlacementMembership(Session,
				Pole,
				EStructuralEndpointKind::Pole))
		{
			OnWireDelta(Ctx, Pole);
			return;
		}
	}

	FStructuralBridgeAttachRequest Request;
	Request.Host = Pole;
	Request.Kind = EStructuralEndpointKind::Pole;
	Request.bUsePoleRootResolver = true;

	const FStructuralBridgeAttachOutcome Outcome = FStructuralBridgeAttach::AttachOnPlace(Ctx, Request);
	if (!Outcome.OutletBus && !(bBulk && Outcome.bAttached))
	{
		return;
	}

	const FStructuralSiteContext& Site = Outcome.Site;
	const bool bStructurallyAnchored = Site.bAnchored && Site.SiteRoot != INDEX_NONE;

	if (bStructurallyAnchored && Site.SiteRoot != INDEX_NONE && Site.ParentId.IsValid())
	{
		Session.AddEndpointToRootIndex(Site.SiteRoot, EStructuralEndpointKind::Pole, PoleId);
	}
	else if (!bBulk)
	{
		Session.MarkBridgeEndpointRootIndexDirty();
	}

	FStructuralOutletParentResolveResult ParentResolve;
	ParentResolve.Anchor = Site.ParentAnchor;
	ParentResolve.Method = Site.ParentMethod;

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Pole);
	const float ParentDistCm = ParentResolve.Anchor.IsValid()
		? FVector::Dist(AnchorLocation, ParentResolve.Anchor.WorldLocation)
		: -1.0f;
	const TCHAR* PowerPath = bStructurallyAnchored ? TEXT("structural_mesh") : TEXT("vanilla_wire");

	if (bBulk)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] outlet %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d"
				" parentMethod=%s distCm=%.0f path=%s busCircuit=%d powered=%d tag=%s id=%s"),
			*Pole->GetName(),
			StructuralEndpointKindToString(EStructuralEndpointKind::Pole),
			StructuralPowerScopeToString(EStructuralPowerScope::Site),
			Site.SiteRoot,
			StructuralPowerRoleToString(EStructuralPowerRole::Router),
			Site.SiteRoot,
			bStructurallyAnchored ? 1 : 0,
			FStructuralOutletParentResolver::FormatParentMethod(ParentResolve.Method),
			ParentDistCm,
			PowerPath,
			Outcome.OutletBus ? Outcome.OutletBus->GetCircuitID() : -1,
			Outcome.OutletBus
				&& FStructuralCircuitPromotionUtil::ComponentCarriesPower(Outcome.OutletBus)
					? 1
					: 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] outlet %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d"
				" parentMethod=%s distCm=%.0f path=%s busCircuit=%d powered=%d tag=%s id=%s"),
			*Pole->GetName(),
			StructuralEndpointKindToString(EStructuralEndpointKind::Pole),
			StructuralPowerScopeToString(EStructuralPowerScope::Site),
			Site.SiteRoot,
			StructuralPowerRoleToString(EStructuralPowerRole::Router),
			Site.SiteRoot,
			bStructurallyAnchored ? 1 : 0,
			FStructuralOutletParentResolver::FormatParentMethod(ParentResolve.Method),
			ParentDistCm,
			PowerPath,
			Outcome.OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(Outcome.OutletBus) ? 1 : 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
}

void FStructuralPowerPoleProcessor::OnWireDelta(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole) || Ctx.GetAttachContext() != EAttachContext::WireDelta)
	{
		return;
	}

	FStructuralGraphSession& Session = Ctx.Session();
	if (!FStructuralBridgeAttach::HasPlacementMembership(Session,
			Pole,
			EStructuralEndpointKind::Pole))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(Session.GetOrCreateBusConnector(Pole));
	if (!OutletBus)
	{
		return;
	}

	const FStructuralNodeId PoleId = Session.MakeNodeId(Pole);
	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(PoleId);

	FStructuralSiteContext Site;
	if (!FStructuralSiteMembership::ResolveSiteContext(Session,
			Pole,
			Site,
			/*bUsePoleRootResolver=*/true))
	{
		Site.bAnchored = false;
	}

	Session.LinkBusToVisibleConnectionsLocal(Pole, OutletBus);

	if (Site.bAnchored && Site.SiteRoot != INDEX_NONE && !Session.HasBridgeBusPeerMesh(OutletBus))
	{
		Session.TryMeshPeerBusOnComponent(
			Pole,
			OutletBus,
			Site.SiteRoot,
			PoleId,
			/*bBridgePeersOnly=*/true,
			/*bMeshOnlyLinks=*/true);
	}
	else if (FStructuralCircuitPromotionUtil::ComponentOnCircuit(OutletBus))
	{
		Session.PromoteDirectHiddenLinks(OutletBus);
	}

	if (Site.bAnchored && Site.SiteRoot != INDEX_NONE)
	{
		Tracked.ParentId = Site.ParentId;
		Session.AddEndpointToRootIndex(
			Site.SiteRoot,
			EStructuralEndpointKind::Pole,
			PoleId);
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Pole);
	const float ParentDistCm = Site.ParentAnchor.IsValid()
		? FVector::Dist(AnchorLocation, Site.ParentAnchor.WorldLocation)
		: -1.0f;
	const TCHAR* PowerPath = Site.bAnchored ? TEXT("structural_mesh") : TEXT("vanilla_wire");

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] pole wire delta %s root=%d structural=%d distCm=%.0f"
			" path=%s busCircuit=%d powered=%d"),
		*Pole->GetName(),
		Site.SiteRoot,
		Site.bAnchored ? 1 : 0,
		ParentDistCm,
		PowerPath,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(OutletBus) ? 1 : 0);
}
