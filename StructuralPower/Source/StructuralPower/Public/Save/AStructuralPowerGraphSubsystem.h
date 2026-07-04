// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "FGSaveInterface.h"
#include "GameFramework/Info.h"
#include "Graph/FStructuralConnectivityGraph.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "AStructuralPowerGraphSubsystem.generated.h"

class AFGBuildable;
class AFGBuildablePowerPole;
class UFGPowerConnectionComponent;
class UFGStructuralPowerConnectionComponent;
class UWorld;

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
	static UFGStructuralPowerConnectionComponent* FindOutletBusConnector(const AFGBuildablePowerPole* Outlet);

	void OnWorldReady(UWorld* World);
	void EnqueuePlacement(AFGBuildable* Buildable, EStructuralPlacementJobType JobType, bool bDefer);
	void EnqueueLightweightPlacement(const FStructuralLightweightKey& Key, bool bDefer);
	void TickDeferredPlacements(int32 MaxJobs);
	void OnBuildableRemoved(AFGBuildable* Buildable);
	void OnLightweightRemoved(const FStructuralLightweightKey& Key);
	void ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole);
	void RunDiagnostics() const;

	FStructuralWallAnchor ResolveOutletAnchor(AFGBuildable* Outlet) const;

	int32 GetStructureNodeCount() const { return StructureGraph.GetNodeCount(); }
	int32 GetTrackedPoleCount() const { return TrackedPoles.Num(); }
	int32 GetTrackedLightweightCount() const { return LightweightIndex.GetTrackedCount(); }
	int32 GetPendingJobCount() const
	{
		return (PendingJobs.Num() - PendingJobsHead)
			+ (PendingLightweightJobs.Num() - PendingLightweightJobsHead);
	}
	void GetGraphStats(int32& OutComponents, int32& OutLargest) { StructureGraph.GetComponentStats(OutComponents, OutLargest); }

	// IFGSaveInterface — nothing structural is persisted; connectivity is recomputed on load.
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

	struct FTrackedPole
	{
		TWeakObjectPtr<AFGBuildablePowerPole> Pole;
		FStructuralNodeId ParentId;
	};

	void ProcessStructure(AFGBuildable* Buildable);
	void ProcessLightweightStructure(const FStructuralLightweightKey& Key);
	void ProcessOutlet(AFGBuildable* Buildable);

	UFGStructuralPowerConnectionComponent* GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet);
	int32 ResolvePoleComponentRoot(AFGBuildablePowerPole* Pole, const FStructuralWallAnchor& Anchor, FStructuralNodeId& OutParentId);
	void LinkBusToVisibleConnections(AFGBuildablePowerPole* Pole, UFGStructuralPowerConnectionComponent* Bus);
	void RestitchComponent(int32 Root, bool bTearDownFirst);
	void ReEnergizeComponentRoots(const TArray<int32>& Roots, bool bTearDownFirst);

	bool LinkHiddenPair(UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B);
	void PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed);
	UFGStructuralPowerConnectionComponent* FindPoweredHiddenReachable(
		UFGStructuralPowerConnectionComponent* StartHidden,
		int32 MaxHiddenHops = 512) const;

	void RegisterBuildableActor(AFGBuildable* Buildable);
	void UnregisterBuildableActor(const FStructuralNodeId& NodeId);
	void RebuildBuildableRegistry(UWorld* World);
	void RebuildLightweightIndex(UWorld* World);

	// The outlet bus lives on saved pole actors and its hidden connections serialize with the
	// save. Wipe that persisted mesh on load so the graph is rebuilt purely from live geometry —
	// otherwise a bus mesh written by an earlier session reloads and keeps disconnected
	// structures powered (power that also survives save/load).
	void PurgeSavedOutletBusMesh(UWorld* World);
	static FStructuralNodeId MakeParentNodeId(const FStructuralWallAnchor& Anchor);

	void CompactPendingJobQueues();
	bool IsBuildableAlreadyPending(AFGBuildable* Buildable, EStructuralPlacementJobType JobType) const;
	bool IsLightweightAlreadyPending(const FStructuralLightweightKey& Key) const;

	FStructuralConnectivityGraph StructureGraph;
	FStructuralLightweightIndex LightweightIndex;
	TMap<FStructuralNodeId, FTrackedPole> TrackedPoles;
	TMap<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>> RegisteredBuildables;
	TArray<FDeferredPlacementJob> PendingJobs;
	TArray<FStructuralLightweightKey> PendingLightweightJobs;
	int32 PendingJobsHead = 0;
	int32 PendingLightweightJobsHead = 0;
	bool bPostLoadRebuilt = false;
};
