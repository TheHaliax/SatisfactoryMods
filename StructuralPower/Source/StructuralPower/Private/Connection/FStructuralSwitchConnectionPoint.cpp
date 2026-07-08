// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralSwitchConnectionPoint.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Core/FStructuralPowerContext.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

FStructuralSwitchConnectionPoint::FStructuralSwitchConnectionPoint(
	AStructuralPowerGraphSubsystem& InGraph,
	AFGBuildableCircuitSwitch* InSwitch)
	: Graph(InGraph)
	, Switch(InSwitch)
{
}

UFGPowerConnectionComponent* FStructuralSwitchConnectionPoint::GetStructuralConnector()
{
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr))
	{
		return nullptr;
	}

	return Graph.GetOrCreateBusConnector(SwitchPtr);
}

FStructuralWallAnchor FStructuralSwitchConnectionPoint::GetStructureAnchor() const
{
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr))
	{
		return {};
	}

	FStructuralSiteContext Site;
	if (FStructuralSiteMembership::ResolveSiteContext(Graph, SwitchPtr, Site))
	{
		return Site.ParentAnchor;
	}

	return Graph.ResolveOutletAnchor(SwitchPtr);
}

bool FStructuralSwitchConnectionPoint::ResolveTrackedRoot(
	FStructuralNodeId& OutSwitchId,
	int32& OutRoot,
	bool& OutStructurallyAnchored)
{
	OutStructurallyAnchored = false;
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr))
	{
		return false;
	}

	OutSwitchId = Graph.MakeNodeId(SwitchPtr);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(OutSwitchId);
	Tracked.Actor = SwitchPtr;
	Tracked.Kind = EStructuralEndpointKind::Switch;
	Graph.RegisterBuildableActor(SwitchPtr);

	FStructuralPowerContext ListenerCtx = Graph.MakeProcessorContext(EAttachContext::WireDelta);
	FStructuralPowerSwitchProcessor::EnsureListener(ListenerCtx, SwitchPtr);

	if (Tracked.ParentId.IsValid())
	{
		OutRoot = Graph.FindRootForTrackedEndpoint(Tracked);
		if (OutRoot != INDEX_NONE)
		{
			OutStructurallyAnchored = true;
			return true;
		}
	}

	FStructuralSiteContext Site;
	if (FStructuralSiteMembership::ResolveSiteContext(Graph, SwitchPtr, Site))
	{
		OutRoot = Site.SiteRoot;
		OutStructurallyAnchored = Site.bAnchored;
		Tracked.ParentId = Site.ParentId;
		if (OutRoot != INDEX_NONE)
		{
			Graph.MarkBridgeEndpointRootIndexDirty();
		}
		return OutRoot != INDEX_NONE;
	}

	OutStructurallyAnchored = false;
	return false;
}

void FStructuralSwitchConnectionPoint::OnWireOrGateChanged(EAttachContext AttachContext)
{
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr))
	{
		return;
	}

	if (AttachContext == EAttachContext::Toggle)
	{
		return;
	}

	FStructuralNodeId SwitchId;
	int32 Root = INDEX_NONE;
	bool bStructurallyAnchored = false;
	ResolveTrackedRoot(SwitchId, Root, bStructurallyAnchored);

	FStructuralPowerContext Ctx = Graph.MakeProcessorContext(AttachContext, Root);

	const bool bKeyedSubnet = FStructuralPowerSwitchProcessor::HasAssignedControl(Ctx, SwitchPtr);
	const bool bHasWire = FStructuralSwitchParentResolver::HasAnyVanillaWire(SwitchPtr);

	if (AttachContext == EAttachContext::WireDelta)
	{
		UFGStructuralPowerConnectionComponent* OutletBus =
			Cast<UFGStructuralPowerConnectionComponent>(GetStructuralConnector());

		if (bHasWire || bKeyedSubnet)
		{
			FStructuralPowerSwitchProcessor::SyncDirectedBridgePair(Ctx, SwitchPtr);
		}
		else
		{
			FStructuralPowerSwitchProcessor::DisarmDirectedPair(Ctx, SwitchPtr);
		}

		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] switch wire delta %s scope=%s site=%d role=%s attach=%s"
				" root=%d busCircuit=%d wired=%d"),
			*SwitchPtr->GetName(),
			StructuralPowerScopeToString(EStructuralPowerScope::Site),
			Root,
			StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
			AttachContextToString(AttachContext),
			Root,
			IsValid(OutletBus) ? OutletBus->GetCircuitID() : INDEX_NONE,
			bHasWire ? 1 : 0);
		return;
	}

	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(SwitchId);

	FStructuralSiteContext Site;
	if (FStructuralSiteMembership::ResolveSiteContext(Graph, SwitchPtr, Site))
	{
		Root = Site.SiteRoot;
		bStructurallyAnchored = Site.bAnchored;
	}
	else
	{
		Site.ParentId = Tracked.ParentId;
		Site.SiteRoot = Root;
		Site.bAnchored = bStructurallyAnchored;
	}

	const bool bSwitchOn = SwitchPtr->IsBridgeActive();

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(GetStructuralConnector());
	if (!OutletBus)
	{
		return;
	}

	FStructuralSiteMembershipParams Params;
	Params.bStripSwitchVanillaPortLinks = true;
	Params.bBridgePeersOnly = true;
	Params.bSkipEndpointIndexDirty = Site.SiteRoot != INDEX_NONE;
	FStructuralSiteMembership::IntegrateOnPlace(
		Graph,
		SwitchPtr,
		OutletBus,
		SwitchId,
		EStructuralEndpointKind::Switch,
		Tracked,
		Site,
		Params);

	if (Site.bAnchored && Site.SiteRoot != INDEX_NONE)
	{
		Graph.AddEndpointToRootIndex(
			Site.SiteRoot,
			EStructuralEndpointKind::Switch,
			SwitchId);
	}

	FStructuralPowerSwitchProcessor::ArmPlacementSourceBridgeLegs(OutletBus, SwitchPtr);
	if (bHasWire || bKeyedSubnet)
	{
		FStructuralPowerSwitchProcessor::SyncDirectedBridgePair(Ctx, SwitchPtr);
	}

	if (bHasWire && Root != INDEX_NONE)
	{
		FStructuralPowerSwitchProcessor::PropagateWiredFeedChange(Ctx, SwitchPtr, Root);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] switch wire delta %s scope=%s site=%d role=%s attach=%s"
			" root=%d busCircuit=%d powered=%d wired=%d transfer=%d"),
		*SwitchPtr->GetName(),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
		AttachContextToString(AttachContext),
		Root,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
		bHasWire ? 1 : 0,
		bSwitchOn ? 1 : 0);
}
