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
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "FGPowerConnectionComponent.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Connection/FStructuralPanelConnectionPoint.h"
#include "Routing/EStructuralChannel.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Graph/FStructuralEndpointTypes.h"
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
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("panel.Process"),
		IsValid(Panel) ? Panel->GetName() : TEXT("null"));

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
		FStructuralPanelAttach::TearDownLinks(Panel, Ports);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Tracked.bStructuralPowerTransferActive = false;

		LogPanelState(false, TEXT("lighting_disabled"));
		return;
	}

	const FName FeedSwitchId = ChannelKey.Source;
	if (Root != INDEX_NONE
		&& !FeedSwitchId.IsNone()
		&& Ctx.Graph().IsSwitchFeedOpen(Root, FeedSwitchId))
	{
		FStructuralPanelAttach::TearDownLinks(Panel, Ports);

		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Tracked.bStructuralPowerTransferActive = false;
		LogPanelState(false, TEXT("switch_feed_open"));
		return;
	}

	if (bRoutingUnchanged)
	{
		if (Root != INDEX_NONE
			&& !FeedSwitchId.IsNone()
			&& Ctx.Graph().IsSwitchFeedOpen(Root, FeedSwitchId))
		{
			FStructuralPanelAttach::TearDownLinks(Panel, Ports);
			Tracked.bPanelLinksReady = false;
			Tracked.bDownstreamLinksReady = false;
			Tracked.bStructuralPowerTransferActive = false;
			LogPanelState(false, TEXT("switch_feed_open"));
			return;
		}

		const bool bSupplyLinked = FStructuralPanelAttach::SupplyAlreadyLinked(
			Ctx.Graph(),
			Panel,
			Ports,
			Root,
			ChannelKey);
		if (bSupplyLinked)
		{
			LogPanelState(true, TEXT("routing_unchanged"));

			const FName EffectiveControl =
				FStructuralPanelControlledSync::ResolveEffectiveLightControl(Ctx.Graph(), Panel);
			const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
				&& Tracked.CachedDownstreamControl == EffectiveControl;
			if (Root != INDEX_NONE && !EffectiveControl.IsNone())
			{
				if (!bDownstreamUnchanged)
				{
					FStructuralPanelAttach::RestitchDownstream(
						Ctx.Graph(),
						Panel,
						Ports,
						Root,
						EffectiveControl);
					Tracked.bDownstreamLinksReady = true;
					Tracked.CachedDownstreamControl = EffectiveControl;
				}
				else
				{
					FStructuralPanelControlledSync::ApplyKeyedSubnet(Ctx.Graph(), Panel);
				}
			}

			return;
		}

		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
	}

	FStructuralPanelAttach::TearDownLinks(Panel, Ports);
	Tracked.bDownstreamLinksReady = false;
	Tracked.CachedDownstreamControl = NAME_None;

	UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);

	const bool bMeshOnlySupply = IsBulkLoadAttachContext(AttachContext);

	bool bSupplyReady = false;
	if (Root != INDEX_NONE)
	{
		const bool bFeedReady = Ctx.Graph().DoesComponentRootCarryPower(Root)
			|| Ctx.Graph().DoesSiteStructuralBusCarryPower(Root)
			|| (!ChannelKey.Source.IsNone()
				&& [&]() -> bool
				{
					UFGPowerConnectionComponent* Feed =
						Ctx.Graph().ResolveSubnetFeedConnector(Root, ChannelKey);
					return IsValid(Feed)
						&& FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(Feed);
				}());

		if (bFeedReady)
		{
			if (FStructuralPanelAttach::SupplyAlreadyLinked(
					Ctx.Graph(), Panel, Ports, Root, ChannelKey))
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
					ChannelKey,
					bMeshOnlySupply);
			}
		}
	}

	if (bSupplyReady && IsValid(InputPower) && !bMeshOnlySupply)
	{
		UFGPowerConnectionComponent* Feed =
			Ctx.Graph().ResolveSubnetFeedConnector(Root, ChannelKey);
		if (IsValid(Feed))
		{
			Ctx.Graph().PromotePanelSupplyConnection(InputPower, Feed, bLocalPromoteOnly);
		}
	}

	const FName EffectiveControl =
		FStructuralPanelControlledSync::ResolveEffectiveLightControl(Ctx.Graph(), Panel);
	if (Root != INDEX_NONE && !EffectiveControl.IsNone())
	{
		const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
			&& Tracked.CachedDownstreamControl == EffectiveControl;
		if (!bDownstreamUnchanged)
		{
			FStructuralPanelAttach::RestitchDownstream(
				Ctx.Graph(),
				Panel,
				Ports,
				Root,
				EffectiveControl);
		}
		else
		{
			FStructuralPanelControlledSync::ApplyKeyedSubnet(Ctx.Graph(), Panel);
		}

		Tracked.bDownstreamLinksReady = true;
		Tracked.CachedDownstreamControl = EffectiveControl;
	}
	else
	{
		Tracked.bDownstreamLinksReady = false;
		Tracked.CachedDownstreamControl = NAME_None;
	}

	if (!bLocalPromoteOnly && bSupplyReady && IsValid(InputPower))
	{
		FStructuralPanelAttach::PromotePanelDownstreamSubnet(
			Ctx.Graph(),
			Panel,
			Ports,
			InputPower);
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

