// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphRemoval.h"

#include "Buildables/FGBuildable.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/FStructuralGraphSession.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralBridgeRootIndex.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralGraphStructureIngress.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Processors/FStructuralEndpointDispatcher.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Save/FStructuralGraphIdOps.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerLog.h"

void FStructuralGraphRemoval::Bind(FStructuralGraphSession* InSession) {
  Session = InSession;
}

void FStructuralGraphRemoval::AfterStructureNodeRemoved(const TArray<int32>& AffectedRoots,
                                                        const TCHAR* Reason) {
  if (!Session) {
    return;
  }

  Session->NoteStructureSplitAffectedRoots(AffectedRoots);

  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] structure remove %s affectedRoots=%d"),
         Reason ? Reason : TEXT("?"), AffectedRoots.Num());

  ScheduleStructureSplitReconcile();
}

void FStructuralGraphRemoval::ScheduleStructureSplitReconcile() {
  if (!Session) {
    return;
  }

  Session->ScheduleStructureSplitReconcile();
}

void FStructuralGraphRemoval::RunStructureSplitReconcile() {
  if (!Session) {
    return;
  }

  TArray<FStructuralNodeId> PendingNodes = MoveTemp(Session->PendingStructureNodeRemovals());
  if (PendingNodes.Num() > 0) {
    TArray<int32> BatchedAffected;
    Session->StructureGraph().RemoveNodesBatched(PendingNodes, BatchedAffected);
    Session->NoteStructureSplitAffectedRoots(BatchedAffected);
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] structure batchRemove nodes=%d affectedRoots=%d"),
           PendingNodes.Num(), BatchedAffected.Num());
  }

  TSet<int32> Roots = MoveTemp(Session->PendingStructureSplitAffectedRoots());

  FStructuralCircuitPromotionScope PromotionScope(&Session->Owner());
  Session->PendingStructureSplitReconcile() = true;
  Session->StructureIngress().ReconcileAfterStructureSplit(Roots);
  Session->PendingStructureSplitReconcile() = false;
}

void FStructuralGraphRemoval::OnBuildableRemoved(AFGBuildable* Buildable) {
  if (!Session || !IsValid(Buildable)) {
    return;
  }

  const FStructuralNodeId NodeId = Session->MakeNodeId(Buildable);
  int32 GangRoot = INDEX_NONE;
  if (const FTrackedEndpoint* TrackedBefore = Session->TrackedEndpoints().Find(NodeId)) {
    GangRoot = Session->StructureGraph().FindRoot(TrackedBefore->MountParentId);
  }

  Session->ControlIdGangIndex().RemoveNode(NodeId);
  Session->UnregisterBuildableActor(NodeId);
  Session->PipeTopology().RemoveBuildable(Buildable, *Session);

  if (FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId)) {
    if (AFGBuildable* Host = Tracked->Actor.Get()) {
      FStructuralEndpointDispatcher::DispatchTeardown(*Session, Host);

      if (Tracked->Kind != EStructuralEndpointKind::Switch &&
          Tracked->Kind != EStructuralEndpointKind::Light &&
          Tracked->Kind != EStructuralEndpointKind::Panel) {
        FStructuralCircuitPromotionScope PromotionScope(&Session->Owner());
        if (UFGStructuralPowerConnectionComponent* Bus = Session->FindBusConnector(Host)) {
          TArray<UFGCircuitConnectionComponent*> HiddenLinks;
          Bus->GetHiddenConnections(HiddenLinks);
          for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks) {
            if (UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw)) {
              Bus->RemoveHiddenConnection(Other);
            }
          }
        }
      }
    }

    Session->TrackedEndpoints().Remove(NodeId);
    if (GangRoot != INDEX_NONE) {
      TArray<int32> Roots;
      Roots.Add(GangRoot);
      Session->NoteStructureSplitAffectedRoots(Roots);
    }
    ScheduleStructureSplitReconcile();
  } else if (Session->StructureGraph().IsTracked(NodeId)) {
    Session->QueueStructureNodeRemoval(NodeId);
  }

  Session->Placement().RemoveBuildable(Buildable);

  if (GangRoot != INDEX_NONE) {
    Session->Ids().RebuildControlIdGangsForRoot(GangRoot);
  }
}

void FStructuralGraphRemoval::OnLightweightRemoved(const FStructuralLightweightKey& Key) {
  if (!Session || !Key.IsValid()) {
    return;
  }

  const FStructuralNodeId NodeId = FStructuralLightweightIndex::MakeNodeId(Key);
  if (Session->StructureGraph().IsTracked(NodeId)) {
    Session->QueueStructureNodeRemoval(NodeId);
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] LW hook queueRemove %s[%d]"),
           Key.BuildableClass ? *Key.BuildableClass->GetName() : TEXT("?"), Key.Index);
  } else {
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] LW remove untracked %s[%d]"),
           Key.BuildableClass ? *Key.BuildableClass->GetName() : TEXT("?"), Key.Index);
  }

  Session->LightweightIndex().UnregisterMember(Key);
  Session->Placement().RemoveLightweight(Key);
}
