// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/FStructuralGraphSession.h"

#include "Processors/FStructuralEndpointDispatcher.h"
#include "Save/FStructuralEndpointIdRegistry.h"
#include "Graph/FStructuralGraphBootstrap.h"
#include "Graph/FStructuralGraphBulkDrain.h"
#include "Graph/FStructuralGraphCircuitEcho.h"
#include "Graph/FStructuralGraphRemoval.h"
#include "Graph/FStructuralGraphStructureIngress.h"
#include "Graph/FStructuralBusMemberSpatialIndex.h"
#include "Subsystems/UStructuralPowerFactoryTickHandler.h"

FStructuralGraphSession::FStructuralGraphSession(AStructuralPowerGraphSubsystem& InOwner)
	: OwnerPtr(&InOwner)
{
}

AStructuralPowerGraphSubsystem& FStructuralGraphSession::Owner()
{
	check(OwnerPtr);
	return *OwnerPtr;
}

const AStructuralPowerGraphSubsystem& FStructuralGraphSession::Owner() const
{
	check(OwnerPtr);
	return *OwnerPtr;
}

UWorld* FStructuralGraphSession::GetWorld() const
{
	return OwnerPtr ? OwnerPtr->GetWorld() : nullptr;
}

FStructuralConnectivityGraph& FStructuralGraphSession::StructureGraph()
{
	return Owner().StructureGraph;
}

const FStructuralConnectivityGraph& FStructuralGraphSession::StructureGraph() const
{
	return Owner().StructureGraph;
}

FStructuralLightweightIndex& FStructuralGraphSession::LightweightIndex()
{
	return Owner().LightweightIndex;
}

FStructuralEndpointIndex& FStructuralGraphSession::EndpointIndex()
{
	return Owner().EndpointIndex;
}

TMap<FStructuralNodeId, FTrackedEndpoint>& FStructuralGraphSession::TrackedEndpoints()
{
	return Owner().TrackedEndpoints;
}

const TMap<FStructuralNodeId, FTrackedEndpoint>& FStructuralGraphSession::TrackedEndpoints() const
{
	return Owner().TrackedEndpoints;
}

FStructuralGraphCircuitOps& FStructuralGraphSession::Circuit()
{
	return Owner().CircuitOps;
}

FStructuralGraphIdOps& FStructuralGraphSession::Ids()
{
	return Owner().IdOps;
}

FStructuralPowerReconcile& FStructuralGraphSession::Reconcile()
{
	return Owner().ReconcileOps;
}

FStructuralPowerRestitch& FStructuralGraphSession::Restitch()
{
	return Owner().RestitchOps;
}

FStructuralBridgeRootIndex& FStructuralGraphSession::BridgeRootIndex()
{
	return Owner().BridgeRootIndex;
}

FStructuralGraphBootstrap& FStructuralGraphSession::Bootstrap()
{
	return Owner().BootstrapOps;
}

FStructuralGraphStructureIngress& FStructuralGraphSession::StructureIngress()
{
	return Owner().StructureIngressOps;
}

FStructuralGraphBulkDrain& FStructuralGraphSession::BulkDrain()
{
	return Owner().BulkDrainOps;
}

FStructuralGraphCircuitEcho& FStructuralGraphSession::CircuitEcho()
{
	return Owner().CircuitEchoOps;
}

FStructuralGraphRemoval& FStructuralGraphSession::Removal()
{
	return Owner().RemovalOps;
}

FStructuralPlacementQueue& FStructuralGraphSession::Placement()
{
	return Owner().PlacementQueue;
}

FStructuralCrossSiteGraph& FStructuralGraphSession::CrossSite()
{
	return Owner().CrossSiteGraph;
}

FStructuralSiteState& FStructuralGraphSession::SiteState()
{
	return Owner().SiteState;
}

FStructuralControlIdGangIndex& FStructuralGraphSession::ControlIdGangIndex()
{
	return Owner().ControlIdGangIndex;
}

FStructuralBusMemberSpatialIndex& FStructuralGraphSession::BusMemberSpatialIndex()
{
	return Owner().BusMemberSpatialIndex;
}

int32& FStructuralGraphSession::CircuitPromotionDepth()
{
	return Owner().CircuitPromotionDepth;
}

bool& FStructuralGraphSession::BulkLoadDrainActive()
{
	return Owner().bBulkLoadDrainActive;
}

bool& FStructuralGraphSession::PendingPostLoadLightReconcile()
{
	return Owner().bPendingPostLoadLightReconcile;
}

bool& FStructuralGraphSession::PendingFinalLightingReconcile()
{
	return Owner().bPendingFinalLightingReconcile;
}

int32& FStructuralGraphSession::FinalLightingReconcilePass()
{
	return Owner().FinalLightingReconcilePass;
}

bool& FStructuralGraphSession::BridgeEndpointRootIndexDirty()
{
	return Owner().bBridgeEndpointRootIndexDirty;
}

bool& FStructuralGraphSession::PostLoadRebuilt()
{
	return Owner().bPostLoadRebuilt;
}

bool FStructuralGraphSession::ShouldDeferCircuitDrivenRefresh() const
{
	return Owner().ShouldDeferCircuitDrivenRefresh();
}

bool FStructuralGraphSession::ShouldDeferSwitchCircuitRefresh() const
{
	return Owner().ShouldDeferSwitchCircuitRefresh();
}

bool FStructuralGraphSession::IsBulkLoadDrainActive() const
{
	return Owner().IsBulkLoadDrainActive();
}

bool FStructuralGraphSession::HasPendingBulkRemesh() const
{
	return Owner().HasPendingBulkRemesh();
}

EAttachContext FStructuralGraphSession::GetCurrentAttachContext() const
{
	return Owner().GetCurrentAttachContext();
}

FStructuralPowerContext FStructuralGraphSession::MakeProcessorContext(
	const EAttachContext AttachContext,
	const int32 SiteRoot) const
{
	return FStructuralPowerContext(
		*const_cast<FStructuralGraphSession*>(this),
		AttachContext,
		SiteRoot);
}

FStructuralPowerContext FStructuralGraphSession::GetProcessorContext() const
{
	return MakeProcessorContext(GetCurrentAttachContext());
}

void FStructuralGraphSession::EnqueuePlacement(
	AFGBuildable* Buildable,
	const EStructuralPlacementJobType JobType,
	const bool bDefer)
{
	Owner().EnqueuePlacement(Buildable, JobType, bDefer);
}

void FStructuralGraphSession::ProcessOutlet(AFGBuildable* Buildable)
{
	Owner().ProcessOutlet(Buildable);
}

void FStructuralGraphSession::OnBuildableRemoved(AFGBuildable* Buildable)
{
	Removal().OnBuildableRemoved(Buildable);
}

void FStructuralGraphSession::UnregisterBuildableActor(const FStructuralNodeId& NodeId)
{
	Owner().UnregisterBuildableActor(NodeId);
}

bool FStructuralGraphSession::HasActiveDeferredWork() const
{
	return Owner().HasActiveDeferredWork();
}

void FStructuralGraphSession::NotifyDeferredWorkRegistered()
{
	Owner().NotifyDeferredWorkRegistered();
}

FStructuralNodeId FStructuralGraphSession::MakeNodeId(const AFGBuildable* Buildable) const
{
	return Owner().MakeNodeId(Buildable);
}

UFGStructuralPowerConnectionComponent* FStructuralGraphSession::FindBusConnector(const AFGBuildable* Host) const
{
	return Owner().FindBusConnector(Host);
}

UFGStructuralPowerConnectionComponent* FStructuralGraphSession::GetOrCreateBusConnector(AFGBuildable* Host)
{
	return Owner().GetOrCreateBusConnector(Host);
}

UFGStructuralPowerConnectionComponent* FStructuralGraphSession::GetOrCreateSwitchControlBus(
	AFGBuildableCircuitSwitch* Switch)
{
	return Owner().GetOrCreateSwitchControlBus(Switch);
}

UFGStructuralPowerConnectionComponent* FStructuralGraphSession::GetOrCreatePanelControlBus(
	AFGBuildableLightsControlPanel* Panel)
{
	return Owner().GetOrCreatePanelControlBus(Panel);
}

void FStructuralGraphSession::RegisterBuildableActor(AFGBuildable* Buildable)
{
	Owner().RegisterBuildableActor(Buildable);
}

bool FStructuralGraphSession::IsBuildablePlacementPending(AFGBuildable* Buildable) const
{
	return Owner().IsBuildablePlacementPending(Buildable);
}

void FStructuralGraphSession::DispatchOutlet(AFGBuildable* Buildable)
{
	FStructuralEndpointDispatcher::DispatchOutlet(*this, Buildable);
}

void FStructuralGraphSession::ProcessStructure(AFGBuildable* Buildable)
{
	StructureIngress().ProcessStructure(Buildable);
}

void FStructuralGraphSession::ProcessLightweightStructure(const FStructuralLightweightKey& Key)
{
	StructureIngress().ProcessLightweightStructure(Key);
}

void FStructuralGraphSession::MaybeReleaseFactoryTick()
{
	Owner().MaybeReleaseFactoryTick();
}

void FStructuralGraphSession::ScheduleFinalLightingReconcile()
{
	Owner().ScheduleFinalLightingReconcile();
}

int32 FStructuralGraphSession::GetPendingJobCount() const
{
	return Owner().GetPendingJobCount();
}

FStructuralEndpointIdRegistry& FStructuralGraphSession::IdRegistry()
{
	return Owner().IdRegistry;
}

TMap<int32, TWeakObjectPtr<UFGCircuitConnectionComponent>>& FStructuralGraphSession::SourceConnectorByRoot()
{
	return Owner().SourceConnectorByRoot;
}

TMap<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>>& FStructuralGraphSession::RegisteredBuildables()
{
	return Owner().RegisteredBuildables;
}

void FStructuralGraphSession::DispatchPlacement(
	AFGBuildable* Buildable,
	const bool bLocalPromoteOnly,
	const bool bOverrideAttachContext,
	const EAttachContext AttachContextOverride)
{
	FStructuralEndpointDispatcher::DispatchPlacement(
		*this,
		Buildable,
		bLocalPromoteOnly,
		bOverrideAttachContext,
		AttachContextOverride);
}

void FStructuralGraphSession::DispatchTeardown(AFGBuildable* Buildable)
{
	FStructuralEndpointDispatcher::DispatchTeardown(*this, Buildable);
}

bool FStructuralGraphSession::FindNearestStructureAnchorForEquipment(
	const FVector& QueryLoc,
	const float MaxHorizontal,
	const float MaxVertical,
	FVector& OutAnchor,
	int32& OutComponentRoot) const
{
	return Owner().FindNearestStructureAnchorForEquipment(
		QueryLoc,
		MaxHorizontal,
		MaxVertical,
		OutAnchor,
		OutComponentRoot);
}

bool FStructuralGraphSession::QueryHoverpackStructuralAnchor(
	const FVector& QueryLoc,
	const float MaxHorizontal,
	const float MaxVertical,
	FStructuralHoverpackAnchorQuery& Out) const
{
	return Owner().QueryHoverpackStructuralAnchor(QueryLoc, MaxHorizontal, MaxVertical, Out);
}
