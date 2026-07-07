// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Reconcile/FStructuralPowerRestitch.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralPowerGeneratorProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "Processors/IStructuralPowerProcessor.h"
#include "Reconcile/FStructuralSiteBusMesh.h"
#include "Routing/FStructuralPowerRouter.h"
#include "StructuralPowerLog.h"

void FStructuralPowerRestitch::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

bool FStructuralPowerRestitch::ShouldEndpointParticipateInRestitch(
	AFGBuildable* Host,
	EStructuralEndpointKind Kind)
{
	if (!IsValid(Host))
	{
		return false;
	}

	if (Kind == EStructuralEndpointKind::Pole)
	{
		return true;
	}

	if (Kind != EStructuralEndpointKind::Switch
		|| !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return false;
	}

	const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Host);
	if (!IsValid(Switch) || !Switch->IsBridgeActive())
	{
		return false;
	}

	return FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch);
}

bool FStructuralPowerRestitch::ShouldMeshEndpoints(
	AFGBuildable* HostA,
	AFGBuildable* HostB,
	int32 ComponentRoot) const
{
	if (!FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		return true;
	}

	const FStructuralChannelKey KeyA = Subsystem->IdOps.ResolveChannelKeyForBuildable(HostA);
	const FStructuralChannelKey KeyB = Subsystem->IdOps.ResolveChannelKeyForBuildable(HostB);
	const FStructuralComponentKey CompKey = Subsystem->IdOps.MakeComponentKeyForRoot(ComponentRoot);

	if (KeyA.Tag != EStructuralChannel::Switch && KeyB.Tag != EStructuralChannel::Switch)
	{
		return true;
	}

	const bool bSwitchOnA = KeyA.Tag == EStructuralChannel::Switch;
	const FName SwitchControl = bSwitchOnA ? KeyA.Control : KeyB.Control;
	const FName DeviceSource = bSwitchOnA ? KeyB.Source : KeyA.Source;
	const EStructuralChannel PeerTag = bSwitchOnA ? KeyB.Tag : KeyA.Tag;

	if (PeerTag == EStructuralChannel::Structure)
	{
		return true;
	}

	return FStructuralPowerRouter::ShouldRouteSwitchGate(SwitchControl, DeviceSource, CompKey, CompKey);
}

void FStructuralPowerRestitch::RestitchComponent(
	int32 Root,
	bool bTearDownFirst,
	EAttachContext AttachContext)
{
	if (Root == INDEX_NONE)
	{
		return;
	}

	int32 IndexedEndpoints = 0;
	Subsystem->EndpointIndex.ForEachOnRoot(Root, [&](const FStructuralNodeId&)
	{
		++IndexedEndpoints;
	});

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] restitch root=%d tearDown=%d attach=%s indexedEndpoints=%d"),
		Root,
		bTearDownFirst ? 1 : 0,
		AttachContextToString(AttachContext),
		IndexedEndpoints);

	FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(AttachContext, Root);
	FStructuralPowerSwitchProcessor::StripInactiveLinksOnRoot(Ctx, Root);
	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();

	FStructuralSiteBusMesh::Remesh(
		Root,
		bTearDownFirst,
		Subsystem->EndpointIndex,
		[this](const FStructuralNodeId& NodeId) -> const FTrackedEndpoint*
		{
			return Subsystem->TrackedEndpoints.Find(NodeId);
		},
		[this](AFGBuildable* Host, EStructuralEndpointKind Kind)
		{
			return ShouldEndpointParticipateInRestitch(Host, Kind);
		},
		[this](AFGBuildable* Host)
		{
			return Subsystem->GetOrCreateBusConnector(Host);
		},
		[this](AFGBuildable* Host, UFGStructuralPowerConnectionComponent* Bus)
		{
			Subsystem->CircuitOps.LinkBusToVisibleConnections(Host, Bus);
		},
		[this](AFGBuildable* HostA, AFGBuildable* HostB, int32 ComponentRoot)
		{
			return ShouldMeshEndpoints(HostA, HostB, ComponentRoot);
		},
		[this](UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B)
		{
			return Subsystem->CircuitOps.LinkHiddenPair(A, B);
		},
		[](const UFGPowerConnectionComponent* Component)
		{
			return FStructuralCircuitPromotionUtil::ComponentCarriesPower(Component);
		},
		[this](UFGStructuralPowerConnectionComponent* StartHidden)
		{
			return Subsystem->CircuitOps.FindPoweredHiddenReachable(StartHidden);
		},
		[this](UFGStructuralPowerConnectionComponent* Seed)
		{
			Subsystem->CircuitOps.PromoteStructuralMeshFrom(Seed);
		});
}

void FStructuralPowerRestitch::ReEnergizeComponentRoots(
	const TArray<int32>& Roots,
	bool bTearDownFirst,
	EAttachContext AttachContext)
{
	TSet<int32> Done;
	for (int32 Root : Roots)
	{
		if (Root == INDEX_NONE || Done.Contains(Root))
		{
			continue;
		}
		Done.Add(Root);
		if (AttachContext != EAttachContext::WireDelta)
		{
			RestitchComponent(Root, bTearDownFirst, AttachContext);
		}
		RestitchLightEndpointsForRoot(Root, AttachContext);
		RestitchPanelEndpointsForRoot(Root, AttachContext);
		if (AttachContext == EAttachContext::WireDelta)
		{
			RestitchKeyedSubnetsAfterMeshFeedRefresh(Root, AttachContext);
		}
		if (FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled())
		{
			FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(AttachContext, Root);
			FStructuralPowerGeneratorProcessor::RestitchOnRoot(Ctx, Root);
		}
	}
}

void FStructuralPowerRestitch::RestitchKeyedSubnetsAfterMeshFeedRefresh(
	int32 Root,
	EAttachContext AttachContext)
{
	if (Root == INDEX_NONE
		|| !FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		return;
	}

	if (FStructuralPowerTrace::IsEnabled())
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] keyed subnet refresh root=%d attach=%s"),
			Root,
			AttachContextToString(AttachContext));
	}

	FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(AttachContext, Root);
	if (IsBulkLoadAttachContext(AttachContext))
	{
		FStructuralPowerSwitchProcessor::RestitchActiveKeyedConsumersOnRoot(Ctx, Root);
	}
	else if (FStructuralPowerTransferGate::IsBridgeWireOrToggleContext(AttachContext))
	{
		FStructuralPowerTransferGate::RefreshKeyedTransferOnRoot(Ctx, Root);
	}
	else
	{
		FStructuralPowerSwitchProcessor::RestitchActiveKeyedConsumersOnRoot(Ctx, Root);
	}
}

void FStructuralPowerRestitch::RestitchPanelEndpointsForRoot(
	int32 Root,
	EAttachContext AttachContext)
{
	FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(AttachContext, Root);
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Panel))
	{
		Processor->RestitchOnRoot(Ctx, Root);
	}
}

void FStructuralPowerRestitch::RestitchPanelsWithControlOnRoot(int32 Root, FName ControlId)
{
	FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(Subsystem->GetCurrentAttachContext(), Root);
	FStructuralPowerPanelProcessor::RestitchWithControlOnRoot(Ctx, Root, ControlId);
}

void FStructuralPowerRestitch::RestitchLightEndpointsForRoot(
	int32 Root,
	EAttachContext AttachContext)
{
	FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(AttachContext, Root);
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Light))
	{
		Processor->RestitchOnRoot(Ctx, Root);
	}
}
