// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/FStructuralControlIdGangIndex.h"

void FStructuralControlIdGangIndex::ClearComponent(const FStructuralComponentKey& ComponentKey) {
  TArray<FStructuralNodeId> NodesToClear;
  for (const TPair<FStructuralNodeId, FStructuralControlGangKey>& Pair : NodeToGangKey) {
    if (Pair.Value.ComponentKey == ComponentKey) {
      NodesToClear.Add(Pair.Key);
    }
  }

  for (const FStructuralNodeId& NodeId : NodesToClear) {
    RemoveNode(NodeId);
  }
}

void FStructuralControlIdGangIndex::RegisterPublisher(const FStructuralNodeId& NodeId,
                                                      const FStructuralControlGangKey& GangKey) {
  if (!GangKey.IsValid()) {
    return;
  }

  RemoveNode(NodeId);
  TArray<FStructuralNodeId>& Members = GangsByControlId.FindOrAdd(GangKey);
  Members.AddUnique(NodeId);
  NodeToGangKey.Add(NodeId, GangKey);
}

void FStructuralControlIdGangIndex::RemoveNode(const FStructuralNodeId& NodeId) {
  const FStructuralControlGangKey* GangKey = NodeToGangKey.Find(NodeId);
  if (!GangKey) {
    return;
  }

  if (TArray<FStructuralNodeId>* Members = GangsByControlId.Find(*GangKey)) {
    Members->Remove(NodeId);
    if (Members->IsEmpty()) {
      GangsByControlId.Remove(*GangKey);
    }
  }

  NodeToGangKey.Remove(NodeId);
}

TArray<FStructuralNodeId> FStructuralControlIdGangIndex::GetGangMembers(
    const FStructuralComponentKey& ComponentKey, const FName ControlId) const {
  const FStructuralControlGangKey GangKey{EStructuralControlGangScope::Site, ComponentKey,
                                          ControlId};
  if (const TArray<FStructuralNodeId>* Found = GangsByControlId.Find(GangKey)) {
    return *Found;
  }

  return {};
}

TArray<FStructuralNodeId> FStructuralControlIdGangIndex::GetGlobalGangMembers(
    const FName ControlId) const {
  const FStructuralControlGangKey GangKey{EStructuralControlGangScope::Global,
                                          FStructuralComponentKey{}, ControlId};
  if (const TArray<FStructuralNodeId>* Found = GangsByControlId.Find(GangKey)) {
    return *Found;
  }

  return {};
}

int32 FStructuralControlIdGangIndex::GetGangMemberCount(const FStructuralComponentKey& ComponentKey,
                                                        const FName ControlId) const {
  return GetGangMembers(ComponentKey, ControlId).Num();
}
