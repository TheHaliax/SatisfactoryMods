// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerPanelProcessor.h"

#include "Core/EAttachContext.h"
#include "Attach/FStructuralPanelAttach.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPowerPanelProcessor::TearDown(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	FStructuralPanelPorts Ports;
	if (FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		FStructuralPanelAttach::TearDownLinks(Panel, Ports);
	}
}

void FStructuralPowerPanelProcessor::Process(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	bool bLocalPromoteOnly)
{
	if (!IsValid(Panel))
	{
		return;
	}

	FStructuralCircuitPromotionScope PromotionScope(&Graph);

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		FStructuralPowerTrace::LogPlacementSkip(Panel, TEXT("panel_ports_unresolved"));
		return;
	}

	const FStructuralChannelKey ChannelKey = Graph.ResolveChannelKeyForBuildable(Panel);
	const EAttachContext AttachContext = Graph.GetCurrentAttachContext();
	const FStructuralWallAnchor ParentAnchor = Graph.ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = Graph.ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	const FStructuralNodeId PanelId = Graph.MakeNodeId(Panel);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(PanelId);
	const bool bRoutingUnchanged = Tracked.bPanelLinksReady
		&& Tracked.CachedPanelRoot == Root
		&& Tracked.CachedPanelKey == ChannelKey;

	Tracked.Actor = Panel;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Panel;
	Graph.RegisterBuildableActor(Panel);
	Graph.EnsurePanelListener(Panel);
	if (Root != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(AttachContext))
		{
			Graph.AddEndpointToRootIndex(Root, EStructuralEndpointKind::Panel, PanelId);
		}
		else
		{
			Graph.MarkBridgeEndpointRootIndexDirty();
		}
	}

	auto LogPanelOutlet = [&](int32 Powered, int32 BusCircuit)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] panel %s scope=site site=%d role=host root=%d parentValid=%d busCircuit=%d"
				" powered=%d tag=%s source=%s control=%s wirePort=-"),
			*Panel->GetName(),
			Root,
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			BusCircuit,
			Powered,
			StructuralChannelToString(ChannelKey.Tag),
			*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
			*FStructuralPowerTrace::FormatControlForTrace(ChannelKey));
	};

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		if (!bRoutingUnchanged)
		{
			FStructuralPanelAttach::TearDownLinks(Panel, Ports);
			Tracked.bPanelLinksReady = false;
		}

		LogPanelOutlet(0, INDEX_NONE);
		return;
	}

	if (bRoutingUnchanged)
	{
		const UFGPowerConnectionComponent* InputPower =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
		if (!FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower))
		{
			Tracked.bPanelLinksReady = false;
			Tracked.bDownstreamLinksReady = false;
		}
		else
		{
			const int32 BusCircuit = IsValid(InputPower) ? InputPower->GetCircuitID() : INDEX_NONE;
			const int32 Powered =
				FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower) ? 1 : 0;
			LogPanelOutlet(Powered, BusCircuit);

			const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
				&& Tracked.CachedDownstreamControl == ChannelKey.Control;
			if (Root != INDEX_NONE
				&& ChannelKey.Control != StructuralPowerConstants::ControlUnconfigured)
			{
				if (!bDownstreamUnchanged)
				{
					FStructuralPanelAttach::RestitchDownstream(
						Graph,
						Panel,
						Ports,
						Root,
						ChannelKey.Control);
					Tracked.bDownstreamLinksReady = true;
					Tracked.CachedDownstreamControl = ChannelKey.Control;
				}
				else
				{
					FStructuralPanelControlledSync::ApplyKeyedSubnet(Graph, Panel);
				}
			}

			return;
		}
	}

	FStructuralPanelAttach::TearDownLinks(Panel, Ports);
	Tracked.bDownstreamLinksReady = false;
	Tracked.CachedDownstreamControl = NAME_None;

	UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);

	bool bSupplyReady = false;
	if (Root != INDEX_NONE && Graph.DoesComponentRootCarryPower(Root))
	{
		if (FStructuralPanelAttach::SupplyAlreadyLinked(Graph, Panel, Ports, Root, ChannelKey))
		{
			bSupplyReady = true;
		}
		else
		{
			bSupplyReady = FStructuralPanelAttach::TryLinkSupply(
				Graph,
				Panel,
				Ports,
				Root,
				ChannelKey);
		}
	}

	if (bSupplyReady && IsValid(InputPower))
	{
		UFGPowerConnectionComponent* Feed =
			Graph.ResolveSubnetFeedConnector(Root, ChannelKey);
		if (IsValid(Feed))
		{
			Graph.PromotePanelSupplyConnection(InputPower, Feed, bLocalPromoteOnly);
		}
	}

	if (Root != INDEX_NONE
		&& ChannelKey.Control != StructuralPowerConstants::ControlUnconfigured)
	{
		const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
			&& Tracked.CachedDownstreamControl == ChannelKey.Control;
		if (!bDownstreamUnchanged)
		{
			FStructuralPanelAttach::RestitchDownstream(
				Graph,
				Panel,
				Ports,
				Root,
				ChannelKey.Control);
		}
		else
		{
			FStructuralPanelControlledSync::ApplyKeyedSubnet(Graph, Panel);
		}

		Tracked.bDownstreamLinksReady = true;
		Tracked.CachedDownstreamControl = ChannelKey.Control;
	}
	else
	{
		Tracked.bDownstreamLinksReady = false;
		Tracked.CachedDownstreamControl = NAME_None;
	}

	Tracked.CachedPanelKey = ChannelKey;
	Tracked.CachedPanelRoot = Root;
	Tracked.bPanelLinksReady = true;

	const int32 BusCircuit = IsValid(InputPower) ? InputPower->GetCircuitID() : INDEX_NONE;
	const int32 Powered =
		FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower) ? 1 : 0;
	const int32 Controlled =
		Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();
	UE_LOG(LogStructuralPower, Verbose,
		TEXT("[HALSP] panel %s scope=site site=%d role=host root=%d powered=%d busCircuit=%d"
			" source=%s control=%s controlled=%d"),
		*Panel->GetName(),
		Root,
		Root,
		Powered,
		BusCircuit,
		*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
		*FStructuralPowerTrace::FormatControlForTrace(ChannelKey),
		Controlled);
}

void FStructuralPowerPanelProcessor::RestitchOnRoot(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Root,
	EAttachContext /*AttachContext*/)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Graph.RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* PanelIds =
		Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Panel);
	if (!PanelIds || PanelIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
	for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(PanelId);
		if (!Tracked)
		{
			continue;
		}

		if (AFGBuildableLightsControlPanel* Panel =
				Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get()))
		{
			FTrackedEndpoint& Mutable = Graph.TrackedEndpoints.FindOrAdd(PanelId);
			Mutable.bPanelLinksReady = false;
			Mutable.bDownstreamLinksReady = false;
			Process(Graph, Panel);
		}
	}
}

void FStructuralPowerPanelProcessor::RestitchWithControlOnRoot(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Root,
	FName ControlId,
	EAttachContext /*AttachContext*/)
{
	if (Root == INDEX_NONE
		|| !FStructuralPowerModConfig::IsGroupLightingEnabled()
		|| ControlId.IsNone()
		|| ControlId == StructuralPowerConstants::ControlUnconfigured)
	{
		return;
	}

	Graph.RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* PanelIds =
		Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Panel);
	if (!PanelIds || PanelIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
	for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(PanelId);
		if (!Tracked)
		{
			continue;
		}

		AFGBuildableLightsControlPanel* Panel =
			Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get());
		if (!IsValid(Panel))
		{
			continue;
		}

		const FStructuralChannelKey PanelKey = Graph.ResolveChannelKeyForBuildable(Panel);
		if (PanelKey.Control != ControlId)
		{
			continue;
		}

		FTrackedEndpoint& Mutable = Graph.TrackedEndpoints.FindOrAdd(PanelId);
		Mutable.bPanelLinksReady = false;
		Mutable.bDownstreamLinksReady = false;
		Process(Graph, Panel);
	}
}
