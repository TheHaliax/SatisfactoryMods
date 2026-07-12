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
#include "Processors/FStructuralPowerBridgeProcessor.h"
#include "Routing/EStructuralChannel.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
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
		const UFGPowerConnectionComponent* InputPower =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
		const bool bInputLive = bSupplyLinked
			&& IsValid(InputPower)
			&& FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower);
		if (bInputLive)
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
	Tracked.bPanelLinksReady = IsValid(InputPower)
		&& FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower);

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

void FStructuralPowerPanelProcessor::OnWireDelta(
	FStructuralPowerContext& Ctx,
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel) || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	AStructuralPowerGraphSubsystem& Graph = Ctx.Graph();
	const EAttachContext AttachContext = Ctx.GetAttachContext();

	if (AttachContext != EAttachContext::Toggle
		&& Graph.ShouldSkipPanelCircuitEcho(Panel))
	{
		return;
	}

	if (AttachContext == EAttachContext::Toggle)
	{
		const TCHAR* SkipReason = nullptr;
		if (Graph.ShouldSkipPanelCircuitEcho(Panel, &SkipReason)
			&& SkipReason
			&& FCString::Strcmp(SkipReason, TEXT("skip_feed_open")) == 0)
		{
			Graph.NotePanelToggleHandled(Panel);
			return;
		}
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		return;
	}

	const FStructuralWallAnchor ParentAnchor = Graph.ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = Graph.ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	const FStructuralNodeId PanelId = Graph.MakeNodeId(Panel);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(PanelId);
	Tracked.Actor = Panel;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Panel;
	Graph.RegisterBuildableActor(Panel);
	Graph.EnsurePanelListener(Panel);
	if (Root != INDEX_NONE)
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	const bool bLocalPromoteOnly =
		AttachContext != EAttachContext::RuntimePlace
		|| IsBulkLoadAttachContext(AttachContext)
		|| AttachContext == EAttachContext::WireDelta
		|| AttachContext == EAttachContext::Toggle;

	if (AttachContext == EAttachContext::WireDelta && Tracked.bPanelLinksReady
		&& Tracked.CachedPanelRoot == Root)
	{
		const FStructuralChannelKey ChannelKey = Graph.ResolveChannelKeyForBuildable(Panel);
		if (FStructuralPanelAttach::SupplyAlreadyLinked(
				Graph,
				Panel,
				Ports,
				Root,
				ChannelKey))
		{
			UFGPowerConnectionComponent* InputPower =
				FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
			if (IsValid(InputPower))
			{
				Graph.PromoteDirectHiddenLinks(InputPower);
			}

			const FName EffectiveControl =
				FStructuralPanelControlledSync::ResolveEffectiveLightControl(Graph, Panel);
			if (!EffectiveControl.IsNone())
			{
				FStructuralPanelControlledSync::ApplyKeyedSubnet(Graph, Panel);
			}

			UE_LOG(LogStructuralPower, Log,
				TEXT("[HALSP] panel wire delta %s kind=%s scope=%s site=%d role=%s attach=%s"
					" root=%d inputPowered=%d path=repair_only"),
				*Panel->GetName(),
				StructuralEndpointKindToString(EStructuralEndpointKind::Panel),
				StructuralPowerScopeToString(EStructuralPowerScope::Site),
				Root,
				StructuralPowerRoleToString(EStructuralPowerRole::Router),
				AttachContextToString(AttachContext),
				Root,
				InputPower && FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower)
					? 1
					: 0);
			return;
		}
	}

	FStructuralPowerBridgeProcessor::ApplyLocalAttachForPanel(
		Ctx,
		Panel,
		bLocalPromoteOnly);

	const UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] panel wire delta %s kind=%s scope=%s site=%d role=%s attach=%s"
			" root=%d inputPowered=%d"),
		*Panel->GetName(),
		StructuralEndpointKindToString(EStructuralEndpointKind::Panel),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Router),
		AttachContextToString(AttachContext),
		Root,
		InputPower && FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower) ? 1 : 0);

	if (AttachContext == EAttachContext::Toggle)
	{
		Graph.NotePanelToggleHandled(Panel);
	}
}

