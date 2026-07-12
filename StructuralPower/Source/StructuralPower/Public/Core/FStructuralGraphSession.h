// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralNodeId.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Routing/EStructuralChannel.h"
#include "Save/FStructuralPlacementQueue.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
class AFGBuildablePowerPole;
class AFGBuildablePowerStorage;
class AStructuralPowerGraphSubsystem;
class FStructuralGraphBootstrap;
class FStructuralGraphBulkDrain;
class FStructuralGraphCircuitEcho;
class FStructuralGraphRemoval;
class FStructuralGraphStructureIngress;
class FStructuralBridgeRootIndex;
class FStructuralBusMemberSpatialIndex;
class FStructuralConnectivityGraph;
class FStructuralControlIdGangIndex;
class FStructuralCrossSiteGraph;
class FStructuralEndpointIndex;
class FStructuralGraphCircuitOps;
class FStructuralGraphIdOps;
class FStructuralLightweightIndex;
class FStructuralPowerReconcile;
class FStructuralPowerRestitch;
class FStructuralEndpointIdRegistry;
class FStructuralPlacementQueue;
class FStructuralSiteState;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;
class UFGStructuralPowerConnectionComponent;
class UWorld;
struct FStructuralChannelKey;
struct FStructuralComponentIdList;
struct FStructuralComponentKey;
struct FStructuralComponentResolveResult;
struct FStructuralEndpointOverrides;
struct FStructuralHoverpackAnchorQuery;
struct FStructuralOutletParentResolveParams;
struct FStructuralPowerContext;
struct FStructuralWallAnchor;
struct FTrackedEndpoint;

class STRUCTURALPOWER_API FStructuralGraphSession
{
public:
	explicit FStructuralGraphSession(AStructuralPowerGraphSubsystem& InOwner);

	AStructuralPowerGraphSubsystem& Owner();
	const AStructuralPowerGraphSubsystem& Owner() const;

	UWorld* GetWorld() const;

	FStructuralConnectivityGraph& StructureGraph();
	const FStructuralConnectivityGraph& StructureGraph() const;

	FStructuralLightweightIndex& LightweightIndex();
	FStructuralEndpointIndex& EndpointIndex();
	TMap<FStructuralNodeId, FTrackedEndpoint>& TrackedEndpoints();
	const TMap<FStructuralNodeId, FTrackedEndpoint>& TrackedEndpoints() const;

	FStructuralGraphCircuitOps& Circuit();
	FStructuralGraphIdOps& Ids();
	FStructuralPowerReconcile& Reconcile();
	FStructuralPowerRestitch& Restitch();
	FStructuralBridgeRootIndex& BridgeRootIndex();
	FStructuralGraphBootstrap& Bootstrap();
	FStructuralGraphStructureIngress& StructureIngress();
	FStructuralGraphBulkDrain& BulkDrain();
	FStructuralGraphCircuitEcho& CircuitEcho();
	FStructuralGraphRemoval& Removal();
	FStructuralPlacementQueue& Placement();
	FStructuralCrossSiteGraph& CrossSite();
	FStructuralSiteState& SiteState();
	FStructuralControlIdGangIndex& ControlIdGangIndex();
	FStructuralBusMemberSpatialIndex& BusMemberSpatialIndex();

	int32& CircuitPromotionDepth();
	bool& BulkLoadDrainActive();
	bool& PendingPostLoadLightReconcile();
	bool& PendingFinalLightingReconcile();
	int32& FinalLightingReconcilePass();
	bool& BridgeEndpointRootIndexDirty();
	bool& PostLoadRebuilt();

	bool IsBulkLoadDrainActive() const;

	bool ShouldDeferCircuitDrivenRefresh() const;
	bool ShouldDeferSwitchCircuitRefresh() const;
	EAttachContext GetCurrentAttachContext() const;

	FStructuralPowerContext MakeProcessorContext(
		EAttachContext AttachContext,
		int32 SiteRoot = INDEX_NONE) const;

	FStructuralPowerContext GetProcessorContext() const;

	void EnqueuePlacement(AFGBuildable* Buildable, EStructuralPlacementJobType JobType, bool bDefer);
	void ProcessOutlet(AFGBuildable* Buildable);
	void OnBuildableRemoved(AFGBuildable* Buildable);

	FStructuralNodeId MakeNodeId(const AFGBuildable* Buildable) const;
	UFGStructuralPowerConnectionComponent* FindBusConnector(const AFGBuildable* Host) const;
	UFGStructuralPowerConnectionComponent* GetOrCreateBusConnector(AFGBuildable* Host);
	UFGStructuralPowerConnectionComponent* GetOrCreateSwitchControlBus(AFGBuildableCircuitSwitch* Switch);
	UFGStructuralPowerConnectionComponent* GetOrCreatePanelControlBus(AFGBuildableLightsControlPanel* Panel);

	void RegisterBuildableActor(AFGBuildable* Buildable);
	void UnregisterBuildableActor(const FStructuralNodeId& NodeId);
	bool HasActiveDeferredWork() const;
	void NotifyDeferredWorkRegistered();
	void AddEndpointToRootIndex(int32 Root, EStructuralEndpointKind Kind, const FStructuralNodeId& EndpointId);
	void MarkBridgeEndpointRootIndexDirty();
	void RefreshBridgeEndpointRootIndex();
	int32 FindRootForTrackedEndpoint(const FTrackedEndpoint& Tracked) const;

	FStructuralWallAnchor ResolveOutletAnchor(AFGBuildable* Outlet) const;
	int32 ResolveEndpointComponentRoot(
		AFGBuildable* Endpoint,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	FStructuralChannelKey ResolveChannelKeyForBuildable(AFGBuildable* Buildable);
	FName ResolveSource(AFGBuildable* Buildable, EStructuralChannel Tag);
	FName ResolveControl(AFGBuildable* Buildable, EStructuralChannel Tag);
	FStructuralComponentKey MakeComponentKeyForRoot(int32 ComponentRoot) const;
	FName GetOrCreateComponentDefaultId(const FStructuralComponentKey& ComponentKey);

	bool EnsureParentRegisteredInGraph(const FStructuralWallAnchor& Anchor, FStructuralNodeId& OutParentId);
	FStructuralNodeId MakeParentNodeId(const FStructuralWallAnchor& Anchor) const;
	FStructuralOutletParentResolveParams MakeOutletParentResolveParams() const;

	bool DoesComponentRootCarryPower(int32 ComponentRoot) const;
	bool DoesSiteStructuralBusCarryPower(int32 ComponentRoot) const;
	bool IsDirectSwitchFedLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsPanelDownstreamLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsSwitchFeedOpen(int32 Root, FName SwitchControlId);

	void PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed);
	void PromoteOutletBusIfPowered(UFGStructuralPowerConnectionComponent* OutletBus, bool bLocalPromoteOnly);
	bool LinkHiddenPairLocal(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		bool bPromoteCircuit = true);
	void LinkBusToVisibleConnectionsLocal(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* Bus,
		bool bMeshOnlyLinks = false);

	void MarkEchoDirtyForSwitchToggle(AFGBuildableCircuitSwitch* Switch, int32 LocalRoot);
	void NoteSwitchToggleHandled(AFGBuildableCircuitSwitch* Switch);
	void NoteSwitchCircuitEchoProcessed(AFGBuildableCircuitSwitch* Switch);
	bool ShouldSkipSwitchCircuitEcho(AFGBuildableCircuitSwitch* Switch, const TCHAR** OutReason = nullptr);
	bool ShouldSkipPanelCircuitEcho(AFGBuildableLightsControlPanel* Panel, const TCHAR** OutReason = nullptr);

	void EnsurePanelListener(AFGBuildableLightsControlPanel* Panel);
	void NotePanelToggleHandled(AFGBuildableLightsControlPanel* Panel);
	bool IsBuildablePlacementPending(AFGBuildable* Buildable) const;

	UFGPowerConnectionComponent* ResolveSubnetFeedConnector(
		int32 ComponentRoot,
		const FStructuralChannelKey& DeviceKey);
	bool IsPanelSupplyLinked(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed) const;
	void PromotePanelSupplyConnection(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed,
		bool bLocalPromoteOnly = false);
	void PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed);
	bool LinkHiddenPair(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		bool bPromoteCircuit = true);
	void LinkBusToVisibleConnections(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* Bus);
	bool HasBridgeBusPeerMesh(UFGStructuralPowerConnectionComponent* Bus) const;
	bool TryMeshPeerBusOnComponent(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SelfId,
		bool bBridgePeersOnly,
		bool bMeshOnlyLinks = false);
	int32 ResolvePoleComponentRoot(
		AFGBuildablePowerPole* Pole,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	void EnumerateTrackedLightsOnRoot(
		int32 Root,
		TFunctionRef<void(AFGBuildableLightSource*)> Visitor);

	int32 GetEndpointComponentRoot(AFGBuildable* Endpoint);
	void DispatchOutlet(AFGBuildable* Buildable);
	void ProcessStructure(AFGBuildable* Buildable);
	void ProcessLightweightStructure(const FStructuralLightweightKey& Key);
	void FinishBulkLoadDrain();
	void MaybeReleaseFactoryTick();
	void ScheduleFinalLightingReconcile();
	int32 GetPendingJobCount() const;
	void RebuildControlIdGangsForRoot(int32 ComponentRoot);
	void RefreshPanelsForLightSourceOnRoot(int32 Root, FName LightSource);

	void DispatchPlacement(
		AFGBuildable* Buildable,
		bool bLocalPromoteOnly = false,
		bool bOverrideAttachContext = false,
		EAttachContext AttachContextOverride = EAttachContext::RuntimePlace);
	void DispatchTeardown(AFGBuildable* Buildable);

	FStructuralEndpointIdRegistry& IdRegistry();
	TMap<int32, TWeakObjectPtr<UFGCircuitConnectionComponent>>& SourceConnectorByRoot();
	TMap<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>>& RegisteredBuildables();

	bool QueryHoverpackStructuralAnchor(
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FStructuralHoverpackAnchorQuery& Out) const;

	bool FindNearestStructureAnchorForEquipment(
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FVector& OutAnchor,
		int32& OutComponentRoot) const;

	UFGCircuitConnectionComponent* GetComponentSourceConnector(
		int32 ComponentRoot,
		const AFGBuildable* ExcludeHost = nullptr);

private:
	AStructuralPowerGraphSubsystem* OwnerPtr = nullptr;
};
