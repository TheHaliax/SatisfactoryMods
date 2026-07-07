// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralWireDeltaHandler.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "Processors/IStructuralPowerProcessor.h"
#include "Session/FStructuralPowerSessionSettings.h"

void FStructuralWireDeltaHandler::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

void FStructuralWireDeltaHandler::ProcessSwitchWireDelta(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !Switch->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerSessionSettings::IsPropagationEnabled()
		|| !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	FStructuralPowerTrace::LogHook(
		Switch,
		TEXT("OnCircuitsChanged"),
		TEXT("wire_refresh"),
		TEXT("switch_wire_delta"));

	const FStructuralNodeId SwitchId = AStructuralPowerGraphSubsystem::MakeNodeId(Switch);
	if (const FTrackedEndpoint* Existing = Subsystem->TrackedEndpoints.Find(SwitchId))
	{
		if (Existing->Kind == EStructuralEndpointKind::Switch && Existing->ParentId.IsValid())
		{
			FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(EAttachContext::WireDelta);
			if (IStructuralPowerProcessor* Processor =
					FStructuralPowerProcessorRegistry::Get().FindMutable(
						EStructuralEndpointKind::Switch))
			{
				Processor->OnWireDelta(Ctx, Switch);
			}
			return;
		}
	}

	Subsystem->ProcessSwitchEndpoint(Switch);
}

void FStructuralWireDeltaHandler::ProcessPanelWireDelta(AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel) || !Panel->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerSessionSettings::IsPropagationEnabled()
		|| !FStructuralPowerModConfig::IsGroupLightingEnabled()
		|| Subsystem->bBulkLoadDrainActive)
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

	if (!FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		return;
	}

	FStructuralPowerTrace::LogHook(Pole, TEXT("OnPowerConnectionChanged"), TEXT("wire_refresh"), TEXT("pole_wire_delta"));
	ProcessPoleWireDelta(Pole);
}
