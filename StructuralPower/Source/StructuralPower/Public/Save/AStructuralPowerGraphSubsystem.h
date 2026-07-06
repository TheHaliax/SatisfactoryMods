// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralNodeId.h"
#include "FGSaveInterface.h"
#include "GameFramework/Info.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralConnectivityGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralEndpointIndex.h"
#include "Graph/FStructuralBusMemberSpatialIndex.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Graph/FStructuralCrossSiteGraph.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Save/FStructuralEndpointIdRegistry.h"
#include "Save/FStructuralPlacementQueue.h"
#include "AStructuralPowerGraphSubsystem.generated.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
class AFGBuildableGenerator;
class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
class AFGBuildablePowerPole;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;
class UFGStructuralPowerConnectionComponent;
class UWorld;
class FStructuralPowerGeneratorProcessor;
class FStructuralPowerLightProcessor;
class FStructuralPowerPanelProcessor;
class FStructuralPowerSwitchProcessor;

/** DR-010 hoverpack geometry tether query result. */
struct FStructuralHoverpackAnchorQuery
{
	FVector Anchor = FVector::ZeroVector;
	int32 ComponentRoot = INDEX_NONE;
	TWeakObjectPtr<UFGPowerConnectionComponent> FeedConnector;
	bool bPowered = false;
	bool bFound = false;
};

/**
 * Pole-to-pole structural power. Foundations/walls live only in a data-side connectivity
 * graph (never power circuit members). Bridge poles carry a hidden "outlet bus"; poles whose
 * parent structures share a graph component get their buses meshed and promoted to one circuit
 * on power. Connectivity is recomputed from live geometry on load — nothing structural persists.
 */
UCLASS()
class STRUCTURALPOWER_API AStructuralPowerGraphSubsystem : public AInfo, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AStructuralPowerGraphSubsystem();

	static AStructuralPowerGraphSubsystem* GetOrCreate(UWorld* World);
	static AStructuralPowerGraphSubsystem* Find(UWorld* World);
	static FStructuralNodeId MakeNodeId(const AFGBuildable* Buildable);
	static UFGStructuralPowerConnectionComponent* FindBusConnector(const AFGBuildable* Host);
	static UFGStructuralPowerConnectionComponent* FindPanelControlBus(const AFGBuildable* Host);
	static UFGStructuralPowerConnectionComponent* FindOutletBusConnector(const AFGBuildablePowerPole* Outlet);
	/** Remove saved/runtime outlet bus + switch listener before CircuitBridge BeginPlay. */
	static void StripPersistedEndpointModComponents(AFGBuildable* Host);

	void OnWorldReady(UWorld* World);
	void EnqueuePlacement(AFGBuildable* Buildable, EStructuralPlacementJobType JobType, bool bDefer);
	void EnqueueLightweightPlacement(const FStructuralLightweightKey& Key, bool bDefer);
	void TickDeferredPlacements(int32 MaxJobs);
	void OnBuildableRemoved(AFGBuildable* Buildable);
	void OnLightweightRemoved(const FStructuralLightweightKey& Key);
	void ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole);
	void ProcessPoleWireDelta(AFGBuildablePowerPole* Pole);
	void OnSwitchStateChanged(AFGBuildableCircuitSwitch* Switch);
	void ReconcileAllLightConsumers();
	void RunDiagnostics() const;

	FStructuralWallAnchor ResolveOutletAnchor(AFGBuildable* Outlet) const;
	FStructuralComponentResolveResult ResolveStructuralComponentAt(
		const FVector& WorldLoc,
		float QueryRadiusCm,
		TSubclassOf<AFGBuildable> ClassHint = nullptr) const;

	UFGCircuitConnectionComponent* GetComponentSourceConnector(
		int32 ComponentRoot,
		const AFGBuildable* ExcludeHost = nullptr);

	/** DR-002/011 — default bus or switch-gated outlet bus for keyed Source ids. */
	UFGPowerConnectionComponent* ResolveSubnetFeedConnector(
		int32 ComponentRoot,
		const FStructuralChannelKey& DeviceKey);

	FStructuralComponentKey MakeComponentKeyForRoot(int32 ComponentRoot) const;
	FStructuralComponentKey MakeComponentKeyForBuildable(const AFGBuildable* Buildable) const;
	int32 GetEndpointComponentRoot(AFGBuildable* Endpoint);
	FStructuralChannelKey ResolveChannelKeyForBuildable(AFGBuildable* Buildable);
	FName ResolveSource(AFGBuildable* Buildable, EStructuralChannel Tag);
	FName ResolveControl(AFGBuildable* Buildable, EStructuralChannel Tag);
	void SetEndpointIds(
		AFGBuildable* Buildable,
		FName Source,
		FName Control,
		bool bClearSource,
		bool bClearControl);
	bool GetEndpointOverrides(const AFGBuildable* Buildable, FStructuralEndpointOverrides& Out) const;
	bool CollectIdsOnComponent(const FStructuralComponentKey& Key, FStructuralComponentIdList& Out) const;
	FName GetOrCreateComponentDefaultId(const FStructuralComponentKey& ComponentKey);

	int32 GetStructureNodeCount() const { return StructureGraph.GetNodeCount(); }
	int32 GetTrackedPoleCount() const { return TrackedEndpoints.Num(); }
	int32 GetTrackedLightweightCount() const { return LightweightIndex.GetTrackedCount(); }
	int32 GetPendingJobCount() const { return PlacementQueue.GetPendingCount(); }
	bool IsBuildablePlacementPending(AFGBuildable* Buildable) const;
	void GetGraphStats(int32& OutComponents, int32& OutLargest) { StructureGraph.GetComponentStats(OutComponents, OutLargest); }

	bool DoesComponentRootCarryPower(int32 ComponentRoot) const;
	bool FindNearestStructureAnchorForEquipment(
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FVector& OutAnchor,
		int32& OutComponentRoot) const;
	bool QueryHoverpackStructuralAnchor(
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FStructuralHoverpackAnchorQuery& Out) const;

	/** True while mod is calling ConnectComponents — suppresses switch/panel circuit-refresh enqueue. */
	bool ShouldDeferSwitchCircuitRefresh() const { return CircuitPromotionDepth > 0; }

	/** While true, post-load drain uses cheap pole attach — no RestitchComponent. */
	bool IsBulkLoadDrainActive() const { return bBulkLoadDrainActive; }

	EAttachContext GetCurrentAttachContext() const;

	void EnumerateTrackedLightsOnRoot(
		int32 Root,
		TFunctionRef<void(AFGBuildableLightSource*)> Visitor);

	void BeginCircuitPromotion();
	void EndCircuitPromotion();

	bool LinkHiddenPair(UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B);
	bool IsPanelSupplyLinkedAndLive(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed) const;
	void PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed);
	UFGStructuralPowerConnectionComponent* GetOrCreatePanelControlBus(
		AFGBuildableLightsControlPanel* Panel);

	// IFGSaveInterface — geometry rebuilds on load; Id registry (DR-009) persists here.
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override { bPostLoadRebuilt = false; }
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }

	friend class FStructuralPowerGeneratorProcessor;
	friend class FStructuralPowerLightProcessor;
	friend class FStructuralPowerPanelProcessor;
	friend class FStructuralPowerSwitchProcessor;
	friend class FStructuralCrossSiteGraph;

private:
	void ProcessStructure(AFGBuildable* Buildable);
	void ProcessLightweightStructure(const FStructuralLightweightKey& Key);
	void ProcessOutlet(AFGBuildable* Buildable);
	void ProcessPoleEndpoint(AFGBuildablePowerPole* Pole);
	void ProcessGeneratorEndpoint(AFGBuildableGenerator* Generator);
	void ProcessLightEndpoint(AFGBuildableLightSource* Light, bool bLocalPromoteOnly = false);
	void ProcessPanelEndpoint(AFGBuildableLightsControlPanel* Panel, bool bLocalPromoteOnly = false);
	void RestitchLightEndpointsForRoot(int32 Root, EAttachContext AttachContext);
	void RestitchPanelEndpointsForRoot(int32 Root, EAttachContext AttachContext);
	void RestitchPanelsWithControlOnRoot(int32 Root, FName ControlId);
	void TearDownLightStructuralLinks(AFGBuildableLightSource* Light);
	void TearDownPanelStructuralLinks(AFGBuildableLightsControlPanel* Panel);

	UFGStructuralPowerConnectionComponent* GetOrCreateBusConnector(AFGBuildable* Host);
	UFGStructuralPowerConnectionComponent* GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet);
	int32 ResolveEndpointComponentRoot(
		AFGBuildable* Endpoint,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	int32 ResolvePoleComponentRoot(
		AFGBuildablePowerPole* Pole,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	int32 ResolveBridgeComponentRootBulk(
		AFGBuildable* Host,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	int32 ResolveBridgeHostComponentRoot(
		AFGBuildable* Host,
		FStructuralNodeId* OutParentId = nullptr);
	void PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed);
	void PromoteOutletBusIfPowered(
		UFGStructuralPowerConnectionComponent* OutletBus,
		bool bLocalPromoteOnly = false);
	void PromotePanelSupplyConnection(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed,
		bool bLocalPromoteOnly = false);
	void ApplyLocalBridgeBusAttach(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SelfId,
		const AFGBuildable* FeedExcludeHost = nullptr);
	bool LinkHiddenPairLocal(UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B);
	bool TryMeshPeerBusOnComponent(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SelfId,
		bool bBridgePeersOnly);
	int32 FindRootForTrackedEndpoint(const FTrackedEndpoint& Tracked) const;
	int32 ResolveBridgeRootFromAnchor(
		AFGBuildable* Host,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId,
		bool bPreferBulkResolve);
	void MarkBridgeEndpointRootIndexDirty();
	void RefreshBridgeEndpointRootIndex();
	void AddEndpointToRootIndex(
		int32 Root,
		EStructuralEndpointKind Kind,
		const FStructuralNodeId& EndpointId);
	void ApplyKeyedSubnetAllPanels();
	void CollectKnownPanelEndpoints(TArray<AFGBuildableLightsControlPanel*>& OutPanels);
	bool IsDirectSwitchFedLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsPanelDownstreamLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsSwitchFeedOpen(int32 Root, FName SwitchControlId);
	void LogPanelReconcileSummary(AFGBuildableLightsControlPanel* Panel);
	void FinishBulkLoadDrain();
	void ReconcileAllPanelEndpoints();
	void MaybeRunPostLoadLightReconcile();
	FStructuralComponentKey MakeComponentKeyForParent(const FStructuralNodeId& ParentId) const;
	void LinkBusToVisibleConnections(AFGBuildable* Host, UFGStructuralPowerConnectionComponent* Bus);
	void LinkBusToVisibleConnectionsLocal(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* Bus);
	bool HasBridgeBusPeerMesh(UFGStructuralPowerConnectionComponent* Bus) const;
	void RestitchComponent(int32 Root, bool bTearDownFirst, EAttachContext AttachContext);
	void ReEnergizeComponentRoots(
		const TArray<int32>& Roots,
		bool bTearDownFirst,
		EAttachContext AttachContext);
	bool EnsureParentRegisteredInGraph(
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	void EnsurePanelListener(AFGBuildableLightsControlPanel* Panel);
	bool ShouldEndpointParticipateInRestitch(
		AFGBuildable* Host,
		EStructuralEndpointKind Kind);
	bool ShouldMeshEndpoints(
		AFGBuildable* HostA,
		AFGBuildable* HostB,
		int32 ComponentRoot) const;
	UFGStructuralPowerConnectionComponent* FindPoweredHiddenReachable(
		UFGStructuralPowerConnectionComponent* StartHidden,
		int32 MaxHiddenHops = 512) const;

	void RegisterBuildableActor(AFGBuildable* Buildable);
	void UnregisterBuildableActor(const FStructuralNodeId& NodeId);
	void RebuildBuildableRegistry(UWorld* World);
	void RebuildLightweightIndex(UWorld* World);
	void PurgeSavedOutletBusMesh(UWorld* World);
	static FStructuralNodeId MakeParentNodeId(const FStructuralWallAnchor& Anchor);
	FStructuralOutletParentResolveParams MakeOutletParentResolveParams() const;

	FStructuralConnectivityGraph StructureGraph;
	FStructuralLightweightIndex LightweightIndex;
	FStructuralBusMemberSpatialIndex BusMemberSpatialIndex;
	TMap<FStructuralNodeId, FTrackedEndpoint> TrackedEndpoints;
	TMap<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>> RegisteredBuildables;

	UPROPERTY(SaveGame)
	TMap<FStructuralComponentKey, FName> ComponentDefaultIds;

	UPROPERTY(SaveGame)
	TMap<FStructuralNodeId, FStructuralEndpointOverrides> PlayerEndpointOverrides;

	FStructuralEndpointIdRegistry IdRegistry;
	FStructuralPlacementQueue PlacementQueue;
	int32 CircuitPromotionDepth = 0;
	bool bPostLoadRebuilt = false;
	bool bPendingPostLoadLightReconcile = false;
	bool bBulkLoadDrainActive = false;
	bool bBridgeEndpointRootIndexDirty = true;
	FStructuralEndpointIndex EndpointIndex;
	FStructuralCrossSiteGraph CrossSiteGraph;
	/** Feed connector cache per component root — invalidated with the root index. */
	TMap<int32, TWeakObjectPtr<UFGCircuitConnectionComponent>> SourceConnectorByRoot;
};
