// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphStructureIngress.h"

#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralNodeId.h"
#include "Graph/FStructuralBridgeRootIndex.h"

#include "Buildables/FGBuildable.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/World.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralGraphStructureIngress::Bind(FStructuralGraphSession* InSession) {
  Session = InSession;
}

void FStructuralGraphStructureIngress::ProcessStructure(AFGBuildable* Buildable) {
  if (!Session) {
    return;
  }

  if (!FStructuralEligibilityRules::IsBusMember(Buildable)) {
    FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bus_member"));
    return;
  }

  Session->RegisterBuildableActor(Buildable);

  TArray<int32> MergedRoots;
  if (!Session->StructureGraph().AddActorNode(Buildable, MergedRoots)) {
    return;
  }

  if (MergedRoots.Num() > 0) {
    Session->BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  if (MergedRoots.Num() >= 2) {
    const int32 Root = Session->StructureGraph().FindRoot(Session->MakeNodeId(Buildable));

    UE_LOG(LogStructuralPower, Verbose,
           TEXT("[HALSP] structure %s fused %d component(s) -> root %d"), *Buildable->GetName(),
           MergedRoots.Num(), Root);
  }

  RetryAwaitingStructuralSiteEndpoints();
}

void FStructuralGraphStructureIngress::ProcessLightweightStructure(
    const FStructuralLightweightKey& Key) {
  if (!Session || !Key.IsValid()) {
    return;
  }

  UWorld* World = Session->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  Session->LightweightIndex().RegisterTrackedMember(World, Key);

  TArray<int32> MergedRoots;
  if (!Session->StructureGraph().AddLightweightNode(World, Key, MergedRoots)) {
    return;
  }

  if (MergedRoots.Num() > 0) {
    Session->BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  if (MergedRoots.Num() >= 2) {
    (void)Session->StructureGraph().FindRoot(FStructuralLightweightIndex::MakeNodeId(Key));
  }

  RetryAwaitingStructuralSiteEndpoints();
}

void FStructuralGraphStructureIngress::RetryAwaitingStructuralSiteEndpoints() {
  for (TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints()) {
    if (!Pair.Value.bAwaitingStructuralSite) {
      continue;
    }

    const bool bSiteRetryKind = Pair.Value.Kind == EStructuralEndpointKind::Pole ||
                                Pair.Value.Kind == EStructuralEndpointKind::Switch ||
                                Pair.Value.Kind == EStructuralEndpointKind::Storage ||
                                Pair.Value.Kind == EStructuralEndpointKind::Light ||
                                Pair.Value.Kind == EStructuralEndpointKind::Panel ||
                                Pair.Value.Kind == EStructuralEndpointKind::Generator ||
                                Pair.Value.Kind == EStructuralEndpointKind::Extractor ||
                                Pair.Value.Kind == EStructuralEndpointKind::Manufacturer ||
                                Pair.Value.Kind == EStructuralEndpointKind::Transport ||
                                Pair.Value.Kind == EStructuralEndpointKind::PipePump;
    if (!bSiteRetryKind) {
      continue;
    }

    AFGBuildable* Buildable = Pair.Value.Actor.Get();
    if (!IsValid(Buildable)) {
      continue;
    }

    const bool bUsePoleResolver = Pair.Value.Kind == EStructuralEndpointKind::Pole;
    if (FStructuralSiteMembership::ReaffirmMountParent(*Session, Buildable, Pair.Value,
                                                       bUsePoleResolver)) {
      Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet,
                                /*bDefer=*/true);
    }
  }
}

void FStructuralGraphStructureIngress::MarkOrphanMountEndpointsAwaiting(
    const TSet<int32>& AffectedRoots) {
  if (!Session) {
    return;
  }

  const bool bScoped = AffectedRoots.Num() > 0;
  int32 Orphans = 0;
  for (TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints()) {
    if (!Pair.Value.MountParentId.IsValid()) {
      if (!Pair.Value.bAwaitingStructuralSite) {
        Pair.Value.bAwaitingStructuralSite = true;
        ++Orphans;
      }
      continue;
    }

    const int32 Root = Session->StructureGraph().FindRoot(Pair.Value.MountParentId);
    if (Root == INDEX_NONE) {
      Pair.Value.MountParentId = FStructuralNodeId();
      Pair.Value.bAwaitingStructuralSite = true;
      ++Orphans;
      continue;
    }

    if (bScoped && !AffectedRoots.Contains(Root)) {
      continue;
    }
  }

  if (Orphans > 0) {
    UE_LOG(LogStructuralPower, Log,
           TEXT("[HALSP] structure split orphanMounts=%d (remount queued)"), Orphans);
  }
}

namespace {
bool IsStructureSplitConsumerKind(const EStructuralEndpointKind Kind) {
  return Kind == EStructuralEndpointKind::Extractor ||
         Kind == EStructuralEndpointKind::Manufacturer ||
         Kind == EStructuralEndpointKind::Transport || Kind == EStructuralEndpointKind::PipePump ||
         Kind == EStructuralEndpointKind::Light || Kind == EStructuralEndpointKind::Panel;
}
}  // namespace

void FStructuralGraphStructureIngress::RequeueConsumersAfterStructureSplit(
    const TSet<int32>& AffectedRoots) {
  if (!Session) {
    return;
  }

  const bool bScoped = AffectedRoots.Num() > 0;
  int32 Requeued = 0;
  bool bHitCap = false;

  for (TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints()) {
    if (Requeued >= StructuralPowerConstants::MaxStructureSplitConsumerRequeue) {
      bHitCap = true;
      break;
    }

    if (!IsStructureSplitConsumerKind(Pair.Value.Kind)) {
      continue;
    }

    AFGBuildable* Host = Pair.Value.Actor.Get();
    if (!IsValid(Host)) {
      continue;
    }

    const int32 SelfRoot = Pair.Value.MountParentId.IsValid()
                               ? Session->StructureGraph().FindRoot(Pair.Value.MountParentId)
                               : INDEX_NONE;
    if (bScoped && SelfRoot != INDEX_NONE && !AffectedRoots.Contains(SelfRoot)) {
      continue;
    }

    Session->DispatchTeardown(Host);
    Pair.Value.bStructuralPowerTransferActive = false;
    Pair.Value.bAwaitingStructuralSite = true;

    if (!Session->IsBuildablePlacementPending(Host)) {
      Session->EnqueuePlacement(Host, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
    }
    ++Requeued;
  }

  if (Requeued > 0 || bHitCap) {
    UE_LOG(LogStructuralPower, Log,
           TEXT("[HALSP] structure split consumerRequeue=%d capped=%d"), Requeued,
           bHitCap ? 1 : 0);
  }
}

void FStructuralGraphStructureIngress::ReconcileAfterStructureSplit(
    const TSet<int32>& AffectedRoots) {
  if (!Session) {
    return;
  }

  Session->Circuit().PruneBridgePeerMeshForRoots(AffectedRoots);
  MarkOrphanMountEndpointsAwaiting(AffectedRoots);
  RequeueConsumersAfterStructureSplit(AffectedRoots);

  if (AffectedRoots.Num() > 0) {
    Session->BridgeRootIndex().RekeyEndpointIndexForRoots(AffectedRoots);
  } else {
    Session->BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  RetryAwaitingStructuralSiteEndpoints();
}
