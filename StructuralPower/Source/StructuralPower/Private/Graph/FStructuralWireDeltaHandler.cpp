// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralWireDeltaHandler.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Connection/FStructuralSwitchConnectionPoint.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "Processors/IStructuralPowerProcessor.h"

void FStructuralWireDeltaHandler::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

static uint8 BuildSwitchWireSignature(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return 0;
	}

	uint8 Signature = 0;
	if (UFGPowerConnectionComponent* Conn0 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection0()))
	{
		if (Conn0->GetNumConnections() > 0)
		{
			Signature |= 0x1;
		}
	}

	if (UFGPowerConnectionComponent* Conn1 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection1()))
	{
		if (Conn1->GetNumConnections() > 0)
		{
			Signature |= 0x2;
		}
	}

	return Signature;
}

void FStructuralWireDeltaHandler::ProcessSwitchWireDelta(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !Switch->HasAuthority())
	{
		return;
	}

	if (Subsystem->ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	const FStructuralNodeId SwitchId = AStructuralPowerGraphSubsystem::MakeNodeId(Switch);
	FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(SwitchId);
	const uint8 WireSignature = BuildSwitchWireSignature(Switch);
	if (Tracked.CachedSwitchWireSignature == WireSignature)
	{
		return;
	}
	Tracked.CachedSwitchWireSignature = WireSignature;

	FStructuralPowerTrace::LogHook(
		Switch,
		TEXT("OnCircuitsChanged"),
		TEXT("wire_refresh"),
		TEXT("switch_wire_delta"));

	FStructuralSwitchConnectionPoint(*Subsystem, Switch).OnWireOrGateChanged(EAttachContext::WireDelta);
}

void FStructuralWireDeltaHandler::ProcessPanelWireDelta(AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel) || !Panel->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled()
		|| Subsystem->ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	if (Subsystem->ShouldSkipPanelCircuitEcho(Panel))
	{
		return;
	}

	FStructuralPanelPorts Ports;
	const bool bPortsResolved = FStructuralPanelPortResolver::Resolve(Panel, Ports);
	const FStructuralNodeId PanelId = AStructuralPowerGraphSubsystem::MakeNodeId(Panel);
	const FTrackedEndpoint* Existing = Subsystem->TrackedEndpoints.Find(PanelId);
	if (!bPortsResolved
		&& (!Existing
			|| Existing->Kind != EStructuralEndpointKind::Panel
			|| !Existing->ParentId.IsValid()))
	{
		return;
	}

	FStructuralPowerTrace::LogHook(
		Panel,
		TEXT("OnCircuitsRebuilt"),
		TEXT("wire_refresh"),
		TEXT("panel_wire_delta"));

	if (Existing)
	{
		if (Existing->Kind == EStructuralEndpointKind::Panel && Existing->ParentId.IsValid())
		{
			FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(EAttachContext::WireDelta);
			if (IStructuralPowerProcessor* Processor =
					FStructuralPowerProcessorRegistry::Get().FindMutable(
						EStructuralEndpointKind::Panel))
			{
				Processor->OnWireDelta(Ctx, Panel);
			}
			return;
		}
	}

	Subsystem->ProcessPanelEndpoint(Panel, /*bLocalPromoteOnly=*/true);
}

void FStructuralWireDeltaHandler::ProcessPoleWireDelta(AFGBuildablePowerPole* Pole)
{
	FStructuralPowerContext Ctx = Subsystem->GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Pole))
	{
		Processor->OnWireDelta(Ctx, Pole);
	}
}

void FStructuralWireDeltaHandler::ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole) || !Pole->HasAuthority())
	{
		return;
	}

	FStructuralPowerTrace::LogHook(Pole, TEXT("OnPowerConnectionChanged"), TEXT("wire_refresh"), TEXT("pole_wire_delta"));
	ProcessPoleWireDelta(Pole);
}
