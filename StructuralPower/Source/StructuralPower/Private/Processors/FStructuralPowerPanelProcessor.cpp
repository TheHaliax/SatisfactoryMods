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
#include "Core/FStructuralGraphSession.h"
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

	FStructuralCircuitPromotionScope PromotionScope(&Ctx.Session().Owner());

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		FStructuralPowerTrace::LogPlacementSkip(Panel, TEXT("panel_ports_unresolved"));
		return;
	}

	const FStructuralChannelKey ChannelKey = Ctx.Session().ResolveChannelKeyForBuildable(Panel);
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const FStructuralWallAnchor ParentAnchor = Ctx.Session().ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = Ctx.Session().ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	const FStructuralNodeId PanelId = Ctx.Session().MakeNodeId(Panel);
	FTrackedEndpoint& Tracked = Ctx.Session().TrackedEndpoints().FindOrAdd(PanelId);
	const bool bRoutingUnchanged = Tracked.bPanelLinksReady
		&& Tracked.CachedPanelRoot == Root
		&& Tracked.CachedPanelKey == ChannelKey;

	Tracked.Actor = Panel;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Panel;
	Ctx.Session().RegisterBuildableActor(Panel);
	Ctx.Session().EnsurePanelListener(Panel);
	if (Root != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(AttachContext))
		{
			Ctx.Session().AddEndpointToRootIndex(Root, EStructuralEndpointKind::Panel, PanelId);
		}
		else
		{
			Ctx.Session().MarkBridgeEndpointRootIndexDirty();
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
		&& Ctx.Session().IsSwitchFeedOpen(Root, FeedSwitchId))
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
			&& Ctx.Session().IsSwitchFeedOpen(Root, FeedSwitchId))
		{
			FStructuralPanelAttach::TearDownLinks(Panel, Ports);
			Tracked.bPanelLinksReady = false;
			Tracked.bDownstreamLinksReady = false;
			Tracked.bStructuralPowerTransferActive = false;
			LogPanelState(false, TEXT("switch_feed_open"));
			return;
		}

		const bool bSupplyLinked = FStructuralPanelAttach::SupplyAlreadyLinked(Ctx.Session(),
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
				FStructuralPanelControlledSync::ResolveEffectiveLightControl(Ctx.Session(), Panel);
			const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
				&& Tracked.CachedDownstreamControl == EffectiveControl;
			if (Root != INDEX_NONE && !EffectiveControl.IsNone())
			{
				if (!bDownstreamUnchanged)
				{
					FStructuralPanelAttach::RestitchDownstream(Ctx.Session(),
						Panel,
						Ports,
						Root,
						EffectiveControl);
					Tracked.bDownstreamLinksReady = true;
					Tracked.CachedDownstreamControl = EffectiveControl;
				}
				else
				{
					FStructuralPanelControlledSync::ApplyKeyedSubnet(Ctx.Session(), Panel);
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
		const bool bFeedReady = Ctx.Session().DoesComponentRootCarryPower(Root)
			|| Ctx.Session().DoesSiteStructuralBusCarryPower(Root)
			|| (!ChannelKey.Source.IsNone()
				&& [&]() -> bool
				{
					UFGPowerConnectionComponent* Feed =
						Ctx.Session().ResolveSubnetFeedConnector(Root, ChannelKey);
					return IsValid(Feed)
						&& FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(Feed);
				}());

		if (bFeedReady)
		{
			if (FStructuralPanelAttach::SupplyAlreadyLinked(Ctx.Session(), Panel, Ports, Root, ChannelKey))
			{
				bSupplyReady = true;
			}
			else
			{
				bSupplyReady = FStructuralPanelAttach::TryLinkSupply(Ctx.Session(),
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
			Ctx.Session().ResolveSubnetFeedConnector(Root, ChannelKey);
		if (IsValid(Feed))
		{
			Ctx.Session().PromotePanelSupplyConnection(InputPower, Feed, bLocalPromoteOnly);
		}
	}

	const FName EffectiveControl =
		FStructuralPanelControlledSync::ResolveEffectiveLightControl(Ctx.Session(), Panel);
	if (Root != INDEX_NONE && !EffectiveControl.IsNone())
	{
		const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
			&& Tracked.CachedDownstreamControl == EffectiveControl;
		if (!bDownstreamUnchanged)
		{
			FStructuralPanelAttach::RestitchDownstream(Ctx.Session(),
				Panel,
				Ports,
				Root,
				EffectiveControl);
		}
		else
		{
			FStructuralPanelControlledSync::ApplyKeyedSubnet(Ctx.Session(), Panel);
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
		FStructuralPanelAttach::PromotePanelDownstreamSubnet(Ctx.Session(),
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

	FStructuralGraphSession& Session = Ctx.Session();
	const EAttachContext AttachContext = Ctx.GetAttachContext();

	if (AttachContext != EAttachContext::Toggle
		&& Session.ShouldSkipPanelCircuitEcho(Panel))
	{
		return;
	}

	if (AttachContext == EAttachContext::Toggle)
	{
		const TCHAR* SkipReason = nullptr;
		if (Session.ShouldSkipPanelCircuitEcho(Panel, &SkipReason)
			&& SkipReason
			&& FCString::Strcmp(SkipReason, TEXT("skip_feed_open")) == 0)
		{
			Session.NotePanelToggleHandled(Panel);
			return;
		}
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		return;
	}

	const FStructuralWallAnchor ParentAnchor = Session.ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = Session.ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	const FStructuralNodeId PanelId = Session.MakeNodeId(Panel);
	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(PanelId);
	Tracked.Actor = Panel;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Panel;
	Session.RegisterBuildableActor(Panel);
	Session.EnsurePanelListener(Panel);
	if (Root != INDEX_NONE)
	{
		Session.MarkBridgeEndpointRootIndexDirty();
	}

	const bool bLocalPromoteOnly =
		AttachContext != EAttachContext::RuntimePlace
		|| IsBulkLoadAttachContext(AttachContext)
		|| AttachContext == EAttachContext::WireDelta
		|| AttachContext == EAttachContext::Toggle;

	if (AttachContext == EAttachContext::WireDelta && Tracked.bPanelLinksReady
		&& Tracked.CachedPanelRoot == Root)
	{
		const FStructuralChannelKey ChannelKey = Session.ResolveChannelKeyForBuildable(Panel);
		if (FStructuralPanelAttach::SupplyAlreadyLinked(Session,
				Panel,
				Ports,
				Root,
				ChannelKey))
		{
			UFGPowerConnectionComponent* InputPower =
				FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
			if (IsValid(InputPower))
			{
				Session.PromoteDirectHiddenLinks(InputPower);
			}

			const FName EffectiveControl =
				FStructuralPanelControlledSync::ResolveEffectiveLightControl(Session, Panel);
			if (!EffectiveControl.IsNone())
			{
				FStructuralPanelControlledSync::ApplyKeyedSubnet(Session, Panel);
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
		Session.NotePanelToggleHandled(Panel);
	}
}

