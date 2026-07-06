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
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

void FStructuralPowerLightProcessor::TearDown(
	AStructuralPowerGraphSubsystem& Graph,
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
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightSource* Light,
	bool bLocalPromoteOnly)
{
	if (!IsValid(Light))
	{
		return;
	}

	const FStructuralChannelKey ChannelKey = Graph.ResolveChannelKeyForBuildable(Light);
	const EAttachContext AttachContext = Graph.GetCurrentAttachContext();
	const FStructuralWallAnchor ParentAnchor = Graph.ResolveOutletAnchor(Light);
	FStructuralNodeId ParentId;
	const int32 Root = Graph.ResolveEndpointComponentRoot(Light, ParentAnchor, ParentId);

	const FStructuralNodeId LightId = Graph.MakeNodeId(Light);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(LightId);
	Tracked.Actor = Light;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Light;
	Graph.RegisterBuildableActor(Light);
	if (Root != INDEX_NONE)
	{
		if (IsBulkLoadAttachContext(AttachContext))
		{
			Graph.AddEndpointToRootIndex(Root, EStructuralEndpointKind::Light, LightId);
		}
		else
		{
			Graph.MarkBridgeEndpointRootIndexDirty();
		}
	}

	UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
	if (!IsValid(Plug))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_plug_missing"));
		return;
	}

	FStructuralDeviceAttach::TearDownConsumerLinks(Plug);

	auto LogLightOutlet = [&](int32 Powered, int32 BusCircuit, const TCHAR* Path)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] light %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s"
				" source=%s control=%s wirePort=- path=%s"),
			*Light->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			BusCircuit,
			Powered,
			StructuralChannelToString(ChannelKey.Tag),
			*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
			*FStructuralPowerTrace::FormatControlForTrace(ChannelKey),
			Path);
	};

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		LogLightOutlet(0, INDEX_NONE, TEXT("-"));
		return;
	}

	if (Root == INDEX_NONE || !Graph.DoesComponentRootCarryPower(Root))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_no_component_feed"));
		LogLightOutlet(0, INDEX_NONE, TEXT("-"));
		return;
	}

	const bool bPanelFed = Graph.IsPanelDownstreamLight(Root, ChannelKey);

	const bool bAttached = FStructuralDeviceAttach::TryAttachConsumer(
		Graph,
		Light,
		Plug,
		Root,
		ChannelKey);
	if (!bAttached)
	{
		if (Graph.IsDirectSwitchFedLight(Root, ChannelKey)
			&& Graph.IsSwitchFeedOpen(Root, ChannelKey.Source))
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
			Graph.PromoteDirectHiddenLinks(Plug);
		}
		else
		{
			Graph.PromoteStructuralMeshFrom(Plug);
		}
	}

	if (Root != INDEX_NONE && !ChannelKey.Source.IsNone())
	{
		FStructuralPowerPanelProcessor::RestitchWithControlOnRoot(
			Graph,
			Root,
			ChannelKey.Source,
			AttachContext);
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
				Graph.PromoteDirectHiddenLinks(Seed);
			}
			else
			{
				Graph.PromoteStructuralMeshFrom(Seed);
			}
		}
	}

	const int32 BusCircuit = Plug->GetCircuitID();
	const int32 Powered = FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(Plug) ? 1 : 0;
	LogLightOutlet(
		Powered,
		BusCircuit,
		bPanelFed ? TEXT("panel_downstream") : (bAttached ? TEXT("direct") : TEXT("-")));
}

void FStructuralPowerLightProcessor::RestitchOnRoot(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Root,
	EAttachContext /*AttachContext*/)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Graph.RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* LightIds =
		Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Light);
	if (!LightIds || LightIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> LightIdsSnapshot = *LightIds;
	for (const FStructuralNodeId& LightId : LightIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(LightId);
		if (!Tracked)
		{
			continue;
		}

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Tracked->Actor.Get()))
		{
			Process(Graph, Light);
		}
	}
}
