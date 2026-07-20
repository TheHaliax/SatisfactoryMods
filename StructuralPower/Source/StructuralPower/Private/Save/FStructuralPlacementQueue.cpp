// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/FStructuralPlacementQueue.h"

#include "Buildables/FGBuildable.h"
#include "HAL/PlatformTime.h"
#include "StructuralPowerConstants.h"

bool FStructuralPlacementQueue::EnqueueBuildable(AFGBuildable* Buildable,
                                                 EStructuralPlacementJobType JobType) {
  if (!IsValid(Buildable)) {
    return false;
  }

  const FPendingBuildableKey Key{Buildable, JobType};
  if (PendingBuildableKeys.Contains(Key)) {
    return false;
  }

  FDeferredPlacementJob Job;
  Job.Buildable = Buildable;
  Job.JobType = JobType;
  PendingJobs.Add(Job);
  PendingBuildableKeys.Add(Key);
  return true;
}

bool FStructuralPlacementQueue::EnqueueLightweight(const FStructuralLightweightKey& Key) {
  if (!Key.IsValid() || PendingLightweightKeys.Contains(Key)) {
    return false;
  }

  PendingLightweightJobs.Add(Key);
  PendingLightweightKeys.Add(Key);
  return true;
}

int32 FStructuralPlacementQueue::GetPendingCount() const {
  return (PendingJobs.Num() - PendingJobsHead) +
         (PendingLightweightJobs.Num() - PendingLightweightJobsHead);
}

bool FStructuralPlacementQueue::IsBuildablePending(AFGBuildable* Buildable,
                                                   EStructuralPlacementJobType JobType) const {
  if (!IsValid(Buildable)) {
    return false;
  }
  return PendingBuildableKeys.Contains(FPendingBuildableKey{Buildable, JobType});
}

bool FStructuralPlacementQueue::IsAnyBuildableJobPending(AFGBuildable* Buildable) const {
  return IsBuildablePending(Buildable, EStructuralPlacementJobType::Outlet) ||
         IsBuildablePending(Buildable, EStructuralPlacementJobType::Structure) ||
         IsBuildablePending(Buildable, EStructuralPlacementJobType::Pipe);
}

bool FStructuralPlacementQueue::IsLightweightPending(const FStructuralLightweightKey& Key) const {
  return PendingLightweightKeys.Contains(Key);
}

void FStructuralPlacementQueue::NoteBuildableDequeued(const FDeferredPlacementJob& Job) {
  PendingBuildableKeys.Remove(FPendingBuildableKey{Job.Buildable, Job.JobType});
}

void FStructuralPlacementQueue::NoteLightweightDequeued(const FStructuralLightweightKey& Key) {
  PendingLightweightKeys.Remove(Key);
}

void FStructuralPlacementQueue::Tick(
    int32 MaxJobs, bool bBulkLoadDrainActive,
    TFunctionRef<void(AFGBuildable*, EStructuralPlacementJobType)> ProcessBuildableJob,
    TFunctionRef<void(const FStructuralLightweightKey&)> ProcessLightweightJob,
    TFunctionRef<void()> OnQueuesIdle) {
  if (MaxJobs <= 0) {
    return;
  }

  int32 Processed = 0;
  bool bPreferLightweight = true;
  const double BulkTickStartSec = bBulkLoadDrainActive ? FPlatformTime::Seconds() : 0.0;

  while (Processed < MaxJobs) {
    if (bBulkLoadDrainActive && (FPlatformTime::Seconds() - BulkTickStartSec) >=
                                    StructuralPowerConstants::BulkLoadDrainTickBudgetSec) {
      break;
    }

    const bool bHasLightweight = PendingLightweightJobsHead < PendingLightweightJobs.Num();
    const bool bHasActor = PendingJobsHead < PendingJobs.Num();

    if (!bHasLightweight && !bHasActor) {
      break;
    }

    if (bPreferLightweight && bHasLightweight) {
      const FStructuralLightweightKey Key = PendingLightweightJobs[PendingLightweightJobsHead++];
      NoteLightweightDequeued(Key);
      ProcessLightweightJob(Key);
      bPreferLightweight = !bPreferLightweight;
      ++Processed;
      continue;
    }

    if (bHasActor) {
      const FDeferredPlacementJob Job = PendingJobs[PendingJobsHead++];
      NoteBuildableDequeued(Job);
      if (AFGBuildable* Buildable = Job.Buildable.Get()) {
        ProcessBuildableJob(Buildable, Job.JobType);
        bPreferLightweight = !bPreferLightweight;
        ++Processed;
      }

      continue;
    }

    if (bHasLightweight) {
      const FStructuralLightweightKey Key = PendingLightweightJobs[PendingLightweightJobsHead++];
      NoteLightweightDequeued(Key);
      ProcessLightweightJob(Key);
      bPreferLightweight = !bPreferLightweight;
      ++Processed;
      continue;
    }

    break;
  }

  if ((PendingJobsHead > 32 && PendingJobsHead * 2 >= PendingJobs.Num()) ||
      (PendingLightweightJobsHead > 32 &&
       PendingLightweightJobsHead * 2 >= PendingLightweightJobs.Num())) {
    Compact();
  }

  if (GetPendingCount() == 0) {
    OnQueuesIdle();
  }
}

void FStructuralPlacementQueue::RemoveBuildable(AFGBuildable* Buildable) {
  Compact();
  PendingJobs.RemoveAll([this, Buildable](const FDeferredPlacementJob& Job) {
    if (Job.Buildable.Get() != Buildable) {
      return false;
    }
    PendingBuildableKeys.Remove(FPendingBuildableKey{Job.Buildable, Job.JobType});
    return true;
  });
}

void FStructuralPlacementQueue::RemoveLightweight(const FStructuralLightweightKey& Key) {
  Compact();
  PendingLightweightKeys.Remove(Key);
  PendingLightweightJobs.RemoveAll(
      [&Key](const FStructuralLightweightKey& Pending) { return Pending == Key; });
}

void FStructuralPlacementQueue::ForEachPendingOutletBuildable(
    TFunctionRef<void(AFGBuildable*)> Visitor) const {
  for (int32 Index = PendingJobsHead; Index < PendingJobs.Num(); ++Index) {
    const FDeferredPlacementJob& Job = PendingJobs[Index];
    if (Job.JobType != EStructuralPlacementJobType::Outlet) {
      continue;
    }

    if (AFGBuildable* Buildable = Job.Buildable.Get()) {
      Visitor(Buildable);
    }
  }
}

void FStructuralPlacementQueue::Compact() {
  if (PendingJobsHead > 0) {
    PendingJobs.RemoveAt(0, PendingJobsHead, EAllowShrinking::No);
    PendingJobsHead = 0;
  }

  if (PendingLightweightJobsHead > 0) {
    PendingLightweightJobs.RemoveAt(0, PendingLightweightJobsHead, EAllowShrinking::No);
    PendingLightweightJobsHead = 0;
  }
}
