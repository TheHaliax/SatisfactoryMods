// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "FGSaveInterface.h"
#include "GameFramework/Info.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralConnectivityGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Routing/EStructuralChannel.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "AStructuralPowerGraphSubsystem.generated.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
class AFGBuildablePowerPole;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;
class UFGStructuralPowerConnectionComponent;
class UWorld;

/** DR-010 hoverpack geometry tether query result. */
struct FStructuralHoverpackAnchorQuery
{
	FVector Anchor = FVector::ZeroVector;
	int32 ComponentRoot = INDEX_NONE;
	TWeakObjectPtr<UFGPowerConnectionComponent> FeedConnector;
	bool bPowered = false;
	bool bFound = false;
};

UENUM()
enum class EStructuralPlacementJobType : uint8
{
	Structure,
	Outlet
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
	void OnSwitchStateChanged(AFGBuildableCircuitSwitch* Switch);
	void RunDiagnostics() const;

	FStructuralWallAnchor ResolveOutletAnchor(AFGBuildable* Outlet) const;
	FStructuralComponentResolveResult ResolveStructuralComponentAt(
		const FVector& WorldLoc,
		float QueryRadiusCm,
		TSubclassOf<AFGBuildable> ClassHint = nullptr) const;

	UFGCircuitConnectionComponent* GetComponentSourceConnector(
		int32 ComponentRoot,
		const AFGBuildable* ExcludeHost = nullptr);

	FStructuralComponentKey MakeComponentKeyForRoot(int32 ComponentRoot) const;
	FStructuralChannelKey ResolveChannelKeyForBuildable(AFGBuildable* Buildable);
	FName ResolveEffectiveId(AFGBuildable* Buildable, EStructuralChannel Tag);
	void SetPlayerOverrideId(AFGBuildable* Buildable, FName PlayerOverrideId);
	FName GetPlayerOverrideId(const AFGBuildable* Buildable) const;
	FName GetOrCreateComponentDefaultId(const FStructuralComponentKey& ComponentKey);

	int32 GetStructureNodeCount() const { return StructureGraph.GetNodeCount(); }
	int32 GetTrackedEndpointCount() const { return TrackedEndpoints.Num(); }
	int32 GetTrackedPoleCount() const { return TrackedEndpoints.Num(); }
	int32 GetTrackedLightweightCount() const { return LightweightIndex.GetTrackedCount(); }
	int32 GetPendingJobCount() const
	{
		return (PendingJobs.Num() - PendingJobsHead)
			+ (PendingLightweightJobs.Num() - PendingLightweightJobsHead);
	}
	void GetGraphStats(int32& OutComponents, int32& OutLargest) { StructureGraph.GetComponentStats(OutComponents, OutLargest); }

	bool DoesComponentRootCarryPower(int32 ComponentRoot) const;
	bool QueryHoverpackStructuralAnchor(
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FStructuralHoverpackAnchorQuery& Out) const;

	// IFGSaveInterface — geometry rebuilds on load; Id registry (DR-009) persists here.
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override { bPostLoadRebuilt = false; }
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }

private:
	struct FDeferredPlacementJob
	{
		TWeakObjectPtr<AFGBuildable> Buildable;
		EStructuralPlacementJobType JobType = EStructuralPlacementJobType::Structure;
	};

	void ProcessStructure(AFGBuildable* Buildable);
	void ProcessLightweightStructure(const FStructuralLightweightKey& Key);
	void ProcessOutlet(AFGBuildable* Buildable);
	void ProcessPoleEndpoint(AFGBuildablePowerPole* Pole);
	void ProcessSwitchEndpoint(AFGBuildableCircuitSwitch* Switch);

	UFGStructuralPowerConnectionComponent* GetOrCreateBusConnector(AFGBuildable* Host);
	UFGStructuralPowerConnectionComponent* GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet);
	int32 ResolveEndpointComponentRoot(
		AFGBuildable* Endpoint,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	FStructuralComponentKey MakeComponentKeyForParent(const FStructuralNodeId& ParentId) const;
	void LinkBusToVisibleConnections(AFGBuildable* Host, UFGStructuralPowerConnectionComponent* Bus);
	void RestitchComponent(int32 Root, bool bTearDownFirst);
	void ReEnergizeComponentRoots(const TArray<int32>& Roots, bool bTearDownFirst);
	void RestitchSwitchKeyedSubnet(
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 ComponentRoot,
		const FStructuralNodeId& SwitchNodeId);
	bool EnsureParentRegisteredInGraph(
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);
	void TearDownSwitchStructuralLinks(AFGBuildable* Host);
	void StripInactiveSwitchStructuralLinks(int32 Root);
	void EnsureSwitchListener(AFGBuildableCircuitSwitch* Switch);
	static bool ShouldEndpointParticipateInRestitch(
		AFGBuildable* Host,
		EStructuralEndpointKind Kind);
	bool ShouldMeshEndpoints(
		AFGBuildable* HostA,
		AFGBuildable* HostB,
		int32 ComponentRoot) const;

	bool LinkHiddenPair(UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B);
	void PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed);
	UFGStructuralPowerConnectionComponent* FindPoweredHiddenReachable(
		UFGStructuralPowerConnectionComponent* StartHidden,
		int32 MaxHiddenHops = 512) const;

	void RegisterBuildableActor(AFGBuildable* Buildable);
	void UnregisterBuildableActor(const FStructuralNodeId& NodeId);
	void RebuildBuildableRegistry(UWorld* World);
	void RebuildLightweightIndex(UWorld* World);
	void PurgeSavedOutletBusMesh(UWorld* World);
	static FStructuralNodeId MakeParentNodeId(const FStructuralWallAnchor& Anchor);

	void CompactPendingJobQueues();
	bool IsBuildableAlreadyPending(AFGBuildable* Buildable, EStructuralPlacementJobType JobType) const;
	bool IsLightweightAlreadyPending(const FStructuralLightweightKey& Key) const;

	FStructuralConnectivityGraph StructureGraph;
	FStructuralLightweightIndex LightweightIndex;
	TMap<FStructuralNodeId, FTrackedEndpoint> TrackedEndpoints;
	TMap<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>> RegisteredBuildables;

	UPROPERTY(SaveGame)
	TMap<FStructuralComponentKey, FName> ComponentDefaultIds;

	UPROPERTY(SaveGame)
	TMap<FStructuralNodeId, FName> PlayerOverrideIds;
	TArray<FDeferredPlacementJob> PendingJobs;
	TArray<FStructuralLightweightKey> PendingLightweightJobs;
	int32 PendingJobsHead = 0;
	int32 PendingLightweightJobsHead = 0;
	bool bPostLoadRebuilt = false;
};
