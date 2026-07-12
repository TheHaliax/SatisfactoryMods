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
#include "Processors/FStructuralSwitchBridgeStrategy.h"
#include "Processors/FStructuralSwitchWireEcho.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Core/FStructuralGraphSession.h"
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

	UFGStructuralPowerConnectionComponent* Bus = Ctx.Session().FindBusConnector(Switch);
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
	return FStructuralSwitchWireEcho::BuildWireSignature(Switch);
}

void FStructuralPowerSwitchProcessor::DisarmDirectedPair(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* SourceBus = Ctx.Session().FindBusConnector(Switch);
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

	UFGStructuralPowerConnectionComponent* SourceBus = Ctx.Session().FindBusConnector(Switch);
	if (!IsValid(SourceBus))
	{
		SourceBus = Ctx.Session().GetOrCreateBusConnector(Switch);
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
			Ctx.Session().GetOrCreateSwitchControlBus(Switch);
		if (IsValid(ControlBus))
		{
			AssignConfiguredBridgeSlots(SourceBus, ControlBus, Switch);
		}
	}
}

void FStructuralPowerSwitchProcessor::ApplySwitchBridgeStrategy(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralSwitchBridgeStrategy::ApplyMembership(Ctx, Switch);
}

void FStructuralPowerSwitchProcessor::OnCircuitsRebuilt(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralSwitchWireEcho::OnCircuitsRebuilt(Ctx, Switch);
}

void FStructuralPowerSwitchProcessor::RemeshUnmeshedBridgesAfterBulkLoad(FStructuralPowerContext& Ctx)
{
	FStructuralGraphSession& Session = Ctx.Session();
	Session.RefreshBridgeEndpointRootIndex();

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session.TrackedEndpoints())
	{
		const EStructuralEndpointKind Kind = Pair.Value.Kind;
		if (Kind != EStructuralEndpointKind::Pole
			&& Kind != EStructuralEndpointKind::Switch
			&& Kind != EStructuralEndpointKind::Storage)
		{
			continue;
		}

		if (Kind == EStructuralEndpointKind::Pole && !Pair.Value.bStructuralPowerTransferActive)
		{
			continue;
		}

		AFGBuildable* Host = Pair.Value.Actor.Get();
		if (!IsValid(Host) || !Pair.Value.ParentId.IsValid())
		{
			continue;
		}

		const int32 Root = Session.StructureGraph().FindRoot(Pair.Value.ParentId);
		if (Root == INDEX_NONE)
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Host);
		if (!IsValid(OutletBus))
		{
			continue;
		}

		if (Kind != EStructuralEndpointKind::Switch)
		{
			Session.LinkBusToVisibleConnectionsLocal(Host, OutletBus, /*bMeshOnlyLinks=*/false);
		}

		if (!Session.HasBridgeBusPeerMesh(OutletBus))
		{
			Session.TryMeshPeerBusOnComponent(
				Host,
				OutletBus,
				Root,
				Pair.Key,
				/*bBridgePeersOnly=*/true);
		}

		if (Kind != EStructuralEndpointKind::Switch)
		{
			Session.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/false);
		}
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

	const FName Control = const_cast<FStructuralPowerContext&>(Ctx).Session().ResolveControl(
		const_cast<AFGBuildableCircuitSwitch*>(Switch),
		EStructuralChannel::Switch);
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

	OutParentId = Ctx.Session().MakeParentNodeId(Anchor);
	const int32 Root = Ctx.Session().StructureGraph().FindRoot(OutParentId);
	if (Root != INDEX_NONE)
	{
		return Root;
	}

	if (IsBulkLoadAttachContext(Ctx.GetAttachContext()))
	{
		return INDEX_NONE;
	}

	if (Ctx.Session().EnsureParentRegisteredInGraph(Anchor, OutParentId))
	{
		return Ctx.Session().StructureGraph().FindRoot(OutParentId);
	}

	return INDEX_NONE;
}

int32 FStructuralPowerSwitchProcessor::ResolveToggleSiteRoot(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	FTrackedEndpoint& Tracked)
{
	FStructuralNodeId ParentId = Tracked.ParentId;
	int32 Root = Ctx.Session().FindRootForTrackedEndpoint(Tracked);

	const FStructuralOutletParentResolveParams ParentParams = Ctx.Session().MakeOutletParentResolveParams();
	auto TryResolveFromParent = [&](bool bPreferWirePort) -> int32
	{
		const FStructuralSwitchParentResolveResult ParentResolve =
			FStructuralSwitchParentResolver::Resolve(
				Switch,
				Ctx.Session().GetWorld(),
				Ctx.Session().StructureGraph(),
				Ctx.Session().LightweightIndex(),
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
			Ctx.Session().GetWorld(),
			Ctx.Session().LightweightIndex(),
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
		Ctx.Session().MarkBridgeEndpointRootIndexDirty();
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

	FStructuralGraphSession& Session = Ctx.Session();
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
		Listener->BindSubsystem(&Session.Owner(), Switch);
		return;
	}

	Listener->SyncSubscriptions(&Session.Owner(), Switch);
}

void FStructuralPowerSwitchProcessor::PropagateWiredFeedChange(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	FStructuralPowerBridgeProcessor::PropagateCrossSiteFeedChange(Ctx, Switch, LocalRoot);
}

void FStructuralPowerSwitchProcessor::ApplyWireDeltaTransferSideEffects(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	int32 Root)
{
	if (!IsValid(Switch) || Root == INDEX_NONE)
	{
		return;
	}

	const bool bAdvancedWork = NeedsAdvancedWork(Ctx, Switch);
	const bool bSwitchOn = Switch->IsBridgeActive();
	const bool bGateOpen = bAdvancedWork && bSwitchOn;

	if (bAdvancedWork)
	{
		FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, bSwitchOn);

		if (HasAssignedControl(Ctx, Switch))
		{
			const FStructuralChannelKey SwitchKey = Ctx.Session().ResolveChannelKeyForBuildable(Switch);
			FStructuralPowerTransferGate::ApplyKeyedTransferOnRoot(
				Ctx,
				Root,
				SwitchKey.Control,
				bSwitchOn,
				/*bLocalPromoteOnly=*/true);
		}
	}
	else
	{
		FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, false);
	}

	if (!HasAssignedControl(Ctx, Switch))
	{
		FStructuralPowerTransferGate::RefreshSiteStructuralConsumersOnRoot(Ctx, Root, bGateOpen);
	}
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

	const FStructuralNodeId SwitchId = Ctx.Session().MakeNodeId(Switch);
	if (FTrackedEndpoint* Tracked = Ctx.Session().TrackedEndpoints().Find(SwitchId))
	{
		FStructuralCircuitPromotionScope PromotionScope(&Ctx.Session().Owner());

		int32 Root = ResolveToggleSiteRoot(Ctx, Switch, *Tracked);

		const FStructuralChannelKey SwitchKey = Ctx.Session().ResolveChannelKeyForBuildable(Switch);
		const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
		const bool bSwitchOn = Switch->IsBridgeActive();

		Ctx.Session().MarkEchoDirtyForSwitchToggle(Switch, Root);
		Ctx.Session().NoteSwitchToggleHandled(Switch);

		FStructuralSwitchBridgeStrategy::ApplyToggle(Ctx, Switch);

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

			if (Root != INDEX_NONE && !bKeyedSubnet)
			{
				FStructuralPowerTransferGate::RefreshSiteStructuralConsumersOnRoot(
					Ctx,
					Root,
					bSwitchOn);
			}
		}

		return;
	}

	Ctx.Session().EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}

void FStructuralPowerSwitchProcessor::ApplyStructureMembership(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	FStructuralGraphSession& Session = Ctx.Session();
	const bool bBulk = Session.IsBulkLoadDrainActive();
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const bool bFactRefresh = AttachContext == EAttachContext::WireDelta;
	const FStructuralNodeId SwitchId = Session.MakeNodeId(Switch);

	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(SwitchId);
	FStructuralSiteContext Site;
	if (!FStructuralSiteMembership::ResolveSiteContext(Session, Switch, Site))
	{
		const FStructuralOutletParentResolveParams ParentParams = Session.MakeOutletParentResolveParams();
		const FStructuralSwitchParentResolveResult ParentResolve =
			FStructuralSwitchParentResolver::Resolve(
				Switch,
				Session.GetWorld(),
				Session.StructureGraph(),
				Session.LightweightIndex(),
				/*bPreferWirePort=*/false,
				&ParentParams);
		Site.ParentAnchor = ParentResolve.Anchor;
		Session.ResolveEndpointComponentRoot(Switch, Site.ParentAnchor, Site.ParentId);
		Tracked.ParentId = Site.ParentId;
		Site.SiteRoot = Session.FindRootForTrackedEndpoint(Tracked);
		Site.bAnchored = Site.SiteRoot != INDEX_NONE;
	}

	const int32 Root = Site.SiteRoot;

	Tracked.Actor = Switch;
	Tracked.Kind = EStructuralEndpointKind::Switch;
	if (Site.ParentId.IsValid())
	{
		Tracked.ParentId = Site.ParentId;
	}

	const bool bSwitchOn = Switch->IsBridgeActive();
	const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
	const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
	const bool bAdvancedWork = NeedsAdvancedWork(Ctx, Switch);

	if (bBulk)
	{
		if (bAdvancedWork)
		{
			FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, bSwitchOn);
		}

		if (Site.bAnchored && Root != INDEX_NONE)
		{
			FStructuralSiteMembership::RegisterOnBulkLoad(
				Session,
				Switch,
				EStructuralEndpointKind::Switch,
				Tracked,
				Site);
			if (Site.ParentId.IsValid())
			{
				Session.AddEndpointToRootIndex(Root, EStructuralEndpointKind::Switch, SwitchId);
			}
		}
		else
		{
			Tracked.bAwaitingStructuralSite = true;
			Session.RegisterBuildableActor(Switch);
		}

		Tracked.CachedSwitchWireSignature = BuildWireSignature(Switch);
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Switch);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(
			Switch,
			TEXT("switch_bus_create_failed"),
			ELogVerbosity::Warning);
		return;
	}

	const bool bInertPlace = !bWired && !bKeyedSubnet;
	const bool bBypassOnStructure = bInertPlace && bSwitchOn;

	if (bAdvancedWork)
	{
		FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, bSwitchOn);
	}

	FStructuralSiteMembershipParams Params;
	Params.bStripSwitchVanillaPortLinks = bAdvancedWork || bFactRefresh;
	Params.bBridgePeersOnly = true;
	Params.bLinkVisibleConnections = bWired || bKeyedSubnet || bBypassOnStructure;
	Params.bMeshOnlyLinks = bInertPlace;
	Params.bSkipEndpointIndexDirty = !bBulk && Root != INDEX_NONE;
	if (Site.bAnchored && Root != INDEX_NONE
		&& (bBulk || bFactRefresh || !Session.HasBridgeBusPeerMesh(OutletBus)))
	{
		FStructuralSiteMembership::IntegrateOnPlace(Session,
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
		Session.RegisterBuildableActor(Switch);
		if (Params.bLinkVisibleConnections)
		{
			Session.LinkBusToVisibleConnectionsLocal(
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
		Tracked.bAwaitingStructuralSite = true;
		Session.RegisterBuildableActor(Switch);

		if (!bBulk
			&& AttachContext != EAttachContext::WireDelta
			&& AttachContext != EAttachContext::Toggle
			&& !Session.IsBuildablePlacementPending(Switch))
		{
			FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_site_not_ready"));
			Session.EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		}
		return;
	}

	if (Site.bAnchored && Root != INDEX_NONE && Site.ParentId.IsValid())
	{
		Session.AddEndpointToRootIndex(Root, EStructuralEndpointKind::Switch, SwitchId);
	}
	else if (!bBulk)
	{
		Session.MarkBridgeEndpointRootIndexDirty();
	}

	const bool bMeshed = Session.HasBridgeBusPeerMesh(OutletBus);
	const bool bSkipMembershipStrategy =
		AttachContext == EAttachContext::Toggle
		|| (AttachContext == EAttachContext::WireDelta && bMeshed);

	if (!bSkipMembershipStrategy)
	{
		FStructuralSwitchBridgeStrategy::ApplyMembership(Ctx, Switch);
	}

	Tracked.CachedSwitchWireSignature = BuildWireSignature(Switch);
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

	FStructuralGraphSession& Session = Ctx.Session();
	const bool bBulk = Session.IsBulkLoadDrainActive();
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const FStructuralNodeId SwitchId = Session.MakeNodeId(Switch);

	ApplyStructureMembership(Ctx, Switch);

	UFGStructuralPowerConnectionComponent* OutletBus = Session.FindBusConnector(Switch);
	if (!OutletBus)
	{
		return;
	}

	FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(SwitchId);
	if (!Tracked)
	{
		return;
	}

	int32 Root = INDEX_NONE;
	if (Tracked->ParentId.IsValid())
	{
		Root = Session.StructureGraph().FindRoot(Tracked->ParentId);
	}

	const FStructuralOutletParentResolveParams ParentParams = Session.MakeOutletParentResolveParams();
	const FStructuralSwitchParentResolveResult ParentResolve =
		FStructuralSwitchParentResolver::Resolve(
			Switch,
			Session.GetWorld(),
			Session.StructureGraph(),
			Session.LightweightIndex(),
			/*bPreferWirePort=*/false,
			&ParentParams);
	const TCHAR* WirePort = ParentResolve.WirePortIndex == 0
		? TEXT("A")
		: (ParentResolve.WirePortIndex == 1 ? TEXT("B") : TEXT("-"));

	const bool bSwitchOn = Switch->IsBridgeActive();
	const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
	const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);

	if (bWired && Root != INDEX_NONE && !IsBulkLoadAttachContext(AttachContext))
	{
		PropagateWiredFeedChange(Ctx, Switch, Root);
	}

	const FStructuralChannelKey ChannelKey = Session.ResolveChannelKeyForBuildable(Switch);

	const TCHAR* Mode = (!bWired && !bKeyedSubnet)
		? TEXT("inert")
		: (bSwitchOn ? AttachContextToString(AttachContext) : TEXT("bridge_idle"));

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] outlet %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d busCircuit=%d"
			" powered=%d tag=%s source=%s control=%s wirePort=%s mode=%s"),
		*Switch->GetName(),
		StructuralEndpointKindToString(EStructuralEndpointKind::Switch),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Router),
		Root,
		Root != INDEX_NONE ? 1 : 0,
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

bool FStructuralPowerSwitchProcessor::ResolveSwitchTrackedRoot(
	FStructuralGraphSession& Session,
	AFGBuildableCircuitSwitch* Switch,
	FStructuralNodeId& OutSwitchId,
	int32& OutRoot,
	bool& OutStructurallyAnchored)
{
	OutStructurallyAnchored = false;
	if (!IsValid(Switch))
	{
		return false;
	}

	OutSwitchId = Session.MakeNodeId(Switch);
	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(OutSwitchId);
	Tracked.Actor = Switch;
	Tracked.Kind = EStructuralEndpointKind::Switch;
	Session.RegisterBuildableActor(Switch);

	if (Tracked.ParentId.IsValid())
	{
		OutRoot = Session.FindRootForTrackedEndpoint(Tracked);
		if (OutRoot != INDEX_NONE)
		{
			OutStructurallyAnchored = true;
			return true;
		}
	}

	FStructuralSiteContext Site;
	if (FStructuralSiteMembership::ResolveSiteContext(Session, Switch, Site))
	{
		OutRoot = Site.SiteRoot;
		OutStructurallyAnchored = Site.bAnchored;
		Tracked.ParentId = Site.ParentId;
		if (OutRoot != INDEX_NONE)
		{
			Session.MarkBridgeEndpointRootIndexDirty();
		}
		return OutRoot != INDEX_NONE;
	}

	OutStructurallyAnchored = false;
	return false;
}

void FStructuralPowerSwitchProcessor::OnWireDelta(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || Ctx.GetAttachContext() == EAttachContext::Toggle)
	{
		return;
	}

	FStructuralNodeId SwitchId;
	int32 Root = INDEX_NONE;
	bool bStructurallyAnchored = false;
	FStructuralPowerSwitchProcessor::ResolveSwitchTrackedRoot(Ctx.Session(), Switch, SwitchId, Root, bStructurallyAnchored);

	FStructuralPowerSwitchProcessor::ApplyStructureMembership(Ctx, Switch);

	if (Ctx.GetAttachContext() == EAttachContext::WireDelta)
	{
		FStructuralPowerSwitchProcessor::ApplyWireDeltaTransferSideEffects(Ctx, Switch, Root);
	}

	const bool bHasWire = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
	const bool bSwitchOn = Switch->IsBridgeActive();

	const FTrackedEndpoint* Tracked = Ctx.Session().TrackedEndpoints().Find(SwitchId);
	const bool bGateOpen = Tracked && Tracked->bStructuralPowerTransferActive;

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(Ctx.Session().GetOrCreateBusConnector(Switch));
	if (!OutletBus)
	{
		return;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] switch wire delta %s kind=%s scope=%s site=%d role=%s attach=%s"
			" root=%d busCircuit=%d powered=%d wired=%d transfer=%d gate=%d"),
		*Switch->GetName(),
		StructuralEndpointKindToString(EStructuralEndpointKind::Switch),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
		AttachContextToString(Ctx.GetAttachContext()),
		Root,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
		bHasWire ? 1 : 0,
		bSwitchOn ? 1 : 0,
		bGateOpen ? 1 : 0);
}
