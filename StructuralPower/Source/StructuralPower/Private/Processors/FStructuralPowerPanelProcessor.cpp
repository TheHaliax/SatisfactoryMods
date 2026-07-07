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
#include "Core/FStructuralPowerContext.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPowerPanelProcessor::TearDown(
	FStructuralPowerContext& Ctx,
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
	FStructuralPowerContext& Ctx,
	AFGBuildableLightsControlPanel* Panel,
	bool bLocalPromoteOnly)
{
	if (!IsValid(Panel))
	{
		return;
	}

	FStructuralCircuitPromotionScope PromotionScope(&Ctx.Graph());

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		FStructuralPowerTrace::LogPlacementSkip(Panel, TEXT("panel_ports_unresolved"));
		return;
	}

	const FStructuralChannelKey ChannelKey = Ctx.Graph().ResolveChannelKeyForBuildable(Panel);
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const FStructuralWallAnchor ParentAnchor = Ctx.Graph().ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = Ctx.Graph().ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	const FStructuralNodeId PanelId = Ctx.Graph().MakeNodeId(Panel);
	FTrackedEndpoint& Tracked = Ctx.Graph().TrackedEndpoints.FindOrAdd(PanelId);
	const bool bRoutingUnchanged = Tracked.bPanelLinksReady
		&& Tracked.CachedPanelRoot == Root
		&& Tracked.CachedPanelKey == ChannelKey;

	Tracked.Actor = Panel;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Panel;
	Ctx.Graph().RegisterBuildableActor(Panel);
	Ctx.Graph().EnsurePanelListener(Panel);
	if (Root != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(AttachContext))
		{
			Ctx.Graph().AddEndpointToRootIndex(Root, EStructuralEndpointKind::Panel, PanelId);
		}
		else
		{
			Ctx.Graph().MarkBridgeEndpointRootIndexDirty();
		}
	}

	auto LogPanelState = [&](bool bSupplyReady, const TCHAR* Context)
	{
		const int32 Controlled =
			Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();
		FStructuralPowerTrace::LogPanelConsumer(
			Panel,
			Root,
			ChannelKey,
			Ports,
			bSupplyReady,
			Controlled,
			Context);
	};

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		if (!bRoutingUnchanged)
		{
			FStructuralPanelAttach::TearDownLinks(Panel, Ports);
			Tracked.bPanelLinksReady = false;
		}

		LogPanelState(false, TEXT("lighting_disabled"));
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
			LogPanelState(true, TEXT("routing_unchanged"));

			const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
				&& Tracked.CachedDownstreamControl == ChannelKey.Control;
			if (Root != INDEX_NONE
				&& ChannelKey.Control != StructuralPowerConstants::ControlUnconfigured)
			{
				if (!bDownstreamUnchanged)
				{
					FStructuralPanelAttach::RestitchDownstream(
						Ctx.Graph(),
						Panel,
						Ports,
						Root,
						ChannelKey.Control);
					Tracked.bDownstreamLinksReady = true;
					Tracked.CachedDownstreamControl = ChannelKey.Control;
				}
				else
				{
					FStructuralPanelControlledSync::ApplyKeyedSubnet(Ctx.Graph(), Panel);
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
	if (Root != INDEX_NONE && Ctx.Graph().DoesComponentRootCarryPower(Root))
	{
		if (FStructuralPanelAttach::SupplyAlreadyLinked(Ctx.Graph(), Panel, Ports, Root, ChannelKey))
		{
			bSupplyReady = true;
		}
		else
		{
			bSupplyReady = FStructuralPanelAttach::TryLinkSupply(
				Ctx.Graph(),
				Panel,
				Ports,
				Root,
				ChannelKey);
		}
	}

	if (bSupplyReady && IsValid(InputPower))
	{
		UFGPowerConnectionComponent* Feed =
			Ctx.Graph().ResolveSubnetFeedConnector(Root, ChannelKey);
		if (IsValid(Feed))
		{
			Ctx.Graph().PromotePanelSupplyConnection(InputPower, Feed, bLocalPromoteOnly);
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
				Ctx.Graph(),
				Panel,
				Ports,
				Root,
				ChannelKey.Control);
		}
		else
		{
			FStructuralPanelControlledSync::ApplyKeyedSubnet(Ctx.Graph(), Panel);
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

	const int32 Controlled =
		Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();
	FStructuralPowerTrace::LogPanelConsumer(
		Panel,
		Root,
		ChannelKey,
		Ports,
		bSupplyReady,
		Controlled,
		TEXT("process"));
}

void FStructuralPowerPanelProcessor::RestitchOnRoot(
	FStructuralPowerContext& Ctx,
	int32 Root)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Ctx.Graph().RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* PanelIds =
		Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Panel);
	if (!PanelIds || PanelIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
	for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(PanelId);
		if (!Tracked)
		{
			continue;
		}

		if (AFGBuildableLightsControlPanel* Panel =
				Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get()))
		{
			FTrackedEndpoint& Mutable = Ctx.Graph().TrackedEndpoints.FindOrAdd(PanelId);
			Mutable.bPanelLinksReady = false;
			Mutable.bDownstreamLinksReady = false;
			Process(Ctx, Panel);
		}
	}
}

void FStructuralPowerPanelProcessor::RestitchWithControlOnRoot(
	FStructuralPowerContext& Ctx,
	int32 Root,
	FName ControlId)
{
	if (Root == INDEX_NONE
		|| !FStructuralPowerModConfig::IsGroupLightingEnabled()
		|| ControlId.IsNone()
		|| ControlId == StructuralPowerConstants::ControlUnconfigured)
	{
		return;
	}

	Ctx.Graph().RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* PanelIds =
		Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Panel);
	if (!PanelIds || PanelIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
	for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(PanelId);
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

		const FStructuralChannelKey PanelKey = Ctx.Graph().ResolveChannelKeyForBuildable(Panel);
		if (PanelKey.Control != ControlId)
		{
			continue;
		}

		FTrackedEndpoint& Mutable = Ctx.Graph().TrackedEndpoints.FindOrAdd(PanelId);
		Mutable.bPanelLinksReady = false;
		Mutable.bDownstreamLinksReady = false;
		Process(Ctx, Panel);
	}
}
