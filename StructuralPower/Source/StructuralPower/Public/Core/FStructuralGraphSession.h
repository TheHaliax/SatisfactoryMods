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
	bool HasPendingBulkRemesh() const;

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

	bool IsBuildablePlacementPending(AFGBuildable* Buildable) const;
	void DispatchOutlet(AFGBuildable* Buildable);
	void ProcessStructure(AFGBuildable* Buildable);
	void ProcessLightweightStructure(const FStructuralLightweightKey& Key);
	void MaybeReleaseFactoryTick();
	void ScheduleFinalLightingReconcile();
	int32 GetPendingJobCount() const;

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

private:
	AStructuralPowerGraphSubsystem* OwnerPtr = nullptr;
};
