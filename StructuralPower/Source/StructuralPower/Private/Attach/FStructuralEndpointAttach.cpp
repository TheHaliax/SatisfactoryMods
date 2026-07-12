// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralEndpointAttach.h"

#include "Attach/FStructuralBridgeAttach.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralSwitchBridgeStrategy.h"
#include "Processors/FStructuralSwitchMembership.h"

FStructuralBridgeAttachOutcome FStructuralEndpointAttach::AttachOnPlace(
	FStructuralPowerContext& Ctx,
	const FStructuralBridgeAttachRequest& Request)
{
	return FStructuralBridgeAttach::AttachOnPlace(Ctx, Request);
}

bool FStructuralEndpointAttach::AttachConsumer(
	FStructuralPowerContext& Ctx,
	AFGBuildable* Host,
	const bool bLocalPromoteOnly)
{
	if (AFGBuildableLightSource* Light = FStructuralPowerBuildableCasts::AsLight(Host))
	{
		FStructuralPowerLightProcessor::Process(Ctx, Light, bLocalPromoteOnly);
		return true;
	}
	return false;
}

bool FStructuralEndpointAttach::AttachRouter(
	FStructuralPowerContext& Ctx,
	AFGBuildable* Host,
	const bool bLocalPromoteOnly)
{
	if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Host))
	{
		FStructuralPowerPanelProcessor::Process(Ctx, Panel, bLocalPromoteOnly);
		return true;
	}
	return false;
}

bool FStructuralEndpointAttach::AttachToggleBridge(FStructuralPowerContext& Ctx, AFGBuildable* Host)
{
	if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Host))
	{
		FStructuralSwitchMembership::Apply(Ctx, Switch);
		FStructuralSwitchBridgeStrategy::Apply(Ctx, Switch);
		return true;
	}
	return false;
}

bool FStructuralEndpointAttach::RunStrategy(
	FStructuralPowerContext& Ctx,
	AFGBuildable* Host,
	const EStructuralAttachStrategy Strategy,
	const bool bLocalPromoteOnly)
{
	switch (Strategy)
	{
	case EStructuralAttachStrategy::Bridge:
	{
		FStructuralBridgeAttachRequest Request;
		Request.Host = Host;
		if (FStructuralPowerBuildableCasts::AsPole(Host))
		{
			Request.Kind = EStructuralEndpointKind::Pole;
			Request.bUsePoleRootResolver = true;
		}
		else
		{
			Request.Kind = EStructuralEndpointKind::Storage;
		}
		return AttachOnPlace(Ctx, Request).OutletBus != nullptr;
	}
	case EStructuralAttachStrategy::ToggleBridge:
		return AttachToggleBridge(Ctx, Host);
	case EStructuralAttachStrategy::Consumer:
		return AttachConsumer(Ctx, Host, bLocalPromoteOnly);
	case EStructuralAttachStrategy::Router:
		return AttachRouter(Ctx, Host, bLocalPromoteOnly);
	default:
		return false;
	}
}
