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
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Routing/EStructuralChannel.h"
#include "Core/FStructuralPowerContext.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
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

	const FStructuralChannelKey ChannelKey = Ctx.Graph().ResolveChannelKeyForBuildable(Light);
	const EAttachContext AttachContext = Ctx.GetAttachContext();
	const FStructuralWallAnchor ParentAnchor = Ctx.Graph().ResolveOutletAnchor(Light);
	FStructuralNodeId ParentId;
	const int32 Root = Ctx.Graph().ResolveEndpointComponentRoot(Light, ParentAnchor, ParentId);

	const FStructuralNodeId LightId = Ctx.Graph().MakeNodeId(Light);
	FTrackedEndpoint& Tracked = Ctx.Graph().TrackedEndpoints.FindOrAdd(LightId);
	Tracked.Actor = Light;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Light;
	Ctx.Graph().RegisterBuildableActor(Light);
	if (Root != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(AttachContext))
		{
			Ctx.Graph().AddEndpointToRootIndex(Root, EStructuralEndpointKind::Light, LightId);
		}
		else
		{
			Ctx.Graph().MarkBridgeEndpointRootIndexDirty();
		}
	}

	UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
	if (!IsValid(Plug))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_plug_missing"));
		return;
	}

	FStructuralDeviceAttach::TearDownConsumerLinks(Plug);

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		FStructuralPowerTrace::LogLightConsumer(
			Light, Root, ParentAnchor.IsValid(), ChannelKey, Plug, TEXT("-"));
		return;
	}

	if (Root == INDEX_NONE || !Ctx.Graph().DoesComponentRootCarryPower(Root))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_no_component_feed"));
		FStructuralPowerTrace::LogLightConsumer(
			Light, Root, ParentAnchor.IsValid(), ChannelKey, Plug, TEXT("-"));
		return;
	}

	const bool bPanelFed = Ctx.Graph().IsPanelDownstreamLight(Root, ChannelKey);

	const bool bAttached = FStructuralDeviceAttach::TryAttachConsumer(
		Ctx.Graph(),
		Light,
		Plug,
		Root,
		ChannelKey);
	if (!bAttached)
	{
		if (Ctx.Graph().IsDirectSwitchFedLight(Root, ChannelKey)
			&& Ctx.Graph().IsSwitchFeedOpen(Root, ChannelKey.Source))
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
		if (bLocalPromoteOnly || IsBulkLoadAttachContext(AttachContext))
		{
			Ctx.Graph().PromoteDirectHiddenLinks(Plug);
		}
		else
		{
			Ctx.Graph().PromoteStructuralMeshFrom(Plug);
		}
	}

	if (Root != INDEX_NONE && !ChannelKey.Source.IsNone())
	{
		FStructuralPowerPanelProcessor::RestitchWithControlOnRoot(Ctx, Root, ChannelKey.Source);
	}

	if (bPanelFed && IsValid(Plug))
	{
		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		Plug->GetHiddenConnections(HiddenLinks);
		for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
		{
			UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw);
			if (IsValid(Other)
				&& FStructuralCircuitPromotionUtil::ComponentCarriesPower(Other))
			{
				FStructuralCircuitPromotionUtil::PromoteCircuitLink(Other, Plug);
			}
		}

		UFGPowerConnectionComponent* Seed =
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(Plug) ? Plug : nullptr;
		if (Seed)
		{
			if (bLocalPromoteOnly || IsBulkLoadAttachContext(AttachContext))
			{
				Ctx.Graph().PromoteDirectHiddenLinks(Seed);
			}
			else
			{
				Ctx.Graph().PromoteStructuralMeshFrom(Seed);
			}
		}
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

void FStructuralPowerLightProcessor::RestitchOnRoot(
	FStructuralPowerContext& Ctx,
	int32 Root)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Ctx.Graph().RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* LightIds =
		Ctx.Graph().EndpointIndex.Get(Root, EStructuralEndpointKind::Light);
	if (!LightIds || LightIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> LightIdsSnapshot = *LightIds;
	for (const FStructuralNodeId& LightId : LightIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(LightId);
		if (!Tracked)
		{
			continue;
		}

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Tracked->Actor.Get()))
		{
			Process(Ctx, Light);
		}
	}
}
