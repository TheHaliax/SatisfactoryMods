// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralSwitchBridgeStrategy.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

namespace
{
static void StrategyOpenDirectedBridgeLeg(
	UFGStructuralPowerConnectionComponent* SourceBus,
	UFGCircuitConnectionComponent* Leg)
{
	if (!IsValid(SourceBus) || !IsValid(Leg) || SourceBus == Leg)
	{
		return;
	}

	if (SourceBus->HasHiddenConnection(Leg))
	{
		SourceBus->RemoveHiddenConnection(Leg);
	}
}

static void StrategyCloseDirectedBridgeLeg(
	UFGStructuralPowerConnectionComponent* SourceBus,
	UFGPowerConnectionComponent* Leg)
{
	if (!IsValid(SourceBus) || !IsValid(Leg) || SourceBus == Leg)
	{
		return;
	}

	if (!SourceBus->HasHiddenConnection(Leg))
	{
		SourceBus->AddHiddenConnection(Leg);
	}
}

static void StrategyCloseBypassZeroWireOnLegs(
	UFGStructuralPowerConnectionComponent* SourceBus,
	AFGBuildableCircuitSwitch* Switch)
{
	StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
	StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());

	if (UFGPowerConnectionComponent* Conn0 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection0()))
	{
		StrategyCloseDirectedBridgeLeg(SourceBus, Conn0);
	}

	if (UFGPowerConnectionComponent* Conn1 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection1()))
	{
		StrategyCloseDirectedBridgeLeg(SourceBus, Conn1);
	}
}

static int32 StrategyCountVanillaWirePorts(AFGBuildableCircuitSwitch* Switch)
{
	int32 Count = 0;
	if (const UFGPowerConnectionComponent* Conn0 =
			Cast<UFGPowerConnectionComponent>(Switch->GetConnection0()))
	{
		if (Conn0->GetNumConnections() > 0)
		{
			++Count;
		}
	}

	if (const UFGPowerConnectionComponent* Conn1 =
			Cast<UFGPowerConnectionComponent>(Switch->GetConnection1()))
	{
		if (Conn1->GetNumConnections() > 0)
		{
			++Count;
		}
	}

	return Count;
}
} // namespace

void FStructuralSwitchBridgeStrategy::ApplyMembership(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
	const bool bConfigured = FStructuralPowerSwitchProcessor::HasAssignedControl(Ctx, Switch);
	const bool bSwitchOn = Switch->IsBridgeActive();
	const int32 WirePortCount = StrategyCountVanillaWirePorts(Switch);

	FStructuralGraphSession& Session = Ctx.Session();
	UFGStructuralPowerConnectionComponent* SourceBus = Session.FindBusConnector(Switch);
	if (!IsValid(SourceBus))
	{
		SourceBus = Session.GetOrCreateBusConnector(Switch);
	}

	if (!IsValid(SourceBus))
	{
		return;
	}

	if (!bSwitchOn)
	{
		DisarmDirectedPair(Ctx, Switch);
		return;
	}

	if (bConfigured || bWired)
	{
		const bool bMeshOnlyLinks = WirePortCount >= 2;
		Session.LinkBusToVisibleConnectionsLocal(Switch, SourceBus, bMeshOnlyLinks);
		SyncDirectedBridgePair(Ctx, Switch);

		if (bWired && WirePortCount < 2)
		{
			Session.PromoteOutletBusIfPowered(SourceBus, /*bLocalPromoteOnly=*/true);
		}

		return;
	}

	Session.LinkBusToVisibleConnectionsLocal(Switch, SourceBus, /*bMeshOnlyLinks=*/true);
	StrategyCloseBypassZeroWireOnLegs(SourceBus, Switch);
}

void FStructuralSwitchBridgeStrategy::ApplyToggle(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	if (!Switch->IsBridgeActive())
	{
		DisarmDirectedPair(Ctx, Switch);
		return;
	}

	const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
	const bool bConfigured = FStructuralPowerSwitchProcessor::HasAssignedControl(Ctx, Switch);
	if (bWired || bConfigured)
	{
		SyncDirectedBridgePair(Ctx, Switch);
		return;
	}

	FStructuralGraphSession& Session = Ctx.Session();
	UFGStructuralPowerConnectionComponent* SourceBus = Session.FindBusConnector(Switch);
	if (!IsValid(SourceBus))
	{
		SourceBus = Session.GetOrCreateBusConnector(Switch);
	}

	if (!IsValid(SourceBus))
	{
		return;
	}

	Session.LinkBusToVisibleConnectionsLocal(Switch, SourceBus, /*bMeshOnlyLinks=*/true);
	StrategyCloseBypassZeroWireOnLegs(SourceBus, Switch);
}

void FStructuralSwitchBridgeStrategy::ApplyWireEcho(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("wireDelta.switch"),
		IsValid(Switch) ? Switch->GetName() : TEXT("null"));

	if (!IsValid(Switch))
	{
		return;
	}

	FStructuralGraphSession& Session = Ctx.Session();
	UFGStructuralPowerConnectionComponent* OutletBus = Session.FindBusConnector(Switch);
	if (!IsValid(OutletBus))
	{
		OutletBus = Session.GetOrCreateBusConnector(Switch);
	}

	if (!IsValid(OutletBus))
	{
		return;
	}

	if (!Session.HasBridgeBusPeerMesh(OutletBus))
	{
		FStructuralPowerSwitchProcessor::ApplyStructureMembership(Ctx, Switch);
		return;
	}

	Session.LinkBusToVisibleConnectionsLocal(Switch, OutletBus, /*bMeshOnlyLinks=*/true);

	if (FStructuralCircuitPromotionUtil::ComponentOnCircuit(OutletBus))
	{
		Session.PromoteDirectHiddenLinks(OutletBus);
	}

	if (Switch->IsBridgeActive())
	{
		SyncDirectedBridgePair(Ctx, Switch);
	}
	else
	{
		DisarmDirectedPair(Ctx, Switch);
	}
}

void FStructuralSwitchBridgeStrategy::Apply(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	ApplyMembership(Ctx, Switch);
}

void FStructuralSwitchBridgeStrategy::DisarmDirectedPair(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralPowerSwitchProcessor::DisarmDirectedPair(Ctx, Switch);
}

void FStructuralSwitchBridgeStrategy::SyncDirectedBridgePair(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralPowerSwitchProcessor::SyncDirectedBridgePair(Ctx, Switch);
}
