// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralPanelAttach.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPanelAttach::TearDownLinks(
	AFGBuildableControlPanelHost* Panel,
	const FStructuralPanelPorts& Ports)
{
	auto Strip = [](UFGCircuitConnectionComponent* Conn)
	{
		if (UFGPowerConnectionComponent* Power = Cast<UFGPowerConnectionComponent>(Conn))
		{
			FStructuralDeviceAttach::TearDownConsumerLinks(Power);
		}
	};

	Strip(Ports.Input);
	Strip(Ports.Downstream);

	if (UFGStructuralPowerConnectionComponent* ControlBus =
			AStructuralPowerGraphSubsystem::FindPanelControlBus(Panel))
	{
		FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
	}
}

static UFGPowerConnectionComponent* ResolvePanelFeedConnector(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildable* Panel,
	int32 ComponentRoot,
	const FStructuralChannelKey& PanelKey)
{
	return Graph.ResolveSubnetFeedConnector(ComponentRoot, PanelKey);
}

bool FStructuralPanelAttach::SupplyAlreadyLinked(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	const FStructuralPanelPorts& Ports,
	int32 ComponentRoot,
	const FStructuralChannelKey& PanelKey)
{
	UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	if (!IsValid(Panel) || !IsValid(InputPower) || ComponentRoot == INDEX_NONE
		|| PanelKey.Source.IsNone())
	{
		return false;
	}

	UFGPowerConnectionComponent* Feed =
		ResolvePanelFeedConnector(Graph, Panel, ComponentRoot, PanelKey);
	return Graph.IsPanelSupplyLinkedAndLive(InputPower, Feed);
}

bool FStructuralPanelAttach::TryLinkSupply(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	const FStructuralPanelPorts& Ports,
	int32 ComponentRoot,
	const FStructuralChannelKey& PanelKey)
{
	UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	if (!IsValid(Panel) || !IsValid(InputPower) || ComponentRoot == INDEX_NONE
		|| PanelKey.Source.IsNone())
	{
		return false;
	}

	const FStructuralComponentKey CompKey = Graph.MakeComponentKeyForRoot(ComponentRoot);
	if (!CompKey.IsValid())
	{
		return false;
	}

	FStructuralChannelKey FeedKey;
	FeedKey.Tag = EStructuralChannel::Light;
	FeedKey.Source = PanelKey.Source;

	if (!FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled()
		&& !FStructuralPowerRouter::ShouldRouteChannelLink(PanelKey, FeedKey, CompKey, CompKey))
	{
		return false;
	}

	UFGPowerConnectionComponent* Feed =
		ResolvePanelFeedConnector(Graph, Panel, ComponentRoot, PanelKey);
	if (!IsValid(Feed))
	{
		return false;
	}

	if (InputPower->HasHiddenConnection(Feed))
	{
		return true;
	}

	return Graph.LinkHiddenPair(InputPower, Feed);
}

void FStructuralPanelAttach::RestitchDownstream(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	const FStructuralPanelPorts& Ports,
	int32 ComponentRoot,
	FName PanelControl)
{
	UFGPowerConnectionComponent* Downstream =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream);
	if (!IsValid(Panel) || !IsValid(Downstream) || ComponentRoot == INDEX_NONE
		|| PanelControl.IsNone()
		|| PanelControl == StructuralPowerConstants::ControlUnconfigured)
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* ControlBus = Graph.GetOrCreatePanelControlBus(Panel);
	if (!IsValid(ControlBus))
	{
		return;
	}

	FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
	FStructuralDeviceAttach::TearDownConsumerLinks(Downstream);

	if (!Graph.LinkHiddenPair(ControlBus, Downstream))
	{
		return;
	}

	Graph.EnumerateTrackedLightsOnRoot(ComponentRoot, [&](AFGBuildableLightSource* Light)
	{
		if (!IsValid(Light))
		{
			return;
		}

		const FName LightSource = Graph.ResolveSource(Light, EStructuralChannel::Light);
		if (LightSource != PanelControl)
		{
			return;
		}

		UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
		if (!IsValid(Plug))
		{
			return;
		}

		if (ControlBus->HasHiddenConnection(Plug))
		{
			return;
		}

		if (Graph.LinkHiddenPair(ControlBus, Plug))
		{
			UE_LOG(LogStructuralPower, Verbose,
				TEXT("[HALSP] panel %s linked light %s scope=site site=%d role=host path=panel_downstream"
					" control=%s"),
				*Panel->GetName(),
				*Light->GetName(),
				ComponentRoot,
				*PanelControl.ToString());
		}
	});

	FStructuralPanelControlledSync::ApplyKeyedSubnet(Graph, Panel);
}
