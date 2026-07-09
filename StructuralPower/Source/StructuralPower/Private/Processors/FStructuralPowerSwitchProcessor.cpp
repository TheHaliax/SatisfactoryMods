// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerSwitchProcessor.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "StructuralPowerConstants.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralCrossSiteGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Network/UStructuralPowerSwitchListener.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Processors/FStructuralPowerBridgeProcessor.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Core/FStructuralPowerContext.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void FStructuralPowerSwitchProcessor::TearDown(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* Bus = Ctx.Graph().FindBusConnector(Switch);
	if (!IsValid(Bus))
	{
		return;
	}

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Bus->GetHiddenConnections(HiddenLinks);
	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		Bus->RemoveHiddenConnection(OtherRaw);
		if (IsValid(OtherRaw))
		{
			OtherRaw->RemoveHiddenConnection(Bus);
		}
	}
}

namespace
{
static void OpenDirectedBridgeLeg(
	UFGStructuralPowerConnectionComponent* SourceBus,
	UFGCircuitConnectionComponent* Leg)
{
	if (!IsValid(SourceBus) || !IsValid(Leg) || SourceBus == Leg)
	{
		return;
	}

	if (SourceBus->HasHiddenConnection(Leg))
	{
		SourceBus->RemoveHiddenConnection(Leg);
	}
}

static void CloseDirectedBridgeLeg(
	UFGStructuralPowerConnectionComponent* SourceBus,
	UFGPowerConnectionComponent* Leg)
{
	if (!IsValid(SourceBus) || !IsValid(Leg) || SourceBus == Leg)
	{
		return;
	}

	if (!SourceBus->HasHiddenConnection(Leg))
	{
		SourceBus->AddHiddenConnection(Leg);
	}
}

static UFGPowerConnectionComponent* FindSingleWireSourceBridgeLeg(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return nullptr;
	}

	UFGPowerConnectionComponent* Conn0Power = Cast<UFGPowerConnectionComponent>(Switch->GetConnection0());
	UFGPowerConnectionComponent* Conn1Power = Cast<UFGPowerConnectionComponent>(Switch->GetConnection1());
	const bool bConn0Wired = IsValid(Conn0Power) && Conn0Power->GetNumConnections() > 0;
	const bool bConn1Wired = IsValid(Conn1Power) && Conn1Power->GetNumConnections() > 0;

	if (bConn0Wired && !bConn1Wired && IsValid(Conn1Power))
	{
		return Conn1Power;
	}

	if (bConn1Wired && !bConn0Wired && IsValid(Conn0Power))
	{
		return Conn0Power;
	}

	return nullptr;
}

static void AssignConfiguredBridgeSlots(
	UFGStructuralPowerConnectionComponent* SourceBus,
	UFGStructuralPowerConnectionComponent* ControlBus,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(SourceBus) || !IsValid(ControlBus) || !IsValid(Switch))
	{
		return;
	}

	OpenDirectedBridgeLeg(SourceBus, ControlBus);
	OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
	OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());
	OpenDirectedBridgeLeg(ControlBus, Switch->GetConnection0());
	OpenDirectedBridgeLeg(ControlBus, Switch->GetConnection1());

	if (UFGPowerConnectionComponent* Conn0 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection0()))
	{
		CloseDirectedBridgeLeg(SourceBus, Conn0);
	}

	if (UFGPowerConnectionComponent* Conn1 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection1()))
	{
		CloseDirectedBridgeLeg(ControlBus, Conn1);
	}
}

static void OpenConfiguredBridgeSlots(
	UFGStructuralPowerConnectionComponent* SourceBus,
	UFGStructuralPowerConnectionComponent* ControlBus,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	if (IsValid(SourceBus))
	{
		OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
		OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());
	}

	if (IsValid(ControlBus))
	{
		OpenDirectedBridgeLeg(ControlBus, Switch->GetConnection0());
		OpenDirectedBridgeLeg(ControlBus, Switch->GetConnection1());
	}

	if (IsValid(SourceBus) && IsValid(ControlBus))
	{
		OpenDirectedBridgeLeg(SourceBus, ControlBus);
	}
}
} // namespace

uint8 FStructuralPowerSwitchProcessor::BuildWireSignature(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return 0;
	}

	uint8 Signature = 0;
	if (const UFGPowerConnectionComponent* Conn0 =
			Cast<UFGPowerConnectionComponent>(Switch->GetConnection0()))
	{
		if (Conn0->GetNumConnections() > 0)
		{
			Signature |= 0x1;
		}
	}

	if (const UFGPowerConnectionComponent* Conn1 =
			Cast<UFGPowerConnectionComponent>(Switch->GetConnection1()))
	{
		if (Conn1->GetNumConnections() > 0)
		{
			Signature |= 0x2;
		}
	}

	return Signature;
}

void FStructuralPowerSwitchProcessor::DisarmDirectedPair(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* SourceBus = Ctx.Graph().FindBusConnector(Switch);
	if (!IsValid(SourceBus))
	{
		return;
	}

	if (UFGPowerConnectionComponent* UnwiredLeg = FindSingleWireSourceBridgeLeg(Switch))
	{
		OpenDirectedBridgeLeg(SourceBus, UnwiredLeg);
	}

	OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
	OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());

	if (UFGStructuralPowerConnectionComponent* ControlBus =
			AStructuralPowerGraphSubsystem::FindSwitchControlBus(Switch))
	{
		FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
		OpenConfiguredBridgeSlots(SourceBus, ControlBus, Switch);
	}
}

void FStructuralPowerSwitchProcessor::SyncDirectedBridgePair(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
	const bool bConfigured = HasAssignedControl(Ctx, Switch);
	if (!bWired && !bConfigured)
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* SourceBus = Ctx.Graph().FindBusConnector(Switch);
	if (!IsValid(SourceBus))
	{
		SourceBus = Ctx.Graph().GetOrCreateBusConnector(Switch);
	}
	if (!IsValid(SourceBus))
	{
		return;
	}

	if (bWired)
	{
		OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
		OpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());
		if (UFGPowerConnectionComponent* SourceBridgeLeg = FindSingleWireSourceBridgeLeg(Switch))
		{
			CloseDirectedBridgeLeg(SourceBus, SourceBridgeLeg);
		}

		if (UFGStructuralPowerConnectionComponent* ControlBus =
				AStructuralPowerGraphSubsystem::FindSwitchControlBus(Switch))
		{
			FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
			OpenConfiguredBridgeSlots(SourceBus, ControlBus, Switch);
		}
	}
	else if (bConfigured)
	{
		UFGStructuralPowerConnectionComponent* ControlBus =
			Ctx.Graph().GetOrCreateSwitchControlBus(Switch);
		if (IsValid(ControlBus))
		{
			AssignConfiguredBridgeSlots(SourceBus, ControlBus, Switch);
		}
	}
}

void FStructuralPowerSwitchProcessor::RemeshUnmeshedPeersAfterBulkLoad(FStructuralPowerContext& Ctx)
{
	AStructuralPowerGraphSubsystem& Graph = Ctx.Graph();
	Graph.RefreshBridgeEndpointRootIndex();

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Graph.TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch();
		if (!IsValid(Switch) || !Pair.Value.ParentId.IsValid())
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* OutletBus = Graph.FindBusConnector(Switch);
		if (!IsValid(OutletBus) || Graph.HasBridgeBusPeerMesh(OutletBus))
		{
			continue;
		}

		const int32 Root = Graph.StructureGraph.FindRoot(Pair.Value.ParentId);
		if (Root == INDEX_NONE)
		{
			continue;
		}

		Graph.TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			Pair.Key,
			/*bBridgePeersOnly=*/true);
	}
}

bool FStructuralPowerSwitchProcessor::HasAssignedControl(
	const FStructuralPowerContext& Ctx,
	const AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return false;
	}

	const FName Control = const_cast<AStructuralPowerGraphSubsystem&>(Ctx.Graph())
		.ResolveControl(const_cast<AFGBuildableCircuitSwitch*>(Switch), EStructuralChannel::Switch);
	return FStructuralPowerRouter::IsAssignedControl(Control);
}

bool FStructuralPowerSwitchProcessor::NeedsAdvancedWork(
	const FStructuralPowerContext& Ctx,
	const AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return false;
	}

	return HasAssignedControl(Ctx, Switch)
		|| FStructuralSwitchParentResolver::HasAnyVanillaWire(
			const_cast<AFGBuildableCircuitSwitch*>(Switch));
}

int32 FStructuralPowerSwitchProcessor::ResolveMountRoot(
	FStructuralPowerContext& Ctx,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	OutParentId = FStructuralNodeId();
	if (!Anchor.IsValid())
	{
		return INDEX_NONE;
	}

	OutParentId = Ctx.Graph().MakeParentNodeId(Anchor);
	const int32 Root = Ctx.Graph().StructureGraph.FindRoot(OutParentId);
	if (Root != INDEX_NONE)
	{
		return Root;
	}

	if (IsBulkLoadAttachContext(Ctx.GetAttachContext()))
	{
		return INDEX_NONE;
	}

	if (Ctx.Graph().EnsureParentRegisteredInGraph(Anchor, OutParentId))
	{
		return Ctx.Graph().StructureGraph.FindRoot(OutParentId);
	}

	return INDEX_NONE;
}

int32 FStructuralPowerSwitchProcessor::ResolveToggleSiteRoot(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	FTrackedEndpoint& Tracked)
{
	FStructuralNodeId ParentId = Tracked.ParentId;
	int32 Root = Ctx.Graph().FindRootForTrackedEndpoint(Tracked);

	const FStructuralOutletParentResolveParams ParentParams = Ctx.Graph().MakeOutletParentResolveParams();
	auto TryResolveFromParent = [&](bool bPreferWirePort) -> int32
	{
		const FStructuralSwitchParentResolveResult ParentResolve =
			FStructuralSwitchParentResolver::Resolve(
				Switch,
				Ctx.Graph().GetWorld(),
				Ctx.Graph().StructureGraph,
				Ctx.Graph().LightweightIndex,
				bPreferWirePort,
				&ParentParams);
		FStructuralNodeId ResolvedParentId;
		const int32 ResolvedRoot = ResolveMountRoot(Ctx, ParentResolve.Anchor, ResolvedParentId);
		if (ResolvedRoot != INDEX_NONE)
		{
			ParentId = ResolvedParentId;
		}
		return ResolvedRoot;
	};

	if (Root == INDEX_NONE)
	{
		Root = TryResolveFromParent(/*bPreferWirePort=*/false);
	}

	if (Root == INDEX_NONE
		&& FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch))
	{
		Root = TryResolveFromParent(/*bPreferWirePort=*/true);
	}

	if (Root == INDEX_NONE
		&& FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch))
	{
		FStructuralSwitchParentResolver::ForEachWiredStructureSideAnchor(
			Switch,
			Ctx.Graph().GetWorld(),
			Ctx.Graph().LightweightIndex,
			&ParentParams,
			[&](const FStructuralWallAnchor& Anchor)
			{
				if (Root != INDEX_NONE)
				{
					return;
				}

				FStructuralNodeId ResolvedParentId;
				const int32 ResolvedRoot = ResolveMountRoot(Ctx, Anchor, ResolvedParentId);
				if (ResolvedRoot != INDEX_NONE)
				{
					Root = ResolvedRoot;
					ParentId = ResolvedParentId;
				}
			});
	}

	if (Root != INDEX_NONE)
	{
		Tracked.ParentId = ParentId;
		Ctx.Graph().MarkBridgeEndpointRootIndexDirty();
	}

	return Root;
}

bool FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(const AFGBuildableCircuitSwitch* Switch)
{
	return IsValid(Switch) && Switch->IsBridgeActive();
}

void FStructuralPowerSwitchProcessor::EnsureListener(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	AStructuralPowerGraphSubsystem& Graph = Ctx.Graph();
	UStructuralPowerSwitchListener* Listener = nullptr;

	TInlineComponentArray<UStructuralPowerSwitchListener*> Listeners;
	Switch->GetComponents(Listeners);
	if (Listeners.Num() > 0)
	{
		Listener = Listeners[0];
	}
	else
	{
		Listener = NewObject<UStructuralPowerSwitchListener>(Switch, NAME_None, RF_Transient);
		if (!Listener)
		{
			return;
		}

		Switch->AddInstanceComponent(Listener);
		Listener->RegisterComponent();
		Listener->BindSubsystem(&Graph, Switch);
		return;
	}

	Listener->SyncSubscriptions(&Graph, Switch);
}

void FStructuralPowerSwitchProcessor::PropagateWiredFeedChange(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	FStructuralPowerBridgeProcessor::PropagateCrossSiteFeedChange(Ctx, Switch, LocalRoot);
}

void FStructuralPowerSwitchProcessor::OnStateChanged(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !Switch->HasAuthority())
	{
		return;
	}

	if (IsBulkLoadAttachContext(Ctx.GetAttachContext()))
	{
		return;
	}

	const FStructuralNodeId SwitchId = Ctx.Graph().MakeNodeId(Switch);
	if (FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(SwitchId))
	{
		FStructuralCircuitPromotionScope PromotionScope(&Ctx.Graph());

		int32 Root = ResolveToggleSiteRoot(Ctx, Switch, *Tracked);

		const FStructuralChannelKey SwitchKey = Ctx.Graph().ResolveChannelKeyForBuildable(Switch);
		const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
		const bool bSwitchOn = Switch->IsBridgeActive();

		Ctx.Graph().MarkEchoDirtyForSwitchToggle(Switch, Root);
		Ctx.Graph().NoteSwitchToggleHandled(Switch);

		FStructuralPowerTrace::LogHook(
			Switch,
			TEXT("OnIsSwitchOnChanged"),
			bSwitchOn ? TEXT("transfer_on") : TEXT("transfer_off"),
			nullptr);

		if (NeedsAdvancedWork(Ctx, Switch))
		{
			FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, bSwitchOn);

			if (Root != INDEX_NONE && bKeyedSubnet)
			{
				FStructuralPowerTransferGate::ApplyKeyedTransferOnRoot(
					Ctx,
					Root,
					SwitchKey.Control,
					bSwitchOn,
					/*bLocalPromoteOnly=*/true);
			}
		}

		return;
	}

	Ctx.Graph().EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}

void FStructuralPowerSwitchProcessor::Process(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("switch.Process"),
		IsValid(Switch) ? Switch->GetName() : TEXT("null"));

	if (!IsValid(Switch))
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_gating_disabled"));
		return;
	}

	AStructuralPowerGraphSubsystem& Graph = Ctx.Graph();
	const bool bBulk = Graph.IsBulkLoadDrainActive();
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const FStructuralNodeId SwitchId = Graph.MakeNodeId(Switch);

	UFGStructuralPowerConnectionComponent* OutletBus = Graph.GetOrCreateBusConnector(Switch);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_bus_create_failed"));
		return;
	}

	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(SwitchId);
	FStructuralSiteContext Site;
	if (!FStructuralSiteMembership::ResolveSiteContext(Graph, Switch, Site))
	{
		const FStructuralOutletParentResolveParams ParentParams = Graph.MakeOutletParentResolveParams();
		const FStructuralSwitchParentResolveResult ParentResolve =
			FStructuralSwitchParentResolver::Resolve(
				Switch,
				Graph.GetWorld(),
				Graph.StructureGraph,
				Graph.LightweightIndex,
				/*bPreferWirePort=*/false,
				&ParentParams);
		Site.ParentAnchor = ParentResolve.Anchor;
		Graph.ResolveEndpointComponentRoot(Switch, Site.ParentAnchor, Site.ParentId);
		Tracked.ParentId = Site.ParentId;
		Site.SiteRoot = Graph.FindRootForTrackedEndpoint(Tracked);
		Site.bAnchored = Site.SiteRoot != INDEX_NONE;
	}

	int32 Root = Site.SiteRoot;

	Tracked.Actor = Switch;
	Tracked.Kind = EStructuralEndpointKind::Switch;
	if (Site.ParentId.IsValid())
	{
		Tracked.ParentId = Site.ParentId;
	}

	const FStructuralOutletParentResolveParams ParentParams = Graph.MakeOutletParentResolveParams();
	const FStructuralSwitchParentResolveResult ParentResolve =
		FStructuralSwitchParentResolver::Resolve(
			Switch,
			Graph.GetWorld(),
			Graph.StructureGraph,
			Graph.LightweightIndex,
			/*bPreferWirePort=*/false,
			&ParentParams);
	const TCHAR* WirePort = ParentResolve.WirePortIndex == 0
		? TEXT("A")
		: (ParentResolve.WirePortIndex == 1 ? TEXT("B") : TEXT("-"));

	const bool bSwitchOn = Switch->IsBridgeActive();
	const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
	const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
	const bool bAdvancedWork = NeedsAdvancedWork(Ctx, Switch);
	const bool bInertPlace = !bWired && !bKeyedSubnet;

	if (bAdvancedWork)
	{
		FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, bSwitchOn);
	}

	FStructuralSiteMembershipParams Params;
	Params.bStripSwitchVanillaPortLinks = bAdvancedWork;
	Params.bBridgePeersOnly = true;
	// DR-017: unwired+unconfigured place = peer mesh only; A/B arms on wire (SyncDirectedBridgePair).
	Params.bLinkVisibleConnections = !bBulk && !bInertPlace;
	Params.bMeshOnlyLinks = bInertPlace && !bBulk;
	Params.bSkipEndpointIndexDirty = !bBulk && Root != INDEX_NONE;
	if (Site.bAnchored && Root != INDEX_NONE
		&& (bBulk || !Graph.HasBridgeBusPeerMesh(OutletBus)))
	{
		FStructuralSiteMembership::IntegrateOnPlace(
			Graph,
			Switch,
			OutletBus,
			SwitchId,
			EStructuralEndpointKind::Switch,
			Tracked,
			Site,
			Params);
	}
	else if (Site.bAnchored && Root != INDEX_NONE)
	{
		Tracked.Actor = Switch;
		Tracked.ParentId = Site.ParentId;
		Tracked.Kind = EStructuralEndpointKind::Switch;
		Graph.RegisterBuildableActor(Switch);
		if (Params.bLinkVisibleConnections)
		{
			Graph.LinkBusToVisibleConnectionsLocal(
				Switch,
				OutletBus,
				Params.bMeshOnlyLinks);
		}
	}
	else if (!Site.bAnchored || Root == INDEX_NONE)
	{
		Tracked.Actor = Switch;
		Tracked.ParentId = Site.ParentId;
		Tracked.Kind = EStructuralEndpointKind::Switch;
		Graph.RegisterBuildableActor(Switch);

		if (!bBulk)
		{
			FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_site_not_ready"));
			Graph.EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
			return;
		}
	}

	if (Site.bAnchored && Root != INDEX_NONE && Site.ParentId.IsValid())
	{
		Graph.AddEndpointToRootIndex(Root, EStructuralEndpointKind::Switch, SwitchId);
	}
	else if (!bBulk)
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	if (bWired || bKeyedSubnet)
	{
		SyncDirectedBridgePair(Ctx, Switch);
	}

	Tracked.CachedSwitchWireSignature = BuildWireSignature(Switch);

	if (bWired && Root != INDEX_NONE)
	{
		PropagateWiredFeedChange(Ctx, Switch, Root);
	}

	const FStructuralChannelKey ChannelKey = Graph.ResolveChannelKeyForBuildable(Switch);

	const TCHAR* Mode = (!bWired && !bKeyedSubnet)
		? TEXT("inert")
		: (bSwitchOn ? AttachContextToString(AttachContext) : TEXT("bridge_idle"));

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] outlet %s scope=%s site=%d role=%s root=%d parentValid=%d busCircuit=%d"
			" powered=%d tag=%s source=%s control=%s wirePort=%s mode=%s"),
		*Switch->GetName(),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Router),
		Root,
		Site.bAnchored ? 1 : 0,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
		StructuralChannelToString(ChannelKey.Tag),
		*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
		*FStructuralPowerTrace::FormatControlForTrace(ChannelKey),
		WirePort,
		Mode);

	if (bBulk)
	{
		return;
	}

	if (bSwitchOn && Root != INDEX_NONE && bKeyedSubnet && !bWired)
	{
		FStructuralPowerTransferGate::ApplyKeyedTransferOnRoot(
			Ctx,
			Root,
			ChannelKey.Control,
			/*bGateOpen=*/true,
			/*bLocalPromoteOnly=*/true);
	}

	EnsureListener(Ctx, Switch);
}
