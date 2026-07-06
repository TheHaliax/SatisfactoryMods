// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerSwitchProcessor.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/EAttachContext.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralCrossSiteGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Network/UStructuralPowerSwitchListener.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPowerSwitchProcessor::TearDown(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildable* Host)
{
	UFGStructuralPowerConnectionComponent* Bus = Graph.FindBusConnector(Host);
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

void FStructuralPowerSwitchProcessor::StripInactiveLinksOnRoot(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Root)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Graph.TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch
			|| Graph.StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch();
		if (!IsValid(Switch) || Switch->IsBridgeActive())
		{
			continue;
		}

		TearDown(Graph, Switch);
	}
}

bool FStructuralPowerSwitchProcessor::HasAssignedControl(
	const AStructuralPowerGraphSubsystem& Graph,
	const AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return false;
	}

	const FName Control = const_cast<AStructuralPowerGraphSubsystem&>(Graph)
		.ResolveControl(const_cast<AFGBuildableCircuitSwitch*>(Switch), EStructuralChannel::Switch);
	return Control != StructuralPowerConstants::ControlBypass && !Control.IsNone();
}

bool FStructuralPowerSwitchProcessor::NeedsAdvancedWork(
	const AStructuralPowerGraphSubsystem& Graph,
	const AFGBuildableCircuitSwitch* Switch)
{
	return HasAssignedControl(Graph, Switch)
		|| FStructuralSwitchParentResolver::IsWiredToStructureSide(
			const_cast<AFGBuildableCircuitSwitch*>(Switch));
}

int32 FStructuralPowerSwitchProcessor::ResolveMountRoot(
	AStructuralPowerGraphSubsystem& Graph,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	OutParentId = FStructuralNodeId();
	if (!Anchor.IsValid())
	{
		return INDEX_NONE;
	}

	OutParentId = Graph.MakeParentNodeId(Anchor);
	const int32 Root = Graph.StructureGraph.FindRoot(OutParentId);
	if (Root != INDEX_NONE)
	{
		return Root;
	}

	if (IsBulkLoadAttachContext(Graph.GetCurrentAttachContext()))
	{
		return INDEX_NONE;
	}

	if (Graph.EnsureParentRegisteredInGraph(Anchor, OutParentId))
	{
		return Graph.StructureGraph.FindRoot(OutParentId);
	}

	return INDEX_NONE;
}

bool FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(const AFGBuildableCircuitSwitch* Switch)
{
	return IsValid(Switch) && Switch->IsBridgeActive();
}

void FStructuralPowerSwitchProcessor::EnsureListener(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	TInlineComponentArray<UStructuralPowerSwitchListener*> Listeners;
	Switch->GetComponents(Listeners);
	if (Listeners.Num() > 0)
	{
		return;
	}

	UStructuralPowerSwitchListener* Listener =
		NewObject<UStructuralPowerSwitchListener>(Switch, NAME_None, RF_Transient);
	if (!Listener)
	{
		return;
	}

	Switch->AddInstanceComponent(Listener);
	Listener->RegisterComponent();
	Listener->BindSubsystem(&Graph, Switch);
}

void FStructuralPowerSwitchProcessor::RestitchKeyedSubnet(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 ComponentRoot,
	const FStructuralNodeId& SwitchNodeId)
{
	if (!IsValid(Switch) || !IsValid(OutletBus) || ComponentRoot == INDEX_NONE)
	{
		return;
	}

	if (UFGCircuitConnectionComponent* Feed = Graph.GetComponentSourceConnector(ComponentRoot, Switch))
	{
		if (UFGPowerConnectionComponent* FeedPower = Cast<UFGPowerConnectionComponent>(Feed))
		{
			Graph.LinkHiddenPair(OutletBus, FeedPower);
		}
	}

	Graph.EndpointIndex.ForEachBridgeOnRoot(ComponentRoot, [&](const FStructuralNodeId& PeerId)
	{
		if (PeerId == SwitchNodeId)
		{
			return;
		}

		const FTrackedEndpoint* PeerTracked = Graph.TrackedEndpoints.Find(PeerId);
		if (!PeerTracked)
		{
			return;
		}

		AFGBuildable* SiblingHost = PeerTracked->Actor.Get();
		if (!IsValid(SiblingHost))
		{
			return;
		}

		if (!Graph.ShouldMeshEndpoints(Switch, SiblingHost, ComponentRoot))
		{
			return;
		}

		if (UFGStructuralPowerConnectionComponent* SiblingBus = Graph.FindBusConnector(SiblingHost))
		{
			Graph.LinkHiddenPair(OutletBus, SiblingBus);
		}
	});

	UFGStructuralPowerConnectionComponent* Seed = FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus)
		? OutletBus
		: Graph.FindPoweredHiddenReachable(OutletBus);
	if (Seed)
	{
		Graph.PromoteDirectHiddenLinks(Seed);
	}
}

void FStructuralPowerSwitchProcessor::ApplyBaseOutletAttach(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root)
{
	if (!IsValid(Switch) || !IsValid(OutletBus) || Root == INDEX_NONE)
	{
		return;
	}

	Graph.LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (!Graph.DoesComponentRootCarryPower(Root))
	{
		return;
	}

	if (UFGCircuitConnectionComponent* Feed = Graph.GetComponentSourceConnector(Root, Switch))
	{
		if (UFGPowerConnectionComponent* FeedPower = Cast<UFGPowerConnectionComponent>(Feed))
		{
			Graph.LinkHiddenPairLocal(OutletBus, FeedPower);
		}
	}

	Graph.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void FStructuralPowerSwitchProcessor::ApplyAdvancedAttach(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SwitchId,
	bool bKeyedSubnet)
{
	if (!IsValid(Switch) || !IsValid(OutletBus))
	{
		return;
	}

	Graph.LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (Root == INDEX_NONE)
	{
		Graph.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
		return;
	}

	UFGPowerConnectionComponent* FeedPower = nullptr;
	if (UFGCircuitConnectionComponent* Conn0 = Switch->GetConnection0())
	{
		if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn0))
		{
			if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
			{
				FeedPower = Visible;
			}
		}
	}
	if (!FeedPower)
	{
		if (UFGCircuitConnectionComponent* Conn1 = Switch->GetConnection1())
		{
			if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
			{
				if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
				{
					FeedPower = Visible;
				}
			}
		}
	}
	if (!FeedPower)
	{
		if (UFGCircuitConnectionComponent* Feed = Graph.GetComponentSourceConnector(Root, Switch))
		{
			FeedPower = Cast<UFGPowerConnectionComponent>(Feed);
		}
	}

	if (FeedPower)
	{
		Graph.LinkHiddenPairLocal(OutletBus, FeedPower);
	}

	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	if (bKeyedSubnet && !Graph.HasBridgeBusPeerMesh(OutletBus))
	{
		Graph.TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			SwitchId,
			/*bBridgePeersOnly=*/false);
	}
	else if (bWiredBridge && !Graph.HasBridgeBusPeerMesh(OutletBus))
	{
		Graph.TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			SwitchId,
			/*bBridgePeersOnly=*/true);
	}

	Graph.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void FStructuralPowerSwitchProcessor::ApplyRuntimeAttach(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SwitchId,
	EAttachContext AttachContext)
{
	if (!IsValid(Switch) || !IsValid(OutletBus))
	{
		return;
	}

	if (IsBulkLoadAttachContext(AttachContext))
	{
		Graph.LinkBusToVisibleConnections(Switch, OutletBus);
		if (Root != INDEX_NONE)
		{
			Graph.ApplyLocalBridgeBusAttach(Switch, OutletBus, Root, SwitchId, Switch);
		}
		else
		{
			Graph.PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/false);
		}
		return;
	}

	const bool bKeyedSubnet = HasAssignedControl(Graph, Switch);
	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	if (bKeyedSubnet || bWiredBridge)
	{
		ApplyAdvancedAttach(Graph, Switch, OutletBus, Root, SwitchId, bKeyedSubnet);
	}
	else
	{
		ApplyBaseOutletAttach(Graph, Switch, OutletBus, Root);
	}
}

void FStructuralPowerSwitchProcessor::RegisterOutletBase(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	const FStructuralWallAnchor& ParentAnchor,
	FTrackedEndpoint& InOutTracked,
	int32& OutRoot,
	FStructuralNodeId& OutParentId)
{
	OutRoot = ResolveMountRoot(Graph, ParentAnchor, OutParentId);
	InOutTracked.Actor = Switch;
	InOutTracked.ParentId = OutParentId;
	InOutTracked.Kind = EStructuralEndpointKind::Switch;
	Graph.RegisterBuildableActor(Switch);
	if (OutParentId.IsValid() && OutRoot != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(Graph.GetCurrentAttachContext()))
		{
			Graph.AddEndpointToRootIndex(OutRoot, EStructuralEndpointKind::Switch, Graph.MakeNodeId(Switch));
		}
		else
		{
			Graph.MarkBridgeEndpointRootIndexDirty();
		}
	}
	EnsureListener(Graph, Switch);
}

void FStructuralPowerSwitchProcessor::LogConsumerRestitchSummary(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	int32 Root,
	FName SwitchControlId,
	bool bSwitchOn)
{
	if (Root == INDEX_NONE || SwitchControlId.IsNone())
	{
		return;
	}

	int32 PanelCount = 0;
	int32 DirectLightCount = 0;
	int32 LitCount = 0;

	auto CountMatchingPanel = [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildableLightsControlPanel* Panel =
			Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get());
		if (!IsValid(Panel))
		{
			return;
		}

		if (Graph.ResolveSource(Panel, EStructuralChannel::Light) != SwitchControlId)
		{
			return;
		}

		++PanelCount;
		if (!bSwitchOn)
		{
			return;
		}

		FStructuralPanelPorts Ports;
		if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
		{
			return;
		}

		const UFGPowerConnectionComponent* InputPower =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
		if (!FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower))
		{
			return;
		}

		LitCount += Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();
	};

	auto CountMatchingLight = [&](const FStructuralNodeId& NodeId)
	{
		if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			return;
		}

		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Tracked->Actor.Get());
		if (!IsValid(Light))
		{
			return;
		}

		const FStructuralChannelKey LightKey = Graph.ResolveChannelKeyForBuildable(Light);
		if (LightKey.Source != SwitchControlId)
		{
			return;
		}

		++DirectLightCount;
		if (bSwitchOn)
		{
			if (UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light))
			{
				if (FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(Plug))
				{
					++LitCount;
				}
			}
		}
	};

	if (const TArray<FStructuralNodeId>* PanelIds =
			Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
	{
		for (const FStructuralNodeId& NodeId : *PanelIds)
		{
			CountMatchingPanel(NodeId);
		}
	}

	if (const TArray<FStructuralNodeId>* LightIds =
			Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Light))
	{
		for (const FStructuralNodeId& NodeId : *LightIds)
		{
			CountMatchingLight(NodeId);
		}
	}

	FStructuralChannelKey SwitchKey;
	SwitchKey.Tag = EStructuralChannel::Switch;
	SwitchKey.Control = SwitchControlId;

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] switch restitch_%s %s scope=site site=%d role=router root=%d control=%s"
			" panels=%d direct_lights=%d lit=%d"),
		bSwitchOn ? TEXT("on") : TEXT("off"),
		IsValid(Switch) ? *Switch->GetName() : TEXT("null"),
		Root,
		Root,
		*FStructuralPowerTrace::FormatControlForTrace(SwitchKey),
		PanelCount,
		DirectLightCount,
		LitCount);
}

void FStructuralPowerSwitchProcessor::RestitchKeyedConsumersOnRoot(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Root,
	FName SwitchControlId,
	bool bLocalPromoteOnly)
{
	if (Root == INDEX_NONE
		|| SwitchControlId.IsNone()
		|| SwitchControlId == StructuralPowerConstants::ControlBypass)
	{
		return;
	}

	Graph.RefreshBridgeEndpointRootIndex();

	auto RestitchMatchingPanel = [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host))
		{
			return;
		}

		if (Graph.ResolveSource(Host, EStructuralChannel::Light) != SwitchControlId)
		{
			return;
		}

		if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Host))
		{
			FTrackedEndpoint& Mutable = Graph.TrackedEndpoints.FindOrAdd(NodeId);
			Mutable.bPanelLinksReady = false;
			Mutable.bDownstreamLinksReady = false;
			FStructuralPowerPanelProcessor::Process(Graph, Panel, bLocalPromoteOnly);
		}
	};

	auto RestitchMatchingLight = [&](const FStructuralNodeId& NodeId)
	{
		if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			return;
		}

		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host))
		{
			return;
		}

		if (Graph.ResolveSource(Host, EStructuralChannel::Light) != SwitchControlId)
		{
			return;
		}

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Host))
		{
			FStructuralPowerLightProcessor::Process(Graph, Light, bLocalPromoteOnly);
		}
	};

	if (const TArray<FStructuralNodeId>* PanelIds =
			Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
	{
		const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
		for (const FStructuralNodeId& NodeId : PanelIdsSnapshot)
		{
			RestitchMatchingPanel(NodeId);
		}
	}

	if (const TArray<FStructuralNodeId>* LightIds =
			Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Light))
	{
		const TArray<FStructuralNodeId> LightIdsSnapshot = *LightIds;
		for (const FStructuralNodeId& NodeId : LightIdsSnapshot)
		{
			RestitchMatchingLight(NodeId);
		}
	}
}

void FStructuralPowerSwitchProcessor::RestitchActiveKeyedConsumersOnRoot(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Root)
{
	if (Root == INDEX_NONE
		|| !FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		return;
	}

	Graph.RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* SwitchIds =
		Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Switch);
	if (!SwitchIds || SwitchIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> SwitchIdsSnapshot = *SwitchIds;
	for (const FStructuralNodeId& NodeId : SwitchIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* KeyedSwitch =
			Cast<AFGBuildableCircuitSwitch>(Tracked->Actor.Get());
		if (!IsValid(KeyedSwitch)
			|| !KeyedSwitch->IsBridgeActive()
			|| !HasAssignedControl(Graph, KeyedSwitch))
		{
			continue;
		}

		const FName SwitchControl = Graph.ResolveControl(KeyedSwitch, EStructuralChannel::Switch);
		RestitchKeyedConsumersOnRoot(Graph, Root, SwitchControl, /*bLocalPromoteOnly=*/true);
	}
}

void FStructuralPowerSwitchProcessor::PropagateWiredFeedChange(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	if (!IsValid(Switch) || LocalRoot == INDEX_NONE)
	{
		return;
	}

	Graph.CrossSiteGraph.RefreshCouplingsFromWiredSwitch(Graph, Switch, LocalRoot);

	TArray<int32> AffectedRoots;
	Graph.CrossSiteGraph.TraceFeedAffected(Graph, Switch, LocalRoot, AffectedRoots);
	if (AffectedRoots.Num() == 0)
	{
		return;
	}

	Graph.ReEnergizeComponentRoots(
		AffectedRoots,
		/*bTearDownFirst=*/false,
		EAttachContext::WireDelta);

	for (int32 AffectedRoot : AffectedRoots)
	{
		RestitchActiveKeyedConsumersOnRoot(Graph, AffectedRoot);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] wired switch %s scope=cross_site site=%d role=gateway path=wire_bridge"
			" localRoot=%d feedAffected=%d"),
		*Switch->GetName(),
		LocalRoot,
		LocalRoot,
		AffectedRoots.Num());
}

void FStructuralPowerSwitchProcessor::OnStateChanged(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !Switch->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerSessionSettings::IsPropagationEnabled()
		|| !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	if (IsBulkLoadAttachContext(Graph.GetCurrentAttachContext()))
	{
		return;
	}

	const FStructuralNodeId SwitchId = Graph.MakeNodeId(Switch);
	if (FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(SwitchId))
	{
		FStructuralCircuitPromotionScope PromotionScope(&Graph);

		FStructuralNodeId ParentId = Tracked->ParentId;
		int32 Root = Graph.FindRootForTrackedEndpoint(*Tracked);
		if (Root == INDEX_NONE)
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
			Root = ResolveMountRoot(Graph, ParentResolve.Anchor, ParentId);
			if (ParentResolve.IsValid())
			{
				Tracked->ParentId = ParentId;
				Graph.MarkBridgeEndpointRootIndexDirty();
			}
		}

		const FStructuralChannelKey SwitchKey = Graph.ResolveChannelKeyForBuildable(Switch);
		const bool bKeyedSubnet = HasAssignedControl(Graph, Switch);
		const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
		const bool bSwitchOn = Switch->IsBridgeActive();

		FStructuralPowerTrace::LogHook(
			Switch,
			TEXT("OnIsSwitchOnChanged"),
			bSwitchOn ? TEXT("restitch_on") : TEXT("restitch_off"),
			nullptr);

		if (!bSwitchOn)
		{
			TearDown(Graph, Switch);
			if (Root != INDEX_NONE
				&& bKeyedSubnet
				&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
			{
				RestitchKeyedConsumersOnRoot(
					Graph,
					Root,
					SwitchKey.Control,
					/*bLocalPromoteOnly=*/true);
			}
			if (bWiredBridge)
			{
				PropagateWiredFeedChange(Graph, Switch, Root);
			}
		}
		else
		{
			UFGStructuralPowerConnectionComponent* OutletBus = Graph.GetOrCreateBusConnector(Switch);
			if (OutletBus)
			{
				ApplyRuntimeAttach(
					Graph,
					Switch,
					OutletBus,
					Root,
					SwitchId,
					EAttachContext::Toggle);
				if (Root != INDEX_NONE
					&& bKeyedSubnet
					&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
				{
					RestitchKeyedConsumersOnRoot(
						Graph,
						Root,
						SwitchKey.Control,
						/*bLocalPromoteOnly=*/true);
				}
				if (bWiredBridge)
				{
					PropagateWiredFeedChange(Graph, Switch, Root);
				}
			}
		}

		if (Root != INDEX_NONE
			&& FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			FStructuralPowerPanelProcessor::RestitchOnRoot(Graph, Root, EAttachContext::Toggle);
		}

		if (Root != INDEX_NONE
			&& bKeyedSubnet
			&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
		{
			LogConsumerRestitchSummary(Graph, Switch, Root, SwitchKey.Control, bSwitchOn);
		}

		return;
	}

	Graph.EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}

void FStructuralPowerSwitchProcessor::Process(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_gating_disabled"));
		return;
	}

	const EAttachContext AttachContext = Graph.GetCurrentAttachContext();
	const FStructuralOutletParentResolveParams ParentParams = Graph.MakeOutletParentResolveParams();
	const FStructuralSwitchParentResolveResult ParentResolve =
		FStructuralSwitchParentResolver::Resolve(
			Switch,
			Graph.GetWorld(),
			Graph.StructureGraph,
			Graph.LightweightIndex,
			/*bPreferWirePort=*/false,
			&ParentParams);
	const FStructuralWallAnchor ParentAnchor = ParentResolve.Anchor;

	const FStructuralNodeId SwitchId = Graph.MakeNodeId(Switch);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(SwitchId);
	FStructuralNodeId ParentId;
	int32 Root = INDEX_NONE;
	RegisterOutletBase(Graph, Switch, ParentResolve.Anchor, Tracked, Root, ParentId);

	const FStructuralChannelKey ChannelKey = Graph.ResolveChannelKeyForBuildable(Switch);
	const TCHAR* WirePort = ParentResolve.WirePortIndex == 0
		? TEXT("A")
		: (ParentResolve.WirePortIndex == 1 ? TEXT("B") : TEXT("-"));

	auto LogSwitchOutlet = [&](UFGStructuralPowerConnectionComponent* OutletBus, int32 Powered, const TCHAR* Mode)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] outlet %s scope=site site=%d role=router root=%d parentValid=%d busCircuit=%d"
				" powered=%d tag=%s source=%s control=%s wirePort=%s mode=%s"),
			*Switch->GetName(),
			Root,
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			IsValid(OutletBus) ? OutletBus->GetCircuitID() : INDEX_NONE,
			Powered,
			StructuralChannelToString(ChannelKey.Tag),
			*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
			*FStructuralPowerTrace::FormatControlForTrace(ChannelKey),
			WirePort,
			Mode);
	};

	if (!Switch->IsBridgeActive())
	{
		TearDown(Graph, Switch);
		LogSwitchOutlet(nullptr, 0, TEXT("inactive"));
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = Graph.GetOrCreateBusConnector(Switch);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_bus_create_failed"));
		return;
	}

	const bool bKeyedSubnet = HasAssignedControl(Graph, Switch);
	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	ApplyRuntimeAttach(Graph, Switch, OutletBus, Root, SwitchId, AttachContext);

	LogSwitchOutlet(
		OutletBus,
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
		AttachContextToString(AttachContext));

	if (IsBulkLoadAttachContext(AttachContext))
	{
		return;
	}

	if (Root != INDEX_NONE
		&& bKeyedSubnet
		&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		RestitchKeyedConsumersOnRoot(Graph, Root, ChannelKey.Control);
	}
}
