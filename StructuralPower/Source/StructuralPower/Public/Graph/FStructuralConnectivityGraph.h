// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;
class UWorld;

class STRUCTURALPOWER_API FStructuralConnectivityGraph {
 public:
  void Reset();

  void Rebuild(UWorld* World);

  bool AddActorNode(AFGBuildable* Buildable, TArray<int32>& OutMergedRoots);
  bool AddLightweightNode(UWorld* World, const FStructuralLightweightKey& Key,
                          TArray<int32>& OutMergedRoots);

  void RemoveNode(const FStructuralNodeId& Id, TArray<int32>& OutAffectedRoots);

  /** One CollectComponent + one survivor re-union for many deletes (mass dismantle). */
  void RemoveNodesBatched(const TArray<FStructuralNodeId>& Ids, TArray<int32>& OutAffectedRoots);

  int32 FindRoot(const FStructuralNodeId& Id) const;

  int32 FindRootForBounds(const FBox& Bounds, TSubclassOf<AFGBuildable> Class,
                          FStructuralNodeId* OutBestNodeId = nullptr) const;

  bool IsTracked(const FStructuralNodeId& Id) const {
    return IdToIndex.Contains(Id);
  }
  int32 GetNodeCount() const {
    return NumValid;
  }
  void GetComponentStats(int32& OutComponents, int32& OutLargest);

  FStructuralNodeId MakeCanonicalNodeIdForComponent(int32 Root) const;

  bool FindNearestStructureAnchor(const FVector& QueryLoc, float MaxHorizontal, float MaxVertical,
                                  FVector& OutAnchor, int32& OutComponentRoot) const;

 private:
  struct FNode {
    FStructuralNodeId Id;
    FBox Bounds = FBox(ForceInit);
    TSubclassOf<AFGBuildable> Class = nullptr;
    bool bValid = false;
  };

  static FIntVector ToCell(const FVector& P);
  static FBox ComputeLightweightBounds(UWorld* World, const FStructuralLightweightKey& Key,
                                       TSubclassOf<AFGBuildable>& OutClass);

  int32 AllocSlot();
  int32 AddNodeInternal(const FStructuralNodeId& Id, const FBox& Bounds,
                        TSubclassOf<AFGBuildable> Class, TArray<int32>& OutMergedRoots);
  void IndexCells(int32 NodeIndex);
  void UnindexCells(int32 NodeIndex);
  void CollectNeighbors(int32 NodeIndex, TArray<int32>& Out) const;
  void CollectComponent(int32 StartIndex, TArray<int32>& Out) const;
  void InvalidateSlot(int32 Index);
  void ReuniteSurvivors(const TArray<int32>& Survivors, int32 OldRootHint,
                        TArray<int32>& OutAffectedRoots);
  int32 Find(int32 Index);
  int32 FindRootIndexReadOnly(int32 Index) const;
  void Union(int32 A, int32 B);
  void InvalidateCanonicalCache() const;

  TArray<FNode> Nodes;
  TArray<int32> Parent;
  TArray<int32> RankArr;
  /** Valid only while Parent[i]==i — slot indices in that component. */
  TArray<TArray<int32>> Members;
  TArray<int32> FreeSlots;
  TMap<FStructuralNodeId, int32> IdToIndex;
  TMap<FIntVector, TArray<int32>> Cells;
  int32 NumValid = 0;
  mutable TMap<int32, FStructuralNodeId> CanonicalByRoot;
};
