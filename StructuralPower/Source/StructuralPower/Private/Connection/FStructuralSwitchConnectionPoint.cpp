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
	FStructuralPowerSwitchProcessor::ApplyStructureMembership(Ctx, SwitchPtr);

	if (AttachContext == EAttachContext::WireDelta)
	{
		FStructuralPowerSwitchProcessor::ApplyWireDeltaTransferSideEffects(Ctx, SwitchPtr, Root);
	}

	const bool bHasWire = FStructuralSwitchParentResolver::HasAnyVanillaWire(SwitchPtr);
	const bool bSwitchOn = SwitchPtr->IsBridgeActive();

	const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(SwitchId);
	const bool bGateOpen = Tracked && Tracked->bStructuralPowerTransferActive;

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(GetStructuralConnector());
	if (!OutletBus)
	{
		return;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] switch wire delta %s kind=%s scope=%s site=%d role=%s attach=%s"
			" root=%d busCircuit=%d powered=%d wired=%d transfer=%d gate=%d"),
		*SwitchPtr->GetName(),
		StructuralEndpointKindToString(EStructuralEndpointKind::Switch),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
		AttachContextToString(AttachContext),
		Root,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
		bHasWire ? 1 : 0,
		bSwitchOn ? 1 : 0,
		bGateOpen ? 1 : 0);
}
