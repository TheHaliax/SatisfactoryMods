// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphBulkDrain.h"

#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralBridgeRootIndex.h"

#include "Attach/FStructuralPowerTransferGate.h"
#include "Attach/FStructuralSwitchBridgeStrategy.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

namespace {
static TSet<int32> CollectBridgeRoots(FStructuralGraphSession& Session) {
  TSet<int32> Roots;
  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session.TrackedEndpoints()) {
    if (Pair.Value.Kind != EStructuralEndpointKind::Pole &&
        Pair.Value.Kind != EStructuralEndpointKind::Switch &&
        Pair.Value.Kind != EStructuralEndpointKind::Storage &&
        Pair.Value.Kind != EStructuralEndpointKind::Generator) {
      continue;
    }

    if (Pair.Value.Kind == EStructuralEndpointKind::Pole &&
        !Pair.Value.bStructuralPowerTransferActive) {
      continue;
    }

    const int32 Root = Session.StructureGraph().FindRoot(Pair.Value.MountParentId);
    if (Root != INDEX_NONE) {
      Roots.Add(Root);
    }
  }

  return Roots;
}
}  // namespace

void FStructuralGraphBulkDrain::Bind(FStructuralGraphSession* InSession) {
  Session = InSession;
}

void FStructuralGraphBulkDrain::ResetRemeshState() {
  RemeshQueue.Reset();
  RemeshHubByRoot.Reset();
  RemeshHead = 0;
  bRemeshPrepared = false;
}

void FStructuralGraphBulkDrain::PrepareRemeshQueue() {
  RemeshQueue.Reset();
  RemeshHubByRoot.Reset();
  RemeshHead = 0;

  TArray<FStructuralNodeId> Poles;
  TArray<FStructuralNodeId> Storage;
  TArray<FStructuralNodeId> Generators;
  TArray<FStructuralNodeId> Switches;

  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints()) {
    const EStructuralEndpointKind Kind = Pair.Value.Kind;
    if (Kind != EStructuralEndpointKind::Pole && Kind != EStructuralEndpointKind::Switch &&
        Kind != EStructuralEndpointKind::Storage && Kind != EStructuralEndpointKind::Generator) {
      continue;
    }

    if (Kind == EStructuralEndpointKind::Pole && !Pair.Value.bStructuralPowerTransferActive) {
      continue;
    }

    if (!IsValid(Pair.Value.Actor.Get()) || !Pair.Value.MountParentId.IsValid()) {
      continue;
    }

    if (Session->StructureGraph().FindRoot(Pair.Value.MountParentId) == INDEX_NONE) {
      continue;
    }

    if (Kind == EStructuralEndpointKind::Pole) {
      Poles.Add(Pair.Key);
    } else if (Kind == EStructuralEndpointKind::Storage) {
      Storage.Add(Pair.Key);
    } else if (Kind == EStructuralEndpointKind::Generator) {
      Generators.Add(Pair.Key);
    } else {
      Switches.Add(Pair.Key);
    }
  }

  RemeshQueue.Reserve(Poles.Num() + Storage.Num() + Generators.Num() + Switches.Num());
  RemeshQueue.Append(Poles);
  RemeshQueue.Append(Storage);
  RemeshQueue.Append(Generators);
  RemeshQueue.Append(Switches);

  bRemeshPrepared = true;
}

bool FStructuralGraphBulkDrain::TickRemeshBudget() {
  const double StartSec = FPlatformTime::Seconds();
  FStructuralPowerContext Ctx = Session->MakeProcessorContext(EAttachContext::BulkLoad);

  while (RemeshHead < RemeshQueue.Num()) {
    if ((FPlatformTime::Seconds() - StartSec) >=
        StructuralPowerConstants::BulkLoadDrainTickBudgetSec) {
      return false;
    }

    const FStructuralNodeId& EndpointId = RemeshQueue[RemeshHead++];
    const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(EndpointId);
    if (!Tracked) {
      continue;
    }

    AFGBuildable* Host = Tracked->Actor.Get();
    if (!IsValid(Host) || !Tracked->MountParentId.IsValid()) {
      continue;
    }

    const int32 Root = Session->StructureGraph().FindRoot(Tracked->MountParentId);
    if (Root == INDEX_NONE) {
      continue;
    }

    UFGStructuralPowerConnectionComponent* Bus = Session->GetOrCreateBusConnector(Host);
    if (!IsValid(Bus)) {
      continue;
    }

    if (Tracked->Kind != EStructuralEndpointKind::Switch) {
      Session->Circuit().LinkBusToVisibleConnectionsLocal(Host, Bus, /*bMeshOnlyLinks=*/false);
    }

    if (const FStructuralNodeId* HubId = RemeshHubByRoot.Find(Root)) {
      if (*HubId != EndpointId) {
        const FTrackedEndpoint* HubTracked = Session->TrackedEndpoints().Find(*HubId);
        AFGBuildable* HubHost = HubTracked ? HubTracked->Actor.Get() : nullptr;
        if (UFGStructuralPowerConnectionComponent* HubBus = Session->FindBusConnector(HubHost)) {
          if (!Bus->HasHiddenConnection(HubBus)) {
            Session->Circuit().LinkHiddenPairLocal(Bus, HubBus, /*bPromoteCircuit=*/true);
          }
        }
      }
    } else {
      RemeshHubByRoot.Add(Root, EndpointId);
    }

    if (Tracked->Kind != EStructuralEndpointKind::Switch) {
      Session->Circuit().PromoteOutletBusIfPowered(Bus, /*bLocalPromoteOnly=*/false);
    }
  }

  return true;
}

void FStructuralGraphBulkDrain::FinalizeAfterRemesh() {
  const TSet<int32> Roots = CollectBridgeRoots(*Session);

  if (Roots.Num() > 0) {
    const TArray<int32> RootArray = Roots.Array();

    FStructuralPowerContext BulkCtx = Session->MakeProcessorContext(EAttachContext::BulkLoad);
    for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints()) {
      if (Pair.Value.Kind != EStructuralEndpointKind::Switch) {
        continue;
      }

      if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch()) {
        FStructuralSwitchBridgeStrategy::ApplyMembership(BulkCtx, Switch);
      }
    }

    for (int32 Root : RootArray) {
      FStructuralPowerContext RootCtx =
          Session->MakeProcessorContext(EAttachContext::BulkLoad, Root);
      FStructuralPowerTransferGate::RefreshKeyedTransferOnRoot(RootCtx, Root,
                                                               /*bLocalPromoteOnly=*/false);
    }

    for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints()) {
      if (Pair.Value.Kind != EStructuralEndpointKind::Switch) {
        continue;
      }

      if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch()) {
        FStructuralPowerSwitchProcessor::EnsureListener(BulkCtx, Switch);
      }
    }
  }

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] Post-load bulk drain complete — %d bridge component root(s) remeshed=%d"),
         Roots.Num(), RemeshQueue.Num());

  Session->CrossSite().SeedFeedSignaturesForSites(*Session, Roots);
  ResetRemeshState();
}

void FStructuralGraphBulkDrain::FinishBulkLoadDrain() {
  if (!Session || !Session->BulkLoadDrainActive()) {
    return;
  }

  if (!bRemeshPrepared) {
    Session->BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
    Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();
    PrepareRemeshQueue();
  }

  if (!TickRemeshBudget()) {
    Session->NotifyDeferredWorkRegistered();
    return;
  }

  FinalizeAfterRemesh();
}
