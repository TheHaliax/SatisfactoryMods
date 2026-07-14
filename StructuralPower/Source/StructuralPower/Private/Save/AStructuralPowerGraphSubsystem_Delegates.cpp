// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Attach/FStructuralPanelAttach.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralPowerContext.h"
#include "Core/FStructuralPowerWorldGate.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "Diagnostics/FStructuralPowerDiagnostics.h"
#include "Equipment/FStructuralEquipmentBridgeRegistry.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Network/UStructuralPowerPanelListener.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/FStructuralEndpointDispatcher.h"
#include "Processors/FStructuralEndpointProcessors.h"
#include "Routing/FStructuralMembershipRole.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "TimerManager.h"

void AStructuralPowerGraphSubsystem::BeginCircuitPromotion()
{
	CircuitOps.BeginCircuitPromotion();
}

void AStructuralPowerGraphSubsystem::EndCircuitPromotion()
{
	CircuitOps.EndCircuitPromotion();
}

bool AStructuralPowerGraphSubsystem::LinkHiddenPair(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	bool bPromoteCircuit)
{
	return CircuitOps.LinkHiddenPair(A, B, bPromoteCircuit);
}

bool AStructuralPowerGraphSubsystem::LinkHiddenPairLocal(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	bool bPromoteCircuit)
{
	return CircuitOps.LinkHiddenPairLocal(A, B, bPromoteCircuit);
}

void AStructuralPowerGraphSubsystem::MarkBridgeEndpointRootIndexDirty()
{
	BridgeRootIndex.MarkBridgeEndpointRootIndexDirty();
}

void AStructuralPowerGraphSubsystem::RefreshBridgeEndpointRootIndex()
{
	BridgeRootIndex.RefreshBridgeEndpointRootIndex();
}

void AStructuralPowerGraphSubsystem::AddEndpointToRootIndex(
	int32 Root,
	EStructuralEndpointKind Kind,
	const FStructuralNodeId& EndpointId)
{
	BridgeRootIndex.AddEndpointToRootIndex(Root, Kind, EndpointId);
}

int32 AStructuralPowerGraphSubsystem::FindRootForTrackedEndpoint(
	const FTrackedEndpoint& Tracked) const
{
	return BridgeRootIndex.FindRootForTrackedEndpoint(Tracked);
}

int32 AStructuralPowerGraphSubsystem::ResolveBridgeRootFromAnchor(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId,
	bool bPreferBulkResolve)
{
	return BridgeRootIndex.ResolveBridgeRootFromAnchor(Host, Anchor, OutParentId, bPreferBulkResolve);
}

void AStructuralPowerGraphSubsystem::PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed)
{
	CircuitOps.PromoteStructuralMeshFrom(Seed);
}

void AStructuralPowerGraphSubsystem::PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed)
{
	CircuitOps.PromoteDirectHiddenLinks(Seed);
}

bool AStructuralPowerGraphSubsystem::IsPanelSupplyLinked(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return CircuitOps.IsPanelSupplyLinked(InputPower, Feed);
}

bool AStructuralPowerGraphSubsystem::IsPanelSupplyLinkedAndLive(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return CircuitOps.IsPanelSupplyLinkedAndLive(InputPower, Feed);
}

void AStructuralPowerGraphSubsystem::PromotePanelSupplyConnection(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed,
	bool bLocalPromoteOnly)
{
	CircuitOps.PromotePanelSupplyConnection(InputPower, Feed, bLocalPromoteOnly);
}

void AStructuralPowerGraphSubsystem::PromoteOutletBusIfPowered(
	UFGStructuralPowerConnectionComponent* OutletBus,
	bool bLocalPromoteOnly)
{
	CircuitOps.PromoteOutletBusIfPowered(OutletBus, bLocalPromoteOnly);
}

void AStructuralPowerGraphSubsystem::ApplyLocalBridgeBusAttach(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SelfId,
	const AFGBuildable* FeedExcludeHost)
{
	CircuitOps.ApplyLocalBridgeBusAttach(Host, OutletBus, Root, SelfId, FeedExcludeHost);
}

bool AStructuralPowerGraphSubsystem::TryMeshPeerBusOnComponent(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SelfId,
	bool bBridgePeersOnly,
	bool bMeshOnlyLinks)
{
	return CircuitOps.TryMeshPeerBusOnComponent(
		Host,
		OutletBus,
		Root,
		SelfId,
		bBridgePeersOnly,
		bMeshOnlyLinks);
}

int32 AStructuralPowerGraphSubsystem::ResolveBridgeComponentRootBulk(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.ResolveBridgeComponentRootBulk(Host, Anchor, OutParentId);
}

int32 AStructuralPowerGraphSubsystem::ResolvePoleComponentRoot(
	AFGBuildablePowerPole* Pole,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.ResolvePoleComponentRoot(Pole, Anchor, OutParentId);
}

EAttachContext AStructuralPowerGraphSubsystem::GetCurrentAttachContext() const
{
	return AttachContextFromBulkDrain(bBulkLoadDrainActive);
}

FStructuralPowerContext AStructuralPowerGraphSubsystem::MakeProcessorContext(
	EAttachContext AttachContext,
	int32 SiteRoot) const
{
	return GetGraphSession().MakeProcessorContext(AttachContext, SiteRoot);
}

FStructuralPowerContext AStructuralPowerGraphSubsystem::GetProcessorContext() const
{
	return MakeProcessorContext(GetCurrentAttachContext());
}

bool AStructuralPowerGraphSubsystem::ShouldDeferCircuitDrivenRefresh() const
{
	return bBulkLoadDrainActive
		|| bPendingPostLoadLightReconcile
		|| bPendingFinalLightingReconcile
		|| CircuitPromotionDepth > 0;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindPoweredHiddenReachable(
	UFGStructuralPowerConnectionComponent* StartHidden,
	int32 MaxHiddenHops) const
{
	return CircuitOps.FindPoweredHiddenReachable(StartHidden, MaxHiddenHops);
}

FStructuralComponentKey AStructuralPowerGraphSubsystem::MakeComponentKeyForRoot(
	int32 ComponentRoot) const
{
	return IdOps.MakeComponentKeyForRoot(ComponentRoot);
}

FStructuralComponentKey AStructuralPowerGraphSubsystem::MakeComponentKeyForBuildable(
	const AFGBuildable* Buildable) const
{
	return IdOps.MakeComponentKeyForBuildable(Buildable);
}

bool AStructuralPowerGraphSubsystem::GetEndpointOverrides(
	const AFGBuildable* Buildable,
	FStructuralEndpointOverrides& Out) const
{
	return IdOps.GetEndpointOverrides(Buildable, Out);
}

FName AStructuralPowerGraphSubsystem::GetOrCreateComponentDefaultId(
	const FStructuralComponentKey& ComponentKey)
{
	return IdOps.GetOrCreateComponentDefaultId(ComponentKey);
}

FName AStructuralPowerGraphSubsystem::ResolveSource(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	return IdOps.ResolveSource(Buildable, Tag);
}

FName AStructuralPowerGraphSubsystem::ResolveControl(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	return IdOps.ResolveControl(Buildable, Tag);
}

FStructuralChannelKey AStructuralPowerGraphSubsystem::ResolveChannelKeyForBuildable(
	AFGBuildable* Buildable)
{
	return IdOps.ResolveChannelKeyForBuildable(Buildable);
}

int32 AStructuralPowerGraphSubsystem::GetEndpointComponentRoot(AFGBuildable* Endpoint)
{
	return BridgeRootIndex.GetEndpointComponentRoot(Endpoint);
}

void AStructuralPowerGraphSubsystem::SetEndpointIds(
	AFGBuildable* Buildable,
	FName Source,
	FName Control,
	bool bClearSource,
	bool bClearControl,
	const bool bGlobalControl,
	const bool bTouchGlobalControl)
{
	IdOps.SetEndpointIds(
		Buildable,
		Source,
		Control,
		bClearSource,
		bClearControl,
		bGlobalControl,
		bTouchGlobalControl);
}

bool AStructuralPowerGraphSubsystem::CollectIdsOnComponent(
	const FStructuralComponentKey& Key,
	FStructuralComponentIdList& Out) const
{
	return IdOps.CollectIdsOnComponent(Key, Out);
}

void AStructuralPowerGraphSubsystem::RebuildControlIdGangsForRoot(const int32 ComponentRoot)
{
	if (ComponentRoot == INDEX_NONE)
	{
		return;
	}

	const FStructuralComponentKey ComponentKey = MakeComponentKeyForRoot(ComponentRoot);
	ControlIdGangIndex.ClearComponent(ComponentKey);

	auto RegisterPublisher = [&](const FStructuralNodeId& NodeId, AFGBuildable* Buildable)
	{
		if (!IsValid(Buildable))
		{
			return;
		}

		const EStructuralChannel Tag = FStructuralEligibilityRules::ClassifyBuildable(Buildable);
		const EStructuralMembershipRole Role = FStructuralMembershipRole::Resolve(Buildable, Tag);
		if (!FStructuralMembershipRole::PublishesControl(Role))
		{
			return;
		}

		const FName ControlId = ResolveControl(Buildable, Tag);
		if (!FStructuralPowerRouter::IsAssignedControl(ControlId))
		{
			return;
		}

		bool bGlobal = false;
		if (const FStructuralEndpointOverrides* Overrides =
				IdRegistry.FindPlayerOverride(NodeId))
		{
			bGlobal = Overrides->bGlobalControl;
		}

		FStructuralControlGangKey GangKey;
		GangKey.Scope = bGlobal
			? EStructuralControlGangScope::Global
			: EStructuralControlGangScope::Site;
		GangKey.ComponentKey = ComponentKey;
		GangKey.ControlId = ControlId;
		ControlIdGangIndex.RegisterPublisher(NodeId, GangKey);
	};

	// Tracked endpoints on this root only — never scan RegisteredBuildables foundations.
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (!Pair.Value.MountParentId.IsValid())
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.MountParentId) != ComponentRoot)
		{
			continue;
		}

		RegisterPublisher(Pair.Key, Pair.Value.Actor.Get());
	}
}

TArray<FStructuralNodeId> AStructuralPowerGraphSubsystem::GetControlIdGangMembers(
	const FStructuralComponentKey& ComponentKey,
	const FName ControlId) const
{
	return ControlIdGangIndex.GetGangMembers(ComponentKey, ControlId);
}

int32 AStructuralPowerGraphSubsystem::ResolveEndpointComponentRoot(
	AFGBuildable* Endpoint,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.ResolveEndpointComponentRoot(Endpoint, Anchor, OutParentId);
}

int32 AStructuralPowerGraphSubsystem::ResolveBridgeHostComponentRoot(
	AFGBuildable* Host,
	FStructuralNodeId* OutParentId)
{
	return BridgeRootIndex.ResolveBridgeHostComponentRoot(Host, OutParentId);
}

void AStructuralPowerGraphSubsystem::MaybeRunFinalLightingReconcile()
{
	ReconcileOps.MaybeRunFinalLightingReconcile();
}

void AStructuralPowerGraphSubsystem::ScheduleFinalLightingReconcile()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		bPendingFinalLightingReconcile = false;
		return;
	}

	bPendingFinalLightingReconcile = true;

	UWorld* World = GetWorld();
	if (!IsValid(World) || !FStructuralPowerWorldGate::IsGameplayWorld(World))
	{
		bPendingFinalLightingReconcile = false;
		return;
	}

	NotifyDeferredWorkRegistered();

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			MaybeRunFinalLightingReconcile();
		}));
}

bool AStructuralPowerGraphSubsystem::IsDirectSwitchFedLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	return ReconcileOps.IsDirectSwitchFedLight(Root, LightKey);
}

bool AStructuralPowerGraphSubsystem::IsPanelDownstreamLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	return ReconcileOps.IsPanelDownstreamLight(Root, LightKey);
}

bool AStructuralPowerGraphSubsystem::IsSwitchFeedOpen(
	int32 Root,
	FName SwitchControlId)
{
	return ReconcileOps.IsSwitchFeedOpen(Root, SwitchControlId);
}

void AStructuralPowerGraphSubsystem::LogPanelReconcileSummary(
	AFGBuildableLightsControlPanel* Panel)
{
	ReconcileOps.LogPanelReconcileSummary(Panel);
}

void AStructuralPowerGraphSubsystem::CollectKnownPanelEndpoints(
	TArray<AFGBuildableLightsControlPanel*>& OutPanels)
{
	ReconcileOps.CollectKnownPanelEndpoints(OutPanels);
}

void AStructuralPowerGraphSubsystem::ReconcileAllPanelEndpoints()
{
	ReconcileOps.ReconcileAllPanelEndpoints();
}

void AStructuralPowerGraphSubsystem::ApplyKeyedSubnetAllPanels()
{
	ReconcileOps.ApplyKeyedSubnetAllPanels();
}

bool AStructuralPowerGraphSubsystem::EnsureParentRegisteredInGraph(
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.EnsureParentRegisteredInGraph(Anchor, OutParentId);
}

void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnectionsLocal(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus,
	bool bMeshOnlyLinks)
{
	CircuitOps.LinkBusToVisibleConnectionsLocal(Host, Bus, bMeshOnlyLinks);
}

void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnections(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus)
{
	CircuitOps.LinkBusToVisibleConnections(Host, Bus);
}

bool AStructuralPowerGraphSubsystem::HasBridgeBusPeerMesh(
	UFGStructuralPowerConnectionComponent* Bus) const
{
	return CircuitOps.HasBridgeBusPeerMesh(Bus);
}

UFGCircuitConnectionComponent* AStructuralPowerGraphSubsystem::GetComponentSourceConnector(
	int32 ComponentRoot,
	const AFGBuildable* ExcludeHost)
{
	return CircuitOps.GetComponentSourceConnector(ComponentRoot, ExcludeHost);
}

UFGPowerConnectionComponent* AStructuralPowerGraphSubsystem::ResolveSubnetFeedConnector(
	int32 ComponentRoot,
	const FStructuralChannelKey& DeviceKey)
{
	return CircuitOps.ResolveSubnetFeedConnector(ComponentRoot, DeviceKey);
}

bool AStructuralPowerGraphSubsystem::DoesComponentRootCarryPower(int32 ComponentRoot) const
{
	return const_cast<AStructuralPowerGraphSubsystem*>(this)
		->CircuitOps.DoesComponentRootCarryPower(ComponentRoot);
}

bool AStructuralPowerGraphSubsystem::DoesSiteStructuralBusCarryPower(int32 ComponentRoot) const
{
	return const_cast<AStructuralPowerGraphSubsystem*>(this)
		->CircuitOps.DoesSiteStructuralBusCarryPower(ComponentRoot);
}

bool AStructuralPowerGraphSubsystem::FindNearestStructureAnchorForEquipment(
	const FVector& QueryLoc,
	float MaxHorizontal,
	float MaxVertical,
	FVector& OutAnchor,
	int32& OutComponentRoot) const
{
	return StructureGraph.FindNearestStructureAnchor(
		QueryLoc,
		MaxHorizontal,
		MaxVertical,
		OutAnchor,
		OutComponentRoot);
}

bool AStructuralPowerGraphSubsystem::QueryHoverpackStructuralAnchor(
	const FVector& QueryLoc,
	float MaxHorizontal,
	float MaxVertical,
	FStructuralHoverpackAnchorQuery& Out) const
{
	return FStructuralEquipmentBridgeRegistry::Get().QueryHoverpackStructuralAnchor(
		const_cast<AStructuralPowerGraphSubsystem*>(this)->GetGraphSession(),
		QueryLoc,
		MaxHorizontal,
		MaxVertical,
		Out);
}

bool AStructuralPowerGraphSubsystem::ShouldEndpointParticipateInRestitch(
	AFGBuildable* Host,
	EStructuralEndpointKind Kind)
{
	return RestitchOps.ShouldEndpointParticipateInRestitch(Host, Kind);
}

bool AStructuralPowerGraphSubsystem::ShouldMeshEndpoints(
	AFGBuildable* HostA,
	AFGBuildable* HostB,
	int32 ComponentRoot) const
{
	return RestitchOps.ShouldMeshEndpoints(HostA, HostB, ComponentRoot);
}

void AStructuralPowerGraphSubsystem::OnSwitchStateChanged(AFGBuildableCircuitSwitch* Switch)
{
	if (ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	if (IStructuralEndpointProcessor* Processor =
			FStructuralEndpointCatalog::Get().FindMutable(EStructuralEndpointKind::Switch))
	{
		FStructuralEndpointDispatcher::RunToggleChanged(GetGraphSession(), *Processor, Switch);
	}
}

void AStructuralPowerGraphSubsystem::EnsurePanelListener(AFGBuildableLightsControlPanel* Panel)
{
	GetGraphSession().CircuitEcho().EnsurePanelListener(Panel);
}

void AStructuralPowerGraphSubsystem::ProcessStructure(AFGBuildable* Buildable)
{
	GetGraphSession().StructureIngress().ProcessStructure(Buildable);
}

void AStructuralPowerGraphSubsystem::ProcessLightweightStructure(
	const FStructuralLightweightKey& Key)
{
	GetGraphSession().StructureIngress().ProcessLightweightStructure(Key);
}

void AStructuralPowerGraphSubsystem::ProcessOutlet(AFGBuildable* Buildable)
{
	FStructuralEndpointDispatcher::DispatchOutlet(GetGraphSession(), Buildable);
}

void AStructuralPowerGraphSubsystem::ProcessSwitchCircuitsRebuilt(AFGBuildableCircuitSwitch* Switch)
{
	if (ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	FStructuralEndpointDispatcher::DispatchSwitchCircuitsRebuilt(GetGraphSession(), Switch);
}

void AStructuralPowerGraphSubsystem::ProcessPanelWireDelta(AFGBuildableLightsControlPanel* Panel)
{
	if (ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	FStructuralEndpointDispatcher::DispatchPanelWireDelta(GetGraphSession(), Panel);
}

bool AStructuralPowerGraphSubsystem::ShouldSkipPanelCircuitEcho(
	AFGBuildableLightsControlPanel* Panel,
	const TCHAR** OutReason)
{
	return GetGraphSession().CircuitEcho().ShouldSkipPanelCircuitEcho(Panel, OutReason);
}

bool AStructuralPowerGraphSubsystem::ShouldSkipSwitchCircuitEcho(
	AFGBuildableCircuitSwitch* Switch,
	const TCHAR** OutReason)
{
	return GetGraphSession().CircuitEcho().ShouldSkipSwitchCircuitEcho(Switch, OutReason);
}

void AStructuralPowerGraphSubsystem::MarkEchoDirtyForSwitchToggle(
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	GetGraphSession().CircuitEcho().MarkEchoDirtyForSwitchToggle(Switch, LocalRoot);
}

void AStructuralPowerGraphSubsystem::NotePanelCircuitEchoProcessed(
	AFGBuildableLightsControlPanel* Panel)
{
	GetGraphSession().CircuitEcho().NotePanelCircuitEchoProcessed(Panel);
}

void AStructuralPowerGraphSubsystem::NotePanelToggleHandled(
	AFGBuildableLightsControlPanel* Panel)
{
	GetGraphSession().CircuitEcho().NotePanelToggleHandled(Panel);
}

void AStructuralPowerGraphSubsystem::NoteSwitchCircuitEchoProcessed(
	AFGBuildableCircuitSwitch* Switch)
{
	GetGraphSession().CircuitEcho().NoteSwitchCircuitEchoProcessed(Switch);
}

void AStructuralPowerGraphSubsystem::NoteSwitchToggleHandled(
	AFGBuildableCircuitSwitch* Switch)
{
	GetGraphSession().CircuitEcho().NoteSwitchToggleHandled(Switch);
}

void AStructuralPowerGraphSubsystem::ProcessPoleWireDelta(AFGBuildablePowerPole* Pole)
{
	FStructuralEndpointDispatcher::DispatchPoleWireDelta(GetGraphSession(), Pole);
}

void AStructuralPowerGraphSubsystem::EnumerateTrackedLightsOnRoot(
	int32 Root,
	TFunctionRef<void(AFGBuildableLightSource*)> Visitor)
{
	ReconcileOps.EnumerateTrackedLightsOnRoot(Root, Visitor);
}

void AStructuralPowerGraphSubsystem::ReconcileAllLightConsumers()
{
	ReconcileOps.ReconcileAllLightConsumers();
}

void AStructuralPowerGraphSubsystem::ReconcileGroupLightingState()
{
	ReconcileOps.ReconcileGroupLightingState();
}

void AStructuralPowerGraphSubsystem::RefreshPanelsForLightSourceOnRoot(
	int32 Root,
	FName LightSource)
{
	ReconcileOps.RefreshPanelsForLightSourceOnRoot(Root, LightSource);
}

void AStructuralPowerGraphSubsystem::ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole)
{
	FStructuralEndpointDispatcher::DispatchWallOutletAfterWire(GetGraphSession(), Pole);
}

void AStructuralPowerGraphSubsystem::OnBuildableRemoved(AFGBuildable* Buildable)
{
	GetGraphSession().Removal().OnBuildableRemoved(Buildable);
}

void AStructuralPowerGraphSubsystem::OnLightweightRemoved(const FStructuralLightweightKey& Key)
{
	GetGraphSession().Removal().OnLightweightRemoved(Key);
}

void AStructuralPowerGraphSubsystem::RegisterBuildableActor(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	RegisteredBuildables.Add(MakeNodeId(Buildable), Buildable);
	if (FStructuralEligibilityRules::IsBusMember(Buildable))
	{
		BusMemberSpatialIndex.RegisterMember(Buildable);
	}
}

void AStructuralPowerGraphSubsystem::UnregisterBuildableActor(const FStructuralNodeId& NodeId)
{
	if (!NodeId.IsValid() || NodeId.IsLightweight())
	{
		return;
	}

	if (const TWeakObjectPtr<AFGBuildable>* Entry = RegisteredBuildables.Find(NodeId))
	{
		if (AFGBuildable* Buildable = Entry->Get())
		{
			BusMemberSpatialIndex.UnregisterMember(Buildable);
		}
	}

	RegisteredBuildables.Remove(NodeId);
}

void AStructuralPowerGraphSubsystem::RunDiagnostics() const
{
	FStructuralPowerDiagnostics::AuditWorld(GetWorld(), true);
}
