// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralEndpointIndex.h"

#include "Graph/FStructuralConnectivityGraph.h"

void FStructuralEndpointIndex::Reset() {
  ByRoot.Empty();
}

void FStructuralEndpointIndex::RebuildFrom(const TMap<FStructuralNodeId, FTrackedEndpoint>& Tracked,
                                           const FStructuralConnectivityGraph& Graph) {
  Reset();

  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Tracked) {
    if (!Pair.Value.MountParentId.IsValid()) {
      continue;
    }

    if (Pair.Value.Kind == EStructuralEndpointKind::Pole &&
        !Pair.Value.bStructuralPowerTransferActive) {
      continue;
    }

    const int32 Root = Graph.FindRoot(Pair.Value.MountParentId);
    if (Root == INDEX_NONE) {
      continue;
    }

    Add(Root, Pair.Value.Kind, Pair.Key);
  }
}

void FStructuralEndpointIndex::Add(int32 Root, EStructuralEndpointKind Kind,
                                   const FStructuralNodeId& EndpointId) {
  if (Root == INDEX_NONE || !EndpointId.IsValid()) {
    return;
  }

  ByRoot.FindOrAdd(Root).FindOrAdd(Kind).AddUnique(EndpointId);
}

void FStructuralEndpointIndex::RemoveRoot(int32 Root) {
  if (Root == INDEX_NONE) {
    return;
  }
  ByRoot.Remove(Root);
}

const TArray<FStructuralNodeId>* FStructuralEndpointIndex::Get(int32 Root,
                                                               EStructuralEndpointKind Kind) const {
  const TMap<EStructuralEndpointKind, TArray<FStructuralNodeId>>* KindMap = ByRoot.Find(Root);
  if (!KindMap) {
    return nullptr;
  }

  return KindMap->Find(Kind);
}

void FStructuralEndpointIndex::ForEachOnRoot(
    int32 Root, TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const {
  const TMap<EStructuralEndpointKind, TArray<FStructuralNodeId>>* KindMap = ByRoot.Find(Root);
  if (!KindMap) {
    return;
  }

  for (const TPair<EStructuralEndpointKind, TArray<FStructuralNodeId>>& Pair : *KindMap) {
    for (const FStructuralNodeId& EndpointId : Pair.Value) {
      Visitor(EndpointId);
    }
  }
}

void FStructuralEndpointIndex::ForEachEndpoint(
    EStructuralEndpointKind Kind,
    TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const {
  for (const TPair<int32, TMap<EStructuralEndpointKind, TArray<FStructuralNodeId>>>& RootPair :
       ByRoot) {
    if (const TArray<FStructuralNodeId>* Endpoints = RootPair.Value.Find(Kind)) {
      for (const FStructuralNodeId& EndpointId : *Endpoints) {
        Visitor(EndpointId);
      }
    }
  }
}

void FStructuralEndpointIndex::ForEachBridgeOnRoot(
    int32 Root, TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const {
  if (const TArray<FStructuralNodeId>* Poles = Get(Root, EStructuralEndpointKind::Pole)) {
    for (const FStructuralNodeId& EndpointId : *Poles) {
      Visitor(EndpointId);
    }
  }

  if (const TArray<FStructuralNodeId>* Switches = Get(Root, EStructuralEndpointKind::Switch)) {
    for (const FStructuralNodeId& EndpointId : *Switches) {
      Visitor(EndpointId);
    }
  }

  if (const TArray<FStructuralNodeId>* Storages = Get(Root, EStructuralEndpointKind::Storage)) {
    for (const FStructuralNodeId& EndpointId : *Storages) {
      Visitor(EndpointId);
    }
  }

  if (const TArray<FStructuralNodeId>* Generators = Get(Root, EStructuralEndpointKind::Generator)) {
    for (const FStructuralNodeId& EndpointId : *Generators) {
      Visitor(EndpointId);
    }
  }
}
