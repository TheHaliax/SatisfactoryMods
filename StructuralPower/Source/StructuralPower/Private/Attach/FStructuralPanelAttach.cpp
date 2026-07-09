// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralPanelAttach.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
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

void FStructuralPanelAttach::TearDownDownstreamLinks(
	AFGBuildableControlPanelHost* Panel,
	const FStructuralPanelPorts& Ports)
{
	if (UFGPowerConnectionComponent* Downstream =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream))
	{
		FStructuralDeviceAttach::TearDownConsumerLinks(Downstream);
	}

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
	return Graph.IsPanelSupplyLinked(InputPower, Feed);
}

bool FStructuralPanelAttach::TryLinkSupply(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	const FStructuralPanelPorts& Ports,
	int32 ComponentRoot,
	const FStructuralChannelKey& PanelKey,
	bool bMeshOnlyLinks)
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

	UFGPowerConnectionComponent* Feed =
		ResolvePanelFeedConnector(Graph, Panel, ComponentRoot, PanelKey);
	if (!IsValid(Feed))
	{
		return false;
	}

	if (InputPower->HasHiddenConnection(Feed))
	{
		if (FStructuralPowerTrace::IsEnabled())
		{
			FStructuralPowerTrace::LogConnector(TEXT("panel_supply"), Panel, InputPower);
			FStructuralPowerTrace::LogConnector(TEXT("panel_feed"), Panel, Feed);
		}
		return true;
	}

	const bool bLinked = Graph.LinkHiddenPair(InputPower, Feed, /*bPromoteCircuit=*/!bMeshOnlyLinks);
	if (FStructuralPowerTrace::IsEnabled())
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] panel supply link %s -> %s ok=%d root=%d source=%s"),
			*Panel->GetName(),
			*Feed->GetName(),
			bLinked ? 1 : 0,
			ComponentRoot,
			*PanelKey.Source.ToString());
		FStructuralPowerTrace::LogConnector(TEXT("panel_supply"), Panel, InputPower);
		FStructuralPowerTrace::LogConnector(TEXT("panel_feed"), Panel, Feed);
	}
	return bLinked;
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
	if (!IsValid(Panel) || !IsValid(Downstream) || ComponentRoot == INDEX_NONE)
	{
		return;
	}

	const FName EffectiveControl =
		FStructuralPanelControlledSync::ResolveEffectiveLightControl(Graph, Panel);
	if (EffectiveControl.IsNone())
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
		if (LightSource != EffectiveControl)
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
			if (FStructuralPowerTrace::IsEnabled())
			{
				UE_LOG(LogStructuralPower, Log,
					TEXT("[HALSP] panel %s linked light %s kind=%s scope=%s site=%d role=%s"
						" path=panel_downstream control=%s"),
					*Panel->GetName(),
					*Light->GetName(),
					StructuralEndpointKindToString(EStructuralEndpointKind::Light),
					StructuralPowerScopeToString(EStructuralPowerScope::Site),
					ComponentRoot,
					StructuralPowerRoleToString(EStructuralPowerRole::Host),
					*EffectiveControl.ToString());
				FStructuralPowerTrace::LogConnector(TEXT("panel_control_bus"), Panel, ControlBus);
				FStructuralPowerTrace::LogConnector(TEXT("light_plug"), Light, Plug);
			}
		}
	});

	(void)PanelControl;
	FStructuralPanelControlledSync::ApplyKeyedSubnet(Graph, Panel);
}

void FStructuralPanelAttach::PromotePanelDownstreamSubnet(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	const FStructuralPanelPorts& Ports,
	UFGPowerConnectionComponent* InputPower)
{
	if (!IsValid(Panel) || !IsValid(InputPower)
		|| !FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower))
	{
		return;
	}

	Graph.PromoteStructuralMeshFrom(InputPower);

	UFGStructuralPowerConnectionComponent* ControlBus = Graph.GetOrCreatePanelControlBus(Panel);
	if (IsValid(ControlBus) && FStructuralCircuitPromotionUtil::ComponentOnCircuit(ControlBus))
	{
		Graph.PromoteStructuralMeshFrom(ControlBus);
	}
	(void)Ports;
}
