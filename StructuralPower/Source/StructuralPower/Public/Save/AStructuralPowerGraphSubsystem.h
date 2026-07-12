// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
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
#include "Graph/FStructuralSiteState.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Save/FStructuralControlIdGangIndex.h"
#include "Save/FStructuralEndpointIdRegistry.h"
#include "Save/FStructuralGraphIdOps.h"
#include "Save/FStructuralPlacementQueue.h"
#include "Reconcile/FStructuralPowerReconcile.h"
#include "Reconcile/FStructuralPowerRestitch.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Graph/FStructuralBridgeRootIndex.h"
#include "Graph/FStructuralGraphBootstrap.h"
#include "Graph/FStructuralGraphBulkDrain.h"
#include "Graph/FStructuralGraphCircuitEcho.h"
#include "Graph/FStructuralGraphRemoval.h"
#include "Graph/FStructuralGraphStructureIngress.h"
#include "AStructuralPowerGraphSubsystem.generated.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
class AFGBuildableGenerator;
class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
class AFGBuildablePowerPole;
class AFGBuildablePowerStorage;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;
class UFGStructuralPowerConnectionComponent;
class UWorld;

struct FStructuralHoverpackAnchorQuery
{
	FVector Anchor = FVector::ZeroVector;
	int32 ComponentRoot = INDEX_NONE;
	TWeakObjectPtr<UFGPowerConnectionComponent> FeedConnector;
	bool bPowered = false;
	bool bFound = false;
};

UCLASS()
class STRUCTURALPOWER_API AStructuralPowerGraphSubsystem : public AInfo, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxFinalLightingReconcilePasses = 6;

	AStructuralPowerGraphSubsystem();

	FStructuralGraphSession& GetGraphSession();
	const FStructuralGraphSession& GetGraphSession() const;

	static AStructuralPowerGraphSubsystem* GetOrCreate(UWorld* World);
	static AStructuralPowerGraphSubsystem* Find(UWorld* World);
	static FStructuralNodeId MakeNodeId(const AFGBuildable* Buildable);
	static UFGStructuralPowerConnectionComponent* FindBusConnector(const AFGBuildable* Host);
	static UFGStructuralPowerConnectionComponent* FindPanelControlBus(const AFGBuildable* Host);
	static UFGStructuralPowerConnectionComponent* FindSwitchControlBus(const AFGBuildable* Host);
	static UFGStructuralPowerConnectionComponent* FindOutletBusConnector(const AFGBuildablePowerPole* Outlet);
	static void StripPersistedEndpointModComponents(AFGBuildable* Host);

	void OnWorldReady(UWorld* World);
	void EnqueuePlacement(AFGBuildable* Buildable, EStructuralPlacementJobType JobType, bool bDefer);
	void EnqueueLightweightPlacement(const FStructuralLightweightKey& Key, bool bDefer);
	void TickDeferredPlacements(int32 MaxJobs);
	void OnBuildableRemoved(AFGBuildable* Buildable);
	void OnLightweightRemoved(const FStructuralLightweightKey& Key);
	void ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole);
	void ProcessSwitchCircuitsRebuilt(AFGBuildableCircuitSwitch* Switch);
	void ProcessPanelWireDelta(AFGBuildableLightsControlPanel* Panel);
	bool ShouldSkipPanelCircuitEcho(
		AFGBuildableLightsControlPanel* Panel,
		const TCHAR** OutReason = nullptr);
	bool ShouldSkipSwitchCircuitEcho(
		AFGBuildableCircuitSwitch* Switch,
		const TCHAR** OutReason = nullptr);
	void MarkEchoDirtyForSwitchToggle(AFGBuildableCircuitSwitch* Switch, int32 LocalRoot);
	void NotePanelCircuitEchoProcessed(AFGBuildableLightsControlPanel* Panel);
	void NotePanelToggleHandled(AFGBuildableLightsControlPanel* Panel);
	void NoteSwitchCircuitEchoProcessed(AFGBuildableCircuitSwitch* Switch);
	void NoteSwitchToggleHandled(AFGBuildableCircuitSwitch* Switch);
	void ProcessPoleWireDelta(AFGBuildablePowerPole* Pole);
	void OnSwitchStateChanged(AFGBuildableCircuitSwitch* Switch);
	void ReconcileAllLightConsumers();
	void ReconcileGroupLightingState();
	void RefreshPanelsForLightSourceOnRoot(int32 Root, FName LightSource);
	void RunDiagnostics() const;

	FStructuralWallAnchor ResolveOutletAnchor(AFGBuildable* Outlet) const;
	FStructuralComponentResolveResult ResolveStructuralComponentAt(
		const FVector& WorldLoc,
		float QueryRadiusCm,
		TSubclassOf<AFGBuildable> ClassHint = nullptr) const;

	UFGCircuitConnectionComponent* GetComponentSourceConnector(
		int32 ComponentRoot,
		const AFGBuildable* ExcludeHost = nullptr);

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

	void RebuildControlIdGangsForRoot(int32 ComponentRoot);
	TArray<FStructuralNodeId> GetControlIdGangMembers(
		const FStructuralComponentKey& ComponentKey,
		FName ControlId) const;

	int32 GetStructureNodeCount() const { return StructureGraph.GetNodeCount(); }
	int32 GetTrackedPoleCount() const { return TrackedEndpoints.Num(); }
	int32 GetTrackedLightweightCount() const { return LightweightIndex.GetTrackedCount(); }
	int32 GetPendingJobCount() const { return PlacementQueue.GetPendingCount(); }
	bool IsBuildablePlacementPending(AFGBuildable* Buildable) const;
	void GetGraphStats(int32& OutComponents, int32& OutLargest) { StructureGraph.GetComponentStats(OutComponents, OutLargest); }

	bool DoesComponentRootCarryPower(int32 ComponentRoot) const;
	bool DoesSiteStructuralBusCarryPower(int32 ComponentRoot) const;
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

	bool ShouldDeferSwitchCircuitRefresh() const { return CircuitPromotionDepth > 0; }

	bool IsBulkLoadDrainActive() const { return bBulkLoadDrainActive; }
	bool IsPendingPostLoadLightReconcile() const { return bPendingPostLoadLightReconcile; }
	bool IsPendingFinalLightingReconcile() const { return bPendingFinalLightingReconcile; }
	bool HasActiveDeferredWork() const;
	void NotifyDeferredWorkRegistered();
	void MaybeReleaseFactoryTick();
	bool ShouldDeferCircuitDrivenRefresh() const;

	EAttachContext GetCurrentAttachContext() const;

	FStructuralPowerContext MakeProcessorContext(
		EAttachContext AttachContext,
		int32 SiteRoot = INDEX_NONE) const;

	FStructuralPowerContext GetProcessorContext() const;

	void LogPanelReconcileSummary(AFGBuildableLightsControlPanel* Panel);

	void EnumerateTrackedLightsOnRoot(
		int32 Root,
		TFunctionRef<void(AFGBuildableLightSource*)> Visitor);

	void BeginCircuitPromotion();
	void EndCircuitPromotion();

	bool LinkHiddenPair(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		bool bPromoteCircuit = true);
	bool IsPanelSupplyLinked(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed) const;
	bool IsPanelSupplyLinkedAndLive(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed) const;
	void PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed);
	UFGStructuralPowerConnectionComponent* GetOrCreatePanelControlBus(
		AFGBuildableLightsControlPanel* Panel);
	UFGStructuralPowerConnectionComponent* GetOrCreateSwitchControlBus(
		AFGBuildableCircuitSwitch* Switch);

	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override { bPostLoadRebuilt = false; }
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }

	void ProcessOutlet(AFGBuildable* Buildable);

	friend class FStructuralGraphSession;

private:
	void ProcessStructure(AFGBuildable* Buildable);
	void ProcessLightweightStructure(const FStructuralLightweightKey& Key);

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
	bool LinkHiddenPairLocal(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		bool bPromoteCircuit = true);
	bool TryMeshPeerBusOnComponent(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SelfId,
		bool bBridgePeersOnly,
		bool bMeshOnlyLinks = false);
	int32 FindRootForTrackedEndpoint(const FTrackedEndpoint& Tracked) const;
	int32 ResolveBridgeRootFromAnchor(
		AFGBuildable* Host,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId,
		bool bPreferBulkResolve);
	int32 ResolveBridgeComponentRootBulk(
		AFGBuildable* Host,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	void MarkBridgeEndpointRootIndexDirty();
	void RefreshBridgeEndpointRootIndex();
	void AddEndpointToRootIndex(
		int32 Root,
		EStructuralEndpointKind Kind,
		const FStructuralNodeId& EndpointId);
	bool IsDirectSwitchFedLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsPanelDownstreamLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsSwitchFeedOpen(int32 Root, FName SwitchControlId);
	void MaybeRunPostLoadLightReconcile();
	void MaybeRunFinalLightingReconcile();
	void ScheduleFinalLightingReconcile();
	void ReconcileAllPanelEndpoints();
	void CollectKnownPanelEndpoints(TArray<AFGBuildableLightsControlPanel*>& OutPanels);
	void ApplyKeyedSubnetAllPanels();
	void LinkBusToVisibleConnections(AFGBuildable* Host, UFGStructuralPowerConnectionComponent* Bus);
	void LinkBusToVisibleConnectionsLocal(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* Bus,
		bool bMeshOnlyLinks = false);
	bool HasBridgeBusPeerMesh(UFGStructuralPowerConnectionComponent* Bus) const;
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
	FStructuralControlIdGangIndex ControlIdGangIndex;
	FStructuralGraphIdOps IdOps;
	FStructuralPowerReconcile ReconcileOps;
	FStructuralPowerRestitch RestitchOps;
	FStructuralGraphCircuitOps CircuitOps;
	FStructuralBridgeRootIndex BridgeRootIndex;
	FStructuralGraphBootstrap BootstrapOps;
	FStructuralGraphStructureIngress StructureIngressOps;
	FStructuralGraphBulkDrain BulkDrainOps;
	FStructuralGraphCircuitEcho CircuitEchoOps;
	FStructuralGraphRemoval RemovalOps;
	FStructuralPlacementQueue PlacementQueue;
	int32 CircuitPromotionDepth = 0;
	bool bPostLoadRebuilt = false;
	bool bPendingPostLoadLightReconcile = false;
	bool bPendingFinalLightingReconcile = false;
	int32 FinalLightingReconcilePass = 0;
	bool bBulkLoadDrainActive = false;
	bool bBridgeEndpointRootIndexDirty = true;
	FStructuralEndpointIndex EndpointIndex;
	FStructuralCrossSiteGraph CrossSiteGraph;
	FStructuralSiteState SiteState;
	TMap<int32, TWeakObjectPtr<UFGCircuitConnectionComponent>> SourceConnectorByRoot;

	TUniquePtr<FStructuralGraphSession> GraphSession;
	bool bOpsBoundToSession = false;
};
