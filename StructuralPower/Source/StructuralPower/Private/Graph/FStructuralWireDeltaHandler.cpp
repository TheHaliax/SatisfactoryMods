// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralWireDeltaHandler.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Attach/FStructuralBridgeAttach.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Processors/IStructuralPowerProcessor.h"

void FStructuralWireDeltaHandler::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

void FStructuralWireDeltaHandler::ProcessSwitchCircuitsRebuilt(AFGBuildableCircuitSwitch* Switch)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("circuitsRebuilt.switch"),
		IsValid(Switch) ? Switch->GetName() : TEXT("null"));

	if (!IsValid(Switch) || !Switch->HasAuthority())
	{
		return;
	}

	if (Subsystem->ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	if (Subsystem->ShouldSkipSwitchCircuitEcho(Switch))
	{
		return;
	}

	const FStructuralNodeId SwitchId = AStructuralPowerGraphSubsystem::MakeNodeId(Switch);
	const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(SwitchId);
	if (!Tracked || !Tracked->ParentId.IsValid())
	{
		return;
	}

	const int32 Root = Subsystem->StructureGraph.FindRoot(Tracked->ParentId);
	FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(EAttachContext::WireDelta, Root);

	FStructuralPowerSwitchProcessor::OnCircuitsRebuilt(Ctx, Switch);
	FStructuralPowerSwitchProcessor::EnsureListener(Ctx, Switch);
	Subsystem->NoteSwitchCircuitEchoProcessed(Switch);
}

void FStructuralWireDeltaHandler::ProcessSwitchWireDelta(AFGBuildableCircuitSwitch* Switch)
{
	ProcessSwitchCircuitsRebuilt(Switch);
}

void FStructuralWireDeltaHandler::ProcessPanelWireDelta(AFGBuildableLightsControlPanel* Panel)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("wireDelta.panel"),
		IsValid(Panel) ? Panel->GetName() : TEXT("null"));

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
	if (!IsValid(Pole) || !Pole->HasAuthority())
	{
		return;
	}

	if (Subsystem->ShouldDeferCircuitDrivenRefresh())
	{
		Subsystem->EnqueuePlacement(Pole, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		return;
	}

	if (!FStructuralBridgeAttach::HasPlacementMembership(
			*Subsystem,
			Pole,
			EStructuralEndpointKind::Pole))
	{
		Subsystem->ProcessPoleEndpoint(Pole);
		return;
	}

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
