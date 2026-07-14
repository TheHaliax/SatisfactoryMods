// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerLightProcessor.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Core/EAttachContext.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "Routing/EStructuralChannel.h"
#include "Core/FStructuralGraphSession.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Core/FStructuralPowerContext.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPowerLightProcessor::TearDown(
	FStructuralPowerContext& Ctx,
	AFGBuildableLightSource* Light)
{
	if (!IsValid(Light))
	{
		return;
	}

	if (UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light))
	{
		FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
	}
}

void FStructuralPowerLightProcessor::Process(
	FStructuralPowerContext& Ctx,
	AFGBuildableLightSource* Light,
	bool bLocalPromoteOnly)
{
	if (!IsValid(Light))
	{
		return;
	}

	const FStructuralChannelKey ChannelKey = Ctx.Session().ResolveChannelKeyForBuildable(Light);
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const FStructuralWallAnchor ParentAnchor = Ctx.Session().ResolveOutletAnchor(Light);
	FStructuralNodeId ParentId;
	const int32 Root = Ctx.Session().ResolveEndpointComponentRoot(Light, ParentAnchor, ParentId);

	const FStructuralNodeId LightId = Ctx.Session().MakeNodeId(Light);
	FTrackedEndpoint& Tracked = Ctx.Session().TrackedEndpoints().FindOrAdd(LightId);
	Tracked.Actor = Light;
	Tracked.MountParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Light;
	Ctx.Session().RegisterBuildableActor(Light);
	if (Root != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(AttachContext))
		{
			Ctx.Session().AddEndpointToRootIndex(Root, EStructuralEndpointKind::Light, LightId);
		}
		else
		{
			Ctx.Session().MarkBridgeEndpointRootIndexDirty();
		}
	}

	UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
	if (!IsValid(Plug))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_plug_missing"));
		return;
	}

	if (FStructuralPowerTransferGate::IsConsumerVanillaWired(Plug))
	{
		Tracked.bStructuralPowerTransferActive = false;
		FStructuralPowerTrace::LogLightConsumer(
			Light, Root, ParentAnchor.IsValid(), ChannelKey, Plug, TEXT("vanilla_wired"));
		return;
	}

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
		Tracked.bStructuralPowerTransferActive = false;
		FStructuralPowerTrace::LogLightConsumer(
			Light, Root, ParentAnchor.IsValid(), ChannelKey, Plug, TEXT("lighting_disabled"));
		return;
	}

	const bool bPanelFed = Root != INDEX_NONE
		&& Ctx.Session().IsPanelDownstreamLight(Root, ChannelKey);

	if (Root == INDEX_NONE
		|| (!bPanelFed
			&& !Ctx.Session().DoesComponentRootCarryPower(Root)
			&& !Ctx.Session().DoesSiteStructuralBusCarryPower(Root)))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_no_component_feed"));
		FStructuralPowerTrace::LogLightConsumer(
			Light, Root, ParentAnchor.IsValid(), ChannelKey, Plug, TEXT("-"));
		return;
	}

	const bool bWireOrToggle =
		FStructuralPowerTransferGate::IsBridgeWireOrToggleContext(AttachContext);
	if (!bWireOrToggle && !bPanelFed)
	{
		FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
	}

	const bool bAttached = FStructuralDeviceAttach::TryAttachConsumer(Ctx.Session(),
		Light,
		Plug,
		Root,
		ChannelKey);
	if (!bAttached)
	{
		if (Ctx.Session().IsDirectSwitchFedLight(Root, ChannelKey)
			&& Ctx.Session().IsSwitchFeedOpen(Root, ChannelKey.Source))
		{
			FStructuralPowerTrace::LogPlacementSkip(
				Light,
				TEXT("light_switch_open"),
				ELogVerbosity::Verbose);
		}
		else if (bPanelFed)
		{
			FStructuralPowerTrace::LogPlacementSkip(
				Light,
				TEXT("light_panel_fed"),
				ELogVerbosity::Verbose);
		}
		else
		{
			FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_attach_failed"));
		}
	}
	if (bAttached)
	{
		Tracked.bStructuralPowerTransferActive = true;
		Ctx.Session().Circuit().PromoteDirectHiddenLinks(Plug);
	}

	if (bPanelFed && IsValid(Plug))
	{
		Ctx.Session().Circuit().PromoteDirectHiddenLinks(Plug);
	}

	const TCHAR* PathLabel = bPanelFed ? TEXT("panel_downstream") : (bAttached ? TEXT("direct") : TEXT("-"));
	FStructuralPowerTrace::LogLightConsumer(
		Light,
		Root,
		ParentAnchor.IsValid(),
		ChannelKey,
		Plug,
		PathLabel);
}
