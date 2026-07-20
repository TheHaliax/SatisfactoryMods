// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;

UENUM()
enum class EStructuralPlacementJobType : uint8 { Structure, Outlet, Pipe };

class STRUCTURALPOWER_API FStructuralPlacementQueue {
 public:
  bool EnqueueBuildable(AFGBuildable* Buildable, EStructuralPlacementJobType JobType);

  bool EnqueueLightweight(const FStructuralLightweightKey& Key);

  int32 GetPendingCount() const;

  bool IsBuildablePending(AFGBuildable* Buildable, EStructuralPlacementJobType JobType) const;

  bool IsAnyBuildableJobPending(AFGBuildable* Buildable) const;

  bool IsLightweightPending(const FStructuralLightweightKey& Key) const;

  void Tick(int32 MaxJobs, bool bBulkLoadDrainActive,
            TFunctionRef<void(AFGBuildable*, EStructuralPlacementJobType)> ProcessBuildableJob,
            TFunctionRef<void(const FStructuralLightweightKey&)> ProcessLightweightJob,
            TFunctionRef<void()> OnQueuesIdle);

  void RemoveBuildable(AFGBuildable* Buildable);

  void RemoveLightweight(const FStructuralLightweightKey& Key);

  void ForEachPendingOutletBuildable(TFunctionRef<void(AFGBuildable*)> Visitor) const;

 private:
  struct FDeferredPlacementJob {
    TWeakObjectPtr<AFGBuildable> Buildable;
    EStructuralPlacementJobType JobType = EStructuralPlacementJobType::Structure;
  };

  struct FPendingBuildableKey {
    TWeakObjectPtr<AFGBuildable> Buildable;
    EStructuralPlacementJobType JobType = EStructuralPlacementJobType::Structure;

    friend uint32 GetTypeHash(const FPendingBuildableKey& Key) {
      return HashCombine(GetTypeHash(Key.Buildable), GetTypeHash(static_cast<uint8>(Key.JobType)));
    }

    bool operator==(const FPendingBuildableKey& Other) const {
      return JobType == Other.JobType && Buildable == Other.Buildable;
    }
  };

  void Compact();
  void NoteBuildableDequeued(const FDeferredPlacementJob& Job);
  void NoteLightweightDequeued(const FStructuralLightweightKey& Key);

  TArray<FDeferredPlacementJob> PendingJobs;
  TArray<FStructuralLightweightKey> PendingLightweightJobs;
  TSet<FPendingBuildableKey> PendingBuildableKeys;
  TSet<FStructuralLightweightKey> PendingLightweightKeys;
  int32 PendingJobsHead = 0;
  int32 PendingLightweightJobsHead = 0;
};
