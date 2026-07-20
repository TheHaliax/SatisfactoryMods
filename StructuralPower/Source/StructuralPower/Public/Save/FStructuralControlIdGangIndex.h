// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

enum class EStructuralControlGangScope : uint8 { Site, Global };

struct FStructuralControlGangKey {
  EStructuralControlGangScope Scope = EStructuralControlGangScope::Site;
  FStructuralComponentKey ComponentKey;
  FName ControlId = NAME_None;

  bool IsValid() const {
    if (ControlId.IsNone()) {
      return false;
    }

    if (Scope == EStructuralControlGangScope::Global) {
      return true;
    }

    return ComponentKey.IsValid();
  }

  bool operator==(const FStructuralControlGangKey& Other) const {
    if (Scope != Other.Scope || ControlId != Other.ControlId) {
      return false;
    }

    if (Scope == EStructuralControlGangScope::Global) {
      return true;
    }

    return ComponentKey == Other.ComponentKey;
  }

  friend uint32 GetTypeHash(const FStructuralControlGangKey& Key) {
    uint32 Hash =
        HashCombine(::GetTypeHash(static_cast<uint8>(Key.Scope)), GetTypeHash(Key.ControlId));
    if (Key.Scope == EStructuralControlGangScope::Site) {
      Hash = HashCombine(Hash, GetTypeHash(Key.ComponentKey));
    }
    return Hash;
  }
};

class STRUCTURALPOWER_API FStructuralControlIdGangIndex {
 public:
  void ClearComponent(const FStructuralComponentKey& ComponentKey);

  void RegisterPublisher(const FStructuralNodeId& NodeId, const FStructuralControlGangKey& GangKey);

  void RemoveNode(const FStructuralNodeId& NodeId);

  TArray<FStructuralNodeId> GetGangMembers(const FStructuralComponentKey& ComponentKey,
                                           FName ControlId) const;

  TArray<FStructuralNodeId> GetGlobalGangMembers(FName ControlId) const;

  int32 GetGangMemberCount(const FStructuralComponentKey& ComponentKey, FName ControlId) const;

 private:
  TMap<FStructuralControlGangKey, TArray<FStructuralNodeId>> GangsByControlId;
  TMap<FStructuralNodeId, FStructuralControlGangKey> NodeToGangKey;
};
