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
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralCrossSiteGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Network/UStructuralPowerSwitchListener.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Connection/FStructuralSwitchConnectionPoint.h"
#include "Processors/FStructuralPowerBridgeProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Core/FStructuralPowerContext.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	FStructuralPowerBridgeProcessor::ApplyLocalAttachForSwitch(
		Ctx,
		Switch,
		OutletBus,
		Root,
		Ctx.Graph().MakeNodeId(Switch),
		Ctx.GetAttachContext(),
		/*bKeyedSubnet=*/false);
}

void FStructuralPowerSwitchProcessor::ApplyAdvancedAttach(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SwitchId,
	bool bKeyedSubnet)
{
	FStructuralPowerBridgeProcessor::ApplyLocalAttachForSwitch(
		Ctx,
		Switch,
		OutletBus,
		Root,
		SwitchId,
		Ctx.GetAttachContext(),
		bKeyedSubnet);
}

void FStructuralPowerSwitchProcessor::ApplyRuntimeAttach(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* /*OutletBus*/,
	int32 /*Root*/,
	const FStructuralNodeId& /*SwitchId*/,
	EAttachContext AttachContext)
{
	if (!IsValid(Switch))
	{
		return;
	}

	FStructuralSwitchConnectionPoint(Ctx.Graph(), Switch).OnWireOrGateChanged(AttachContext);
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
	bool bSwitchOn,
	const TCHAR* PhaseSuffix)
{
	if (Root == INDEX_NONE || SwitchControlId.IsNone())
	{
		return;
	}

	if (!PhaseSuffix)
	{
		PhaseSuffix = TEXT("");
	}

	int32 PanelCount = 0;
	int32 PanelFedCount = 0;
	int32 DirectLightCount = 0;
	int32 PoweredDirectCount = 0;
	int32 PanelLightCount = 0;
	int32 ArmedPanelCount = 0;
	int32 PassPanelCount = 0;

	auto CountMatchingPanel = [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildableLightsControlPanel* Panel = Tracked->GetPanel();
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

		if (bSupplyReady)
		{
			++PanelFedCount;
		}

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

		for (AFGBuildable* ControlledBuildable :
				Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()))
		{
			AFGBuildableLightSource* ControlledLight =
				FStructuralPowerBuildableCasts::AsLight(ControlledBuildable);
			if (!IsValid(ControlledLight))
			{
				continue;
			}

			UFGPowerConnectionComponent* Plug =
				FStructuralDeviceAttach::FindLightWireConnection(ControlledLight);
			const bool bArmedOn = ControlledLight->ShouldLightBeOn();
			++PanelLightCount;
			if (bArmedOn)
			{
				++ArmedPanelCount;
			}
			if (bSupplyReady && bArmedOn)
			{
				++PassPanelCount;
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

		AFGBuildableLightSource* Light = Tracked->GetLight();
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
		UFGPowerConnectionComponent* PlugForCount = FStructuralDeviceAttach::FindLightWireConnection(Light);
		if (IsValid(PlugForCount) && PlugForCount->HasPower())
		{
			++PoweredDirectCount;
		}

		if (FStructuralPowerTrace::IsEnabled())
		{
			UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
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

	const bool bBridgeActive = IsValid(Switch) && Switch->IsBridgeActive();
	const TCHAR* RestitchPhase = bSwitchOn ? TEXT("on") : TEXT("off");

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] switch restitch_%s%s %s scope=%s site=%d role=%s root=%d control=%s"
			" bridgeActive=%d panels=%d panelFed=%d direct_lights=%d poweredDirect=%d"
			" panel_lights=%d armedPanel=%d passPanel=%d"),
		RestitchPhase,
		PhaseSuffix,
		IsValid(Switch) ? *Switch->GetName() : TEXT("null"),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Router),
		Root,
		*FStructuralPowerTrace::FormatControlForTrace(SwitchKey),
		bBridgeActive ? 1 : 0,
		PanelCount,
		PanelFedCount,
		DirectLightCount,
		PoweredDirectCount,
		PanelLightCount,
		ArmedPanelCount,
		PassPanelCount);
}

void FStructuralPowerSwitchProcessor::ScheduleSettledRestitchSummary(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	int32 Root,
	FName SwitchControlId)
{
	UWorld* World = Graph.GetWorld();
	if (!IsValid(World) || !IsValid(Switch) || Root == INDEX_NONE || SwitchControlId.IsNone())
	{
		return;
	}

	TWeakObjectPtr<AStructuralPowerGraphSubsystem> WeakGraph(&Graph);
	TWeakObjectPtr<AFGBuildableCircuitSwitch> WeakSwitch(Switch);
	const int32 RootCopy = Root;
	const FName ControlCopy = SwitchControlId;

	// One tick so OFF pass/fail reads after circuit settle, not mid-hook.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakGraph, WeakSwitch, RootCopy, ControlCopy]()
		{
			AStructuralPowerGraphSubsystem* GraphPtr = WeakGraph.Get();
			AFGBuildableCircuitSwitch* SwitchPtr = WeakSwitch.Get();
			if (!IsValid(GraphPtr) || !IsValid(SwitchPtr))
			{
				return;
			}

			FStructuralPowerContext Ctx =
				GraphPtr->MakeProcessorContext(EAttachContext::Toggle, RootCopy);
			LogConsumerRestitchSummary(
				Ctx,
				SwitchPtr,
				RootCopy,
				ControlCopy,
				/*bSwitchOn=*/false,
				TEXT("_settled"));
		}));
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

	const bool bDeferPanelsToGateFinish =
		Ctx.GetAttachContext() == EAttachContext::Toggle
		&& FStructuralPowerModConfig::IsGroupLightingEnabled();

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

		if (AFGBuildableLightsControlPanel* Panel = Tracked->GetPanel())
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

		if (AFGBuildableLightSource* Light = Tracked->GetLight())
		{
			FStructuralPowerLightProcessor::Process(Ctx, Light, bLocalPromoteOnly);
		}
	};

	if (!bDeferPanelsToGateFinish)
	{
		if (const TArray<FStructuralNodeId>* PanelIds =
				Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
		{
			const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
			for (const FStructuralNodeId& NodeId : PanelIdsSnapshot)
			{
				RestitchMatchingPanel(NodeId);
			}
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

		AFGBuildableCircuitSwitch* KeyedSwitch = Tracked->GetSwitch();
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
			FStructuralPowerBridgeProcessor::ApplyLocalAttachForSwitch(
				Ctx,
				KeyedSwitch,
				OutletBus,
				Root,
				NodeId,
				Ctx.GetAttachContext(),
				/*bKeyedSubnet=*/true);
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

		int32 Root = ResolveToggleSiteRoot(Ctx, Switch, *Tracked);

		const FStructuralChannelKey SwitchKey = Ctx.Graph().ResolveChannelKeyForBuildable(Switch);
		const bool bKeyedSubnet = HasAssignedControl(Ctx, Switch);
		const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
		const bool bSwitchOn = Switch->IsBridgeActive();

		Ctx.Graph().MarkEchoDirtyForSwitchToggle(Switch, Root);

		FStructuralPowerTrace::LogHook(
			Switch,
			TEXT("OnIsSwitchOnChanged"),
			bSwitchOn ? TEXT("restitch_on") : TEXT("restitch_off"),
			nullptr);

		if (!bSwitchOn)
		{
			FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, /*bGateOpen=*/false);
			TearDown(Ctx, Switch);
			if (Root != INDEX_NONE
				&& bKeyedSubnet
				&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
			{
				FStructuralPowerTransferGate::ApplyKeyedTransferOnRoot(
					Ctx,
					Root,
					SwitchKey.Control,
					/*bGateOpen=*/false,
					/*bLocalPromoteOnly=*/true);
			}
			if (bWiredBridge
				&& !FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
			{
				PropagateWiredFeedChange(Ctx, Switch, Root);
			}
		}
		else
		{
			FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, /*bGateOpen=*/true);
			FStructuralSwitchConnectionPoint(Ctx.Graph(), Switch).OnWireOrGateChanged(
				EAttachContext::Toggle);
			if (Root != INDEX_NONE
				&& bKeyedSubnet
				&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
			{
				FStructuralPowerTransferGate::ApplyKeyedTransferOnRoot(
					Ctx,
					Root,
					SwitchKey.Control,
					/*bGateOpen=*/true,
					/*bLocalPromoteOnly=*/true);
			}
		}

		if (Root != INDEX_NONE
			&& (bSwitchOn || !bKeyedSubnet))
		{
			FStructuralPowerBridgeProcessor::FinishPanelBridgeLegsOnSiteAfterGateChange(
				Ctx,
				Root);
		}

		if (!bSwitchOn
			&& Root != INDEX_NONE
			&& bKeyedSubnet
			&& Ctx.Graph().DoesComponentRootCarryPower(Root)
			&& FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			if (const TArray<FStructuralNodeId>* LightIds =
					Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Light))
			{
				const TArray<FStructuralNodeId> LightIdsSnapshot = *LightIds;
				for (const FStructuralNodeId& LightId : LightIdsSnapshot)
				{
					const FTrackedEndpoint* LightTracked =
						Ctx.Graph().TrackedEndpoints.Find(LightId);
					if (!LightTracked)
					{
						continue;
					}

					AFGBuildableLightSource* Light = LightTracked->GetLight();
					if (!IsValid(Light))
					{
						continue;
					}

					const FStructuralChannelKey LightKey =
						Ctx.Graph().ResolveChannelKeyForBuildable(Light);
					if (LightKey.Source == SwitchKey.Control
						|| Ctx.Graph().IsPanelDownstreamLight(Root, LightKey))
					{
						continue;
					}

					FStructuralPowerLightProcessor::Process(
						Ctx,
						Light,
						/*bLocalPromoteOnly=*/true);
				}
			}
		}

		if (Root != INDEX_NONE
			&& bKeyedSubnet
			&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
		{
			LogConsumerRestitchSummary(Ctx, Switch, Root, SwitchKey.Control, bSwitchOn);
			if (!bSwitchOn)
			{
				ScheduleSettledRestitchSummary(Ctx.Graph(), Switch, Root, SwitchKey.Control);
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
			TEXT("[HALSP] outlet %s scope=%s site=%d role=%s root=%d parentValid=%d busCircuit=%d"
				" powered=%d tag=%s source=%s control=%s wirePort=%s mode=%s"),
			*Switch->GetName(),
			StructuralPowerScopeToString(EStructuralPowerScope::Site),
			Root,
			StructuralPowerRoleToString(EStructuralPowerRole::Router),
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
		FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, /*bGateOpen=*/false);
		LogSwitchOutlet(nullptr, 0, TEXT("inactive"));
		return;
	}

	Tracked.bStructuralPowerTransferActive = true;

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
