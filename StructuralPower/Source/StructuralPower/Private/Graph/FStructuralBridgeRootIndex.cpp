// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralBridgeRootIndex.h"

#include "Core/FStructuralGraphSession.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerLog.h"

void FStructuralBridgeRootIndex::Bind(FStructuralGraphSession* InSession) {
  Session = InSession;
}

void FStructuralBridgeRootIndex::MarkBridgeEndpointRootIndexDirty() {
  Session->BridgeEndpointRootIndexDirty() = true;
  Session->SourceConnectorByRoot().Empty();
}

void FStructuralBridgeRootIndex::RefreshBridgeEndpointRootIndex() {
  if (!Session->BridgeEndpointRootIndexDirty()) {
    return;
  }

  HALSP_TRACE_SCOPE(TEXT("mod"), TEXT("graph.RefreshBridgeEndpointRootIndex"));

  Session->EndpointIndex().RebuildFrom(Session->TrackedEndpoints(), Session->StructureGraph());
  Session->BridgeEndpointRootIndexDirty() = false;
}

void FStructuralBridgeRootIndex::RekeyEndpointIndexForRoots(const TSet<int32>& Roots) {
  if (!Session || Roots.Num() == 0) {
    return;
  }

  for (int32 Root : Roots) {
    Session->EndpointIndex().RemoveRoot(Root);
    Session->SourceConnectorByRoot().Remove(Root);
  }

  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints()) {
    if (!Pair.Value.MountParentId.IsValid()) {
      continue;
    }
    if (Pair.Value.Kind == EStructuralEndpointKind::Pole &&
        !Pair.Value.bStructuralPowerTransferActive) {
      continue;
    }

    const int32 Root = Session->StructureGraph().FindRoot(Pair.Value.MountParentId);
    if (Root == INDEX_NONE || !Roots.Contains(Root)) {
      continue;
    }

    Session->EndpointIndex().Add(Root, Pair.Value.Kind, Pair.Key);
  }
}

void FStructuralBridgeRootIndex::AddEndpointToRootIndex(int32 Root, EStructuralEndpointKind Kind,
                                                        const FStructuralNodeId& EndpointId) {
  if (Root == INDEX_NONE || !EndpointId.IsValid()) {
    return;
  }

  if (Session->BridgeEndpointRootIndexDirty()) {
    return;
  }

  Session->EndpointIndex().Add(Root, Kind, EndpointId);
}

int32 FStructuralBridgeRootIndex::FindRootForTrackedEndpoint(
    const FTrackedEndpoint& Tracked) const {
  if (!Tracked.MountParentId.IsValid()) {
    return INDEX_NONE;
  }

  return Session->StructureGraph().FindRoot(Tracked.MountParentId);
}

int32 FStructuralBridgeRootIndex::ResolveBridgeRootFromAnchor(AFGBuildable* Host,
                                                              const FStructuralWallAnchor& Anchor,
                                                              FStructuralNodeId& OutParentId,
                                                              bool bPreferBulkResolve) {
  if (bPreferBulkResolve) {
    if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host)) {
      return ResolvePoleComponentRoot(Pole, Anchor, OutParentId);
    }

    return ResolveBridgeComponentRootBulk(Host, Anchor, OutParentId);
  }

  return ResolveEndpointComponentRoot(Host, Anchor, OutParentId);
}

int32 FStructuralBridgeRootIndex::ResolveBridgeComponentRootBulk(
    AFGBuildable* Host, const FStructuralWallAnchor& Anchor, FStructuralNodeId& OutParentId) {
  OutParentId = FStructuralNodeId();
  if (!IsValid(Host)) {
    return INDEX_NONE;
  }

  if (Anchor.IsValid()) {
    const FStructuralNodeId ParentId = MakeParentNodeId(Anchor);
    const int32 Root = Session->StructureGraph().FindRoot(ParentId);
    if (Root != INDEX_NONE) {
      OutParentId = ParentId;
      return Root;
    }
  }

  return INDEX_NONE;
}

int32 FStructuralBridgeRootIndex::ResolvePoleComponentRoot(AFGBuildablePowerPole* Pole,
                                                           const FStructuralWallAnchor& Anchor,
                                                           FStructuralNodeId& OutParentId) {
  return ResolveBridgeComponentRootBulk(Pole, Anchor, OutParentId);
}

bool FStructuralBridgeRootIndex::EnsureParentRegisteredInGraph(const FStructuralWallAnchor& Anchor,
                                                               FStructuralNodeId& OutParentId) {
  if (!Anchor.IsValid()) {
    return false;
  }

  const FStructuralNodeId ParentId = MakeParentNodeId(Anchor);
  if (Session->StructureGraph().FindRoot(ParentId) != INDEX_NONE) {
    OutParentId = ParentId;
    return true;
  }

  if (IsValid(Anchor.Actor) && FStructuralEligibilityRules::IsBusMember(Anchor.Actor)) {
    Session->ProcessStructure(Anchor.Actor);
  } else if (Anchor.Lightweight.IsValid()) {
    Session->ProcessLightweightStructure(Anchor.Lightweight);
  } else {
    return false;
  }

  if (Session->StructureGraph().FindRoot(ParentId) != INDEX_NONE) {
    OutParentId = ParentId;
    UE_LOG(LogStructuralPower, Verbose,
           TEXT("[HALSP] parent ingested into structure graph actor=%s lw=%s[%d]"),
           IsValid(Anchor.Actor) ? *Anchor.Actor->GetName() : TEXT("null"),
           Anchor.Lightweight.IsValid() ? *Anchor.Lightweight.BuildableClass->GetName()
                                        : TEXT("null"),
           Anchor.Lightweight.IsValid() ? Anchor.Lightweight.Index : INDEX_NONE);
    return true;
  }

  return false;
}

int32 FStructuralBridgeRootIndex::ResolveEndpointComponentRoot(AFGBuildable* Endpoint,
                                                               const FStructuralWallAnchor& Anchor,
                                                               FStructuralNodeId& OutParentId) {
  if (EnsureParentRegisteredInGraph(Anchor, OutParentId)) {
    return Session->StructureGraph().FindRoot(OutParentId);
  }

  return FStructuralAttachmentResolver::ResolveComponentRootForBuildable(
      Endpoint, Session->StructureGraph(), Session->LightweightIndex(), Session->GetWorld(),
      OutParentId);
}

int32 FStructuralBridgeRootIndex::ResolveBridgeHostComponentRoot(AFGBuildable* Host,
                                                                 FStructuralNodeId* OutParentId) {
  if (!IsValid(Host)) {
    if (OutParentId) {
      *OutParentId = FStructuralNodeId();
    }
    return INDEX_NONE;
  }

  FStructuralNodeId ParentId;
  const FStructuralWallAnchor Anchor = ResolveOutletAnchor(Host);
  const int32 Root = ResolveEndpointComponentRoot(Host, Anchor, ParentId);
  if (OutParentId) {
    *OutParentId = ParentId;
  }

  if (ParentId.IsValid()) {
    if (FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(Session->MakeNodeId(Host))) {
      Tracked->MountParentId = ParentId;
    }
  }

  return Root;
}

int32 FStructuralBridgeRootIndex::GetEndpointComponentRoot(AFGBuildable* Endpoint) {
  if (!IsValid(Endpoint)) {
    return INDEX_NONE;
  }

  const FStructuralWallAnchor Anchor = ResolveOutletAnchor(Endpoint);
  FStructuralNodeId ParentId;
  return ResolveEndpointComponentRoot(Endpoint, Anchor, ParentId);
}

FStructuralOutletParentResolveParams
FStructuralBridgeRootIndex::MakeOutletParentResolveParams() const {
  FStructuralOutletParentResolveParams Params;
  Params.LightweightIndex = &Session->LightweightIndex();
  Params.BusMemberIndex = &Session->BusMemberSpatialIndex();
  Params.StructureGraph = &Session->StructureGraph();
  Params.bAllowLiveScan = !Session->IsBulkLoadDrainActive();
  Params.ResolveActorFromNodeId = [this](const FStructuralNodeId& NodeId) -> AFGBuildable* {
    if (const TWeakObjectPtr<AFGBuildable>* Entry = Session->RegisteredBuildables().Find(NodeId)) {
      return Entry->Get();
    }
    return nullptr;
  };
  return Params;
}

FStructuralWallAnchor FStructuralBridgeRootIndex::ResolveOutletAnchor(AFGBuildable* Outlet) const {
  return FStructuralAttachmentResolver::ResolveStructuralParent(Outlet, Session->GetWorld(),
                                                                MakeOutletParentResolveParams());
}

FStructuralNodeId
FStructuralBridgeRootIndex::MakeParentNodeId(const FStructuralWallAnchor& Anchor) const {
  if (IsValid(Anchor.Actor)) {
    return Session->MakeNodeId(Anchor.Actor);
  }

  if (Anchor.Lightweight.IsValid()) {
    return FStructuralLightweightIndex::MakeNodeId(Anchor.Lightweight);
  }

  return FStructuralNodeId();
}
