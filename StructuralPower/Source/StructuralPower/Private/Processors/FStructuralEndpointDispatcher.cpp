// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralEndpointDispatcher.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Graph/FStructuralWireDeltaHandler.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/IStructuralEndpointProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void FStructuralEndpointDispatcher::RunPlacement(
	FStructuralGraphSession& Session,
	IStructuralEndpointProcessor& Processor,
	AFGBuildable* Host,
	const bool bLocalPromoteOnly)
{
	if (!IsValid(Host))
	{
		return;
	}

	const FStructuralEndpointDescriptor& Descriptor = Processor.GetDescriptor();
	if (Descriptor.GroupGate && !Descriptor.GroupGate())
	{
		return;
	}

	FStructuralPowerContext Ctx = Session.GetProcessorContext();
	Processor.ProcessPlacement(Ctx, Host, bLocalPromoteOnly);
}

void FStructuralEndpointDispatcher::RunWireDelta(
	FStructuralGraphSession& Session,
	IStructuralEndpointProcessor& Processor,
	AFGBuildable* Host,
	const EAttachContext AttachContext)
{
	if (!IsValid(Host))
	{
		return;
	}

	FStructuralPowerContext Ctx = Session.MakeProcessorContext(AttachContext);
	Processor.OnWireDelta(Ctx, Host);
}

void FStructuralEndpointDispatcher::RunCircuitsRebuilt(
	FStructuralGraphSession& Session,
	IStructuralEndpointProcessor& Processor,
	AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return;
	}

	FStructuralPowerContext Ctx = Session.GetProcessorContext();
	Processor.OnCircuitsRebuilt(Ctx, Host);
}

void FStructuralEndpointDispatcher::RunToggleChanged(
	FStructuralGraphSession& Session,
	IStructuralEndpointProcessor& Processor,
	AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return;
	}

	FStructuralPowerContext Ctx = Session.MakeProcessorContext(EAttachContext::Toggle);
	Processor.OnToggleChanged(Ctx, Host);
}

void FStructuralEndpointDispatcher::RunTeardown(
	FStructuralGraphSession& Session,
	IStructuralEndpointProcessor& Processor,
	AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return;
	}

	FStructuralPowerContext Ctx = Session.GetProcessorContext();
	Processor.TearDown(Ctx, Host);
}

bool FStructuralEndpointDispatcher::ShouldSkipOutletDuringBulkLoad(
	const FStructuralGraphSession& Session,
	const AFGBuildable* Buildable)
{
	if (!Session.IsBulkLoadDrainActive())
	{
		return false;
	}

	if (const IStructuralEndpointProcessor* Processor =
			FStructuralEndpointCatalog::Get().Classify(Buildable))
	{
		const FStructuralEndpointDescriptor& Descriptor = Processor->GetDescriptor();
		return Descriptor.Kind == EStructuralEndpointKind::Light
			|| Descriptor.Kind == EStructuralEndpointKind::Panel;
	}

	return false;
}

void FStructuralEndpointDispatcher::DispatchPlacement(
	FStructuralGraphSession& Session,
	AFGBuildable* Buildable,
	const bool bLocalPromoteOnly)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	const IStructuralEndpointProcessor* Processor =
		FStructuralEndpointCatalog::Get().Classify(Buildable);
	if (!Processor)
	{
		return;
	}

	if (IStructuralEndpointProcessor* MutableProcessor =
			FStructuralEndpointCatalog::Get().FindMutable(Processor->GetBuildableKind()))
	{
		RunPlacement(Session, *MutableProcessor, Buildable, bLocalPromoteOnly);
	}
}

void FStructuralEndpointDispatcher::DispatchOutlet(
	FStructuralGraphSession& Session,
	AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	if (ShouldSkipOutletDuringBulkLoad(Session, Buildable))
	{
		return;
	}

	const IStructuralEndpointProcessor* Processor =
		FStructuralEndpointCatalog::Get().Classify(Buildable);
	if (!Processor)
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bridge_endpoint"));
		return;
	}

	if (IStructuralEndpointProcessor* MutableProcessor =
			FStructuralEndpointCatalog::Get().FindMutable(Processor->GetBuildableKind()))
	{
		RunPlacement(Session, *MutableProcessor, Buildable, /*bLocalPromoteOnly=*/false);
	}
}

void FStructuralEndpointDispatcher::DispatchPoleWireDelta(
	FStructuralGraphSession& Session,
	AFGBuildablePowerPole* Pole)
{
	Session.WireDelta().ProcessPoleWireDelta(Pole);
}

void FStructuralEndpointDispatcher::DispatchSwitchCircuitsRebuilt(
	FStructuralGraphSession& Session,
	AFGBuildableCircuitSwitch* Switch)
{
	Session.WireDelta().ProcessSwitchCircuitsRebuilt(Switch);
}

void FStructuralEndpointDispatcher::DispatchPanelWireDelta(
	FStructuralGraphSession& Session,
	AFGBuildableLightsControlPanel* Panel)
{
	Session.WireDelta().ProcessPanelWireDelta(Panel);
}
