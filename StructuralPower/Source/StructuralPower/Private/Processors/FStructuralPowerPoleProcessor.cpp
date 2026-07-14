// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerPoleProcessor.h"

#include "Attach/FStructuralBridgeAttach.h"
#include "Attach/FStructuralEndpointAttach.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Core/EStructuralAttachStrategy.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Routing/EStructuralChannel.h"
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
		OutParentId = Site.MountParentId;
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
	const FStructuralNodeId PoleId = Session.MakeNodeId(Pole);

	if (!bBulk && Site.SiteRoot == INDEX_NONE)
	{
		Session.MarkBridgeEndpointRootIndexDirty();
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Pole);
	const float ParentDistCm = Site.ParentAnchor.IsValid()
		? FVector::Dist(AnchorLocation, Site.ParentAnchor.WorldLocation)
		: -1.0f;
	const TCHAR* PowerPath = bStructurallyAnchored ? TEXT("structural_mesh") : TEXT("vanilla_wire");
	const int32 BusCircuit = Outcome.OutletBus ? Outcome.OutletBus->GetCircuitID() : -1;
	const int32 Powered = Outcome.OutletBus
		&& FStructuralCircuitPromotionUtil::ComponentCarriesPower(Outcome.OutletBus)
			? 1
			: 0;

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
			FStructuralOutletParentResolver::FormatParentMethod(Site.ParentMethod),
			ParentDistCm,
			PowerPath,
			BusCircuit,
			Powered,
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
			FStructuralOutletParentResolver::FormatParentMethod(Site.ParentMethod),
			ParentDistCm,
			PowerPath,
			BusCircuit,
			Powered,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
	(void)PoleId;
}

void FStructuralPowerPoleProcessor::OnWireDelta(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole))
	{
		return;
	}

	FStructuralEndpointAttach::RunStrategy(
		Ctx,
		Pole,
		EStructuralAttachStrategy::Bridge);
}
