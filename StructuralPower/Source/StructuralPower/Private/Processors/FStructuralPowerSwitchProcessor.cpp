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
#include "Core/FStructuralPowerContext.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPowerSwitchProcessor::TearDown(
	FStructuralPowerContext& Ctx,
	AFGBuildable* Host)
{
	UFGStructuralPowerConnectionComponent* Bus = Ctx.Graph().FindBusConnector(Host);
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
	FStructuralPowerContext& Ctx,
	int32 Root)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Ctx.Graph().TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch
			|| Ctx.Graph().StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch();
		if (!IsValid(Switch) || Switch->IsBridgeActive())
		{
			continue;
		}

		TearDown(Ctx, Switch);
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
	return Control != StructuralPowerConstants::ControlBypass && !Control.IsNone();
}

bool FStructuralPowerSwitchProcessor::NeedsAdvancedWork(
	const FStructuralPowerContext& Ctx,
	const AFGBuildableCircuitSwitch* Switch)
{
	return HasAssignedControl(Ctx, Switch)
		|| FStructuralSwitchParentResolver::IsWiredToStructureSide(
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
	Listener->BindSubsystem(&Ctx.Graph(), Switch);
}

void FStructuralPowerSwitchProcessor::RestitchKeyedSubnet(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 ComponentRoot,
	const FStructuralNodeId& SwitchNodeId)
{
	if (!IsValid(Switch) || !IsValid(OutletBus) || ComponentRoot == INDEX_NONE)
	{
		return;
	}

	if (UFGCircuitConnectionComponent* Feed = Ctx.Graph().GetComponentSourceConnector(ComponentRoot, Switch))
	{
		if (UFGPowerConnectionComponent* FeedPower = Cast<UFGPowerConnectionComponent>(Feed))
		{
			Ctx.Graph().LinkHiddenPair(OutletBus, FeedPower);
		}
	}

	Ctx.Graph().EndpointIndex.ForEachBridgeOnRoot(ComponentRoot, [&](const FStructuralNodeId& PeerId)
	{
		if (PeerId == SwitchNodeId)
		{
			return;
		}

		const FTrackedEndpoint* PeerTracked = Ctx.Graph().TrackedEndpoints.Find(PeerId);
		if (!PeerTracked)
		{
			return;
		}

		AFGBuildable* SiblingHost = PeerTracked->Actor.Get();
		if (!IsValid(SiblingHost))
		{
			return;
		}

		if (!Ctx.Graph().ShouldMeshEndpoints(Switch, SiblingHost, ComponentRoot))
		{
			return;
		}

		if (UFGStructuralPowerConnectionComponent* SiblingBus = Ctx.Graph().FindBusConnector(SiblingHost))
		{
			Ctx.Graph().LinkHiddenPair(OutletBus, SiblingBus);
		}
	});

	UFGStructuralPowerConnectionComponent* Seed = FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus)
		? OutletBus
		: Ctx.Graph().FindPoweredHiddenReachable(OutletBus);
	if (Seed)
	{
		Ctx.Graph().PromoteDirectHiddenLinks(Seed);
	}
}

void FStructuralPowerSwitchProcessor::ApplyBaseOutletAttach(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root)
{
	if (!IsValid(Switch) || !IsValid(OutletBus) || Root == INDEX_NONE)
	{
		return;
	}

	Ctx.Graph().LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (!Ctx.Graph().DoesComponentRootCarryPower(Root))
	{
		return;
	}

	if (UFGCircuitConnectionComponent* Feed = Ctx.Graph().GetComponentSourceConnector(Root, Switch))
	{
		if (UFGPowerConnectionComponent* FeedPower = Cast<UFGPowerConnectionComponent>(Feed))
		{
			Ctx.Graph().LinkHiddenPairLocal(OutletBus, FeedPower);
		}
	}

	Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void FStructuralPowerSwitchProcessor::ApplyAdvancedAttach(
	FStructuralPowerContext& Ctx,
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

	Ctx.Graph().LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (Root == INDEX_NONE)
	{
		Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
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
		if (UFGCircuitConnectionComponent* Feed = Ctx.Graph().GetComponentSourceConnector(Root, Switch))
		{
			FeedPower = Cast<UFGPowerConnectionComponent>(Feed);
		}
	}

	if (FeedPower)
	{
		Ctx.Graph().LinkHiddenPairLocal(OutletBus, FeedPower);
	}

	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	if (bKeyedSubnet && !Ctx.Graph().HasBridgeBusPeerMesh(OutletBus))
	{
		Ctx.Graph().TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			SwitchId,
			/*bBridgePeersOnly=*/false);
	}
	else if (bWiredBridge && !Ctx.Graph().HasBridgeBusPeerMesh(OutletBus))
	{
		Ctx.Graph().TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			SwitchId,
			/*bBridgePeersOnly=*/true);
	}

	Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void FStructuralPowerSwitchProcessor::ApplyRuntimeAttach(
	FStructuralPowerContext& Ctx,
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
		Ctx.Graph().LinkBusToVisibleConnections(Switch, OutletBus);
		if (Root != INDEX_NONE)
		{
			Ctx.Graph().ApplyLocalBridgeBusAttach(Switch, OutletBus, Root, SwitchId, Switch);
		}
		else
		{
			Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/false);
		}
		return;
	}

	const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	if (bKeyedSubnet || bWiredBridge)
	{
		ApplyAdvancedAttach(Ctx, Switch, OutletBus, Root, SwitchId, bKeyedSubnet);
	}
	else
	{
		ApplyBaseOutletAttach(Ctx, Switch, OutletBus, Root);
	}
}

void FStructuralPowerSwitchProcessor::RegisterOutletBase(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	const FStructuralWallAnchor& ParentAnchor,
	FTrackedEndpoint& InOutTracked,
	int32& OutRoot,
	FStructuralNodeId& OutParentId)
{
	OutRoot = ResolveMountRoot(Ctx, ParentAnchor, OutParentId);
	InOutTracked.Actor = Switch;
	InOutTracked.ParentId = OutParentId;
	InOutTracked.Kind = EStructuralEndpointKind::Switch;
	Ctx.Graph().RegisterBuildableActor(Switch);
	if (OutParentId.IsValid() && OutRoot != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(Ctx.GetAttachContext()))
		{
			Ctx.Graph().AddEndpointToRootIndex(OutRoot, EStructuralEndpointKind::Switch, Ctx.Graph().MakeNodeId(Switch));
		}
		else
		{
			Ctx.Graph().MarkBridgeEndpointRootIndexDirty();
		}
	}
	EnsureListener(Ctx, Switch);
}

void FStructuralPowerSwitchProcessor::LogConsumerRestitchSummary(
	FStructuralPowerContext& Ctx,
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
	int32 LitDirectCount = 0;
	int32 LitPanelCount = 0;

	auto CountMatchingPanel = [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(NodeId);
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

		if (Ctx.Graph().ResolveSource(Panel, EStructuralChannel::Light) != SwitchControlId)
		{
			return;
		}

		++PanelCount;

		FStructuralPanelPorts Ports;
		if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
		{
			return;
		}

		const FStructuralChannelKey PanelKey = Ctx.Graph().ResolveChannelKeyForBuildable(Panel);
		const UFGPowerConnectionComponent* InputPower =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
		const bool bSupplyReady =
			FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower);
		const UFGPowerConnectionComponent* DownstreamPower =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream);
		const bool bDownstreamFed =
			FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(DownstreamPower);
		const int32 Controlled =
			Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();

		if (FStructuralPowerTrace::IsEnabled())
		{
			FStructuralPowerTrace::LogPanelConsumer(
				Panel,
				Root,
				PanelKey,
				Ports,
				bSupplyReady,
				Controlled,
				TEXT("restitch_summary"));
		}

		if (!bSwitchOn)
		{
			return;
		}

		for (AFGBuildable* ControlledBuildable :
				Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()))
		{
			AFGBuildableLightSource* ControlledLight =
				Cast<AFGBuildableLightSource>(ControlledBuildable);
			if (!IsValid(ControlledLight))
			{
				continue;
			}

			UFGPowerConnectionComponent* Plug =
				FStructuralDeviceAttach::FindLightWireConnection(ControlledLight);
			if (bSwitchOn && ControlledLight->ShouldLightBeOn())
			{
				++LitPanelCount;
			}

			if (FStructuralPowerTrace::IsEnabled())
			{
				const FStructuralChannelKey LightKey =
					Ctx.Graph().ResolveChannelKeyForBuildable(ControlledLight);
				FStructuralPowerTrace::LogLightConsumer(
					ControlledLight,
					Root,
					true,
					LightKey,
					Plug,
					TEXT("panel_downstream"),
					bSupplyReady ? 1 : 0,
					bDownstreamFed ? 1 : 0);
			}
		}
	};

	auto CountMatchingLight = [&](const FStructuralNodeId& NodeId)
	{
		if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			return;
		}

		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Tracked->Actor.Get());
		if (!IsValid(Light))
		{
			return;
		}

		const FStructuralChannelKey LightKey = Ctx.Graph().ResolveChannelKeyForBuildable(Light);
		if (LightKey.Source != SwitchControlId)
		{
			return;
		}

		if (Ctx.Graph().IsPanelDownstreamLight(Root, LightKey))
		{
			return;
		}

		++DirectLightCount;
		UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
		if (bSwitchOn && IsValid(Plug) && Plug->HasPower())
		{
			++LitDirectCount;
		}

		if (FStructuralPowerTrace::IsEnabled())
		{
			FStructuralPowerTrace::LogLightConsumer(
				Light,
				Root,
				true,
				LightKey,
				Plug,
				TEXT("direct"));
		}
	};

	if (const TArray<FStructuralNodeId>* PanelIds =
			Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
	{
		for (const FStructuralNodeId& NodeId : *PanelIds)
		{
			CountMatchingPanel(NodeId);
		}
	}

	if (const TArray<FStructuralNodeId>* LightIds =
			Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Light))
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
			" panels=%d direct_lights=%d litDirect=%d litPanel=%d"),
		bSwitchOn ? TEXT("on") : TEXT("off"),
		IsValid(Switch) ? *Switch->GetName() : TEXT("null"),
		Root,
		Root,
		*FStructuralPowerTrace::FormatControlForTrace(SwitchKey),
		PanelCount,
		DirectLightCount,
		LitDirectCount,
		LitPanelCount);
}

void FStructuralPowerSwitchProcessor::RestitchKeyedConsumersOnRoot(
	FStructuralPowerContext& Ctx,
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

	Ctx.Graph().RefreshBridgeEndpointRootIndex();

	auto RestitchMatchingPanel = [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host))
		{
			return;
		}

		if (Ctx.Graph().ResolveSource(Host, EStructuralChannel::Light) != SwitchControlId)
		{
			return;
		}

		if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Host))
		{
			FTrackedEndpoint& Mutable = Ctx.Graph().TrackedEndpoints.FindOrAdd(NodeId);
			Mutable.bPanelLinksReady = false;
			Mutable.bDownstreamLinksReady = false;
			FStructuralPowerPanelProcessor::Process(Ctx, Panel, bLocalPromoteOnly);
		}
	};

	auto RestitchMatchingLight = [&](const FStructuralNodeId& NodeId)
	{
		if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			return;
		}

		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host))
		{
			return;
		}

		if (Ctx.Graph().ResolveSource(Host, EStructuralChannel::Light) != SwitchControlId)
		{
			return;
		}

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Host))
		{
			FStructuralPowerLightProcessor::Process(Ctx, Light, bLocalPromoteOnly);
		}
	};

	if (const TArray<FStructuralNodeId>* PanelIds =
			Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
	{
		const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
		for (const FStructuralNodeId& NodeId : PanelIdsSnapshot)
		{
			RestitchMatchingPanel(NodeId);
		}
	}

	if (const TArray<FStructuralNodeId>* LightIds =
			Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Light))
	{
		const TArray<FStructuralNodeId> LightIdsSnapshot = *LightIds;
		for (const FStructuralNodeId& NodeId : LightIdsSnapshot)
		{
			RestitchMatchingLight(NodeId);
		}
	}
}

void FStructuralPowerSwitchProcessor::RestitchActiveKeyedConsumersOnRoot(
	FStructuralPowerContext& Ctx,
	int32 Root)
{
	if (Root == INDEX_NONE
		|| !FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		return;
	}

	Ctx.Graph().RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* SwitchIds =
		Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Switch);
	if (!SwitchIds || SwitchIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> SwitchIdsSnapshot = *SwitchIds;
	for (const FStructuralNodeId& NodeId : SwitchIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* KeyedSwitch =
			Cast<AFGBuildableCircuitSwitch>(Tracked->Actor.Get());
		if (!IsValid(KeyedSwitch)
			|| !KeyedSwitch->IsBridgeActive()
			|| !HasAssignedControl(Ctx, KeyedSwitch))
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* OutletBus =
			Ctx.Graph().GetOrCreateBusConnector(KeyedSwitch);
		if (IsValid(OutletBus))
		{
			ApplyRuntimeAttach(
				Ctx,
				KeyedSwitch,
				OutletBus,
				Root,
				NodeId,
				Ctx.GetAttachContext());
		}

		const FName SwitchControl = Ctx.Graph().ResolveControl(KeyedSwitch, EStructuralChannel::Switch);
		RestitchKeyedConsumersOnRoot(Ctx, Root, SwitchControl, /*bLocalPromoteOnly=*/true);
		LogConsumerRestitchSummary(Ctx, KeyedSwitch, Root, SwitchControl, /*bSwitchOn=*/true);
	}
}

void FStructuralPowerSwitchProcessor::PropagateWiredFeedChange(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	if (!IsValid(Switch) || LocalRoot == INDEX_NONE)
	{
		return;
	}

	Ctx.Graph().CrossSiteGraph.RefreshCouplingsFromWiredSwitch(Ctx.Graph(), Switch, LocalRoot);

	TArray<int32> AffectedRoots;
	Ctx.Graph().CrossSiteGraph.TraceFeedAffected(Ctx.Graph(), Switch, LocalRoot, AffectedRoots);
	if (AffectedRoots.Num() == 0)
	{
		return;
	}

	Ctx.Graph().ReEnergizeComponentRoots(
		AffectedRoots,
		/*bTearDownFirst=*/false,
		EAttachContext::WireDelta);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] wired switch %s scope=cross_site site=%d role=gateway path=wire_bridge"
			" localRoot=%d feedAffected=%d"),
		*Switch->GetName(),
		LocalRoot,
		LocalRoot,
		AffectedRoots.Num());
}

void FStructuralPowerSwitchProcessor::OnStateChanged(
	FStructuralPowerContext& Ctx,
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

	if (IsBulkLoadAttachContext(Ctx.GetAttachContext()))
	{
		return;
	}

	const FStructuralNodeId SwitchId = Ctx.Graph().MakeNodeId(Switch);
	if (FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(SwitchId))
	{
		FStructuralCircuitPromotionScope PromotionScope(&Ctx.Graph());

		FStructuralNodeId ParentId = Tracked->ParentId;
		int32 Root = Ctx.Graph().FindRootForTrackedEndpoint(*Tracked);
		if (Root == INDEX_NONE)
		{
			const FStructuralOutletParentResolveParams ParentParams = Ctx.Graph().MakeOutletParentResolveParams();
			const FStructuralSwitchParentResolveResult ParentResolve =
				FStructuralSwitchParentResolver::Resolve(
					Switch,
					Ctx.Graph().GetWorld(),
					Ctx.Graph().StructureGraph,
					Ctx.Graph().LightweightIndex,
					/*bPreferWirePort=*/false,
					&ParentParams);
			Root = ResolveMountRoot(Ctx, ParentResolve.Anchor, ParentId);
			if (ParentResolve.IsValid())
			{
				Tracked->ParentId = ParentId;
				Ctx.Graph().MarkBridgeEndpointRootIndexDirty();
			}
		}

		const FStructuralChannelKey SwitchKey = Ctx.Graph().ResolveChannelKeyForBuildable(Switch);
		const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
		const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
		const bool bSwitchOn = Switch->IsBridgeActive();

		FStructuralPowerTrace::LogHook(
			Switch,
			TEXT("OnIsSwitchOnChanged"),
			bSwitchOn ? TEXT("restitch_on") : TEXT("restitch_off"),
			nullptr);

		if (!bSwitchOn)
		{
			TearDown(Ctx, Switch);
			if (Root != INDEX_NONE
				&& bKeyedSubnet
				&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
			{
				RestitchKeyedConsumersOnRoot(
					Ctx,
					Root,
					SwitchKey.Control,
					/*bLocalPromoteOnly=*/true);
			}
			if (bWiredBridge)
			{
				PropagateWiredFeedChange(Ctx, Switch, Root);
			}
		}
		else
		{
			UFGStructuralPowerConnectionComponent* OutletBus = Ctx.Graph().GetOrCreateBusConnector(Switch);
			if (OutletBus)
			{
				ApplyRuntimeAttach(
					Ctx,
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
						Ctx,
						Root,
						SwitchKey.Control,
						/*bLocalPromoteOnly=*/true);
				}
				if (bWiredBridge)
				{
					PropagateWiredFeedChange(Ctx, Switch, Root);
				}
			}
		}

		if (Root != INDEX_NONE
			&& FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			FStructuralPowerPanelProcessor::RestitchOnRoot(Ctx, Root);
		}

		if (Root != INDEX_NONE
			&& bKeyedSubnet
			&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
		{
			LogConsumerRestitchSummary(Ctx, Switch, Root, SwitchKey.Control, bSwitchOn);
		}

		return;
	}

	Ctx.Graph().EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}

void FStructuralPowerSwitchProcessor::Process(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_gating_disabled"));
		return;
	}

	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const FStructuralOutletParentResolveParams ParentParams = Ctx.Graph().MakeOutletParentResolveParams();
	const FStructuralSwitchParentResolveResult ParentResolve =
		FStructuralSwitchParentResolver::Resolve(
			Switch,
			Ctx.Graph().GetWorld(),
			Ctx.Graph().StructureGraph,
			Ctx.Graph().LightweightIndex,
			/*bPreferWirePort=*/false,
			&ParentParams);
	const FStructuralWallAnchor ParentAnchor = ParentResolve.Anchor;

	const FStructuralNodeId SwitchId = Ctx.Graph().MakeNodeId(Switch);
	FTrackedEndpoint& Tracked = Ctx.Graph().TrackedEndpoints.FindOrAdd(SwitchId);
	FStructuralNodeId ParentId;
	int32 Root = INDEX_NONE;
	RegisterOutletBase(Ctx, Switch, ParentResolve.Anchor, Tracked, Root, ParentId);

	const FStructuralChannelKey ChannelKey = Ctx.Graph().ResolveChannelKeyForBuildable(Switch);
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
		TearDown(Ctx, Switch);
		LogSwitchOutlet(nullptr, 0, TEXT("inactive"));
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = Ctx.Graph().GetOrCreateBusConnector(Switch);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_bus_create_failed"));
		return;
	}

	const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	ApplyRuntimeAttach(Ctx, Switch, OutletBus, Root, SwitchId, AttachContext);

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
		RestitchKeyedConsumersOnRoot(Ctx, Root, ChannelKey.Control);
	}
}
