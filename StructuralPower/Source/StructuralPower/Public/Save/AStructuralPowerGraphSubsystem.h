// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeRecord.h"
#include "FGSaveInterface.h"
#include "GameFramework/Info.h"
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

/** Facade + save-backed graph manager (placement-only). */
UCLASS()
class STRUCTURALPOWER_API AStructuralPowerGraphSubsystem : public AInfo, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AStructuralPowerGraphSubsystem();

	static AStructuralPowerGraphSubsystem* GetOrCreate(UWorld* World);
	static FStructuralNodeId MakeNodeId(const AFGBuildable* Buildable);
	static bool HasStructuralConnector(const AFGBuildable* Buildable);
	static UFGStructuralPowerConnectionComponent* FindStructuralConnector(const AFGBuildable* Buildable);
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
	UFGStructuralPowerConnectionComponent* FindStructureHidden(const FStructuralWallAnchor& Anchor) const;

	int32 GetRuntimeNodeCount() const { return RuntimeGraph.Num(); }
	int32 GetPendingJobCount() const
	{
		return (PendingJobs.Num() - PendingJobsHead)
			+ (PendingLightweightJobs.Num() - PendingLightweightJobsHead);
	}
	int32 GetTrackedLightweightCount() const { return LightweightIndex.GetTrackedCount(); }

	// IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }

private:
	struct FDeferredPlacementJob
	{
		TWeakObjectPtr<AFGBuildable> Buildable;
		EStructuralPlacementJobType JobType = EStructuralPlacementJobType::Structure;
	};

	UFGStructuralPowerConnectionComponent* GetOrCreateStructuralConnector(AFGBuildable* Buildable);
	UFGStructuralPowerConnectionComponent* GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet);
	UFGStructuralPowerConnectionComponent* GetOrCreateStructureHidden(const FStructuralWallAnchor& Anchor);
	UFGStructuralPowerConnectionComponent* ResolveNodeConnector(const FStructuralNodeId& NodeId, bool bIsOutlet);
	void ProcessStructure(AFGBuildable* Buildable);
	void ProcessLightweightStructure(const FStructuralLightweightKey& Key);
	void ProcessOutlet(AFGBuildable* Buildable);
	bool LinkHiddenPair(UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B);
	void PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed);
	void PromoteStructuralMeshFrom(
		UFGPowerConnectionComponent* Seed,
		TSet<UFGPowerConnectionComponent*>& OutVisited);
	void SyncBridgePoleToPoweredParent(
		AFGBuildablePowerPole* Pole,
		UFGStructuralPowerConnectionComponent* OutletBus,
		UFGStructuralPowerConnectionComponent* ParentHidden);
	void RefreshBridgePolesOnMesh(const TSet<UFGPowerConnectionComponent*>& PoweredHiddenNodes);
	void RegisterGraphEdge(const FStructuralNodeId& A, const FStructuralNodeId& B, bool bAIsOutlet, bool bBIsOutlet);
	void RemoveGraphNode(const FStructuralNodeId& NodeId);
	AFGBuildable* ResolveBuildable(const FStructuralNodeId& NodeId) const;
	void RebuildRuntimeFromSave();
	void SyncSaveFromRuntime();
	void RegisterSavedLightweightNodes(UWorld* World);
	static FString NodeDedupKey(const FStructuralNodeId& NodeId);

	UPROPERTY(SaveGame)
	TArray<FStructuralNodeRecord> SavedGraph;

	TMap<FStructuralNodeId, FStructuralNodeRecord> RuntimeGraph;
	TArray<FDeferredPlacementJob> PendingJobs;
	TArray<FStructuralLightweightKey> PendingLightweightJobs;
	int32 PendingJobsHead = 0;
	int32 PendingLightweightJobsHead = 0;
	FStructuralLightweightIndex LightweightIndex;
	bool bPostLoadRebuilt = false;

	void CompactPendingJobQueues();
};
