// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralConnectivityGraph.h"

#include "Buildables/FGBuildable.h"
#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Routing/EStructuralChannel.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"

namespace {
int32 CompareNodeId(const FStructuralNodeId& A, const FStructuralNodeId& B) {
  const FString PathA = A.BuildableClass.ToString();
  const FString PathB = B.BuildableClass.ToString();
  int32 Cmp = PathA.Compare(PathB);
  if (Cmp != 0) {
    return Cmp;
  }

  Cmp = A.ActorName.Compare(B.ActorName);
  if (Cmp != 0) {
    return Cmp;
  }

  if (A.LightweightIndex < B.LightweightIndex) {
    return -1;
  }
  if (A.LightweightIndex > B.LightweightIndex) {
    return 1;
  }
  return 0;
}
}

namespace {
// Hash query misses gap-boundary pairs without padding the search box.
constexpr float NeighborQueryMarginCm = 200.0f;

FStructuralNodeId MakeActorNodeId(const AFGBuildable* Buildable) {
  FStructuralNodeId Id;
  if (IsValid(Buildable)) {
    Id.BuildableClass = FSoftClassPath(Buildable->GetClass());
    Id.ActorName = Buildable->GetFName();
  }
  return Id;
}
}

void FStructuralConnectivityGraph::InvalidateCanonicalCache() const {
  CanonicalByRoot.Reset();
}

void FStructuralConnectivityGraph::Reset() {
  Nodes.Reset();
  Parent.Reset();
  RankArr.Reset();
  Members.Reset();
  FreeSlots.Reset();
  IdToIndex.Reset();
  Cells.Reset();
  NumValid = 0;
  InvalidateCanonicalCache();
}

FIntVector FStructuralConnectivityGraph::ToCell(const FVector& P) {
  return FIntVector(FMath::FloorToInt(P.X / StructuralPowerConstants::GridCellCm),
                    FMath::FloorToInt(P.Y / StructuralPowerConstants::GridCellCm),
                    FMath::FloorToInt(P.Z / StructuralPowerConstants::GridCellCm));
}

FBox FStructuralConnectivityGraph::ComputeLightweightBounds(UWorld* World,
                                                            const FStructuralLightweightKey& Key,
                                                            TSubclassOf<AFGBuildable>& OutClass) {
  OutClass = Key.BuildableClass;
  if (!IsValid(World) || !Key.IsValid()) {
    return FBox(ForceInit);
  }

  AFGLightweightBuildableSubsystem* Lw = AFGLightweightBuildableSubsystem::Get(World);
  if (!IsValid(Lw)) {
    return FBox(ForceInit);
  }

  const TArray<FRuntimeBuildableInstanceData>* Instances =
      Lw->GetAllLightweightBuildableInstances().Find(Key.BuildableClass);
  if (!Instances || !Instances->IsValidIndex(Key.Index)) {
    return FBox(ForceInit);
  }

  const FRuntimeBuildableInstanceData& Data = (*Instances)[Key.Index];
  if (Data.BoundingBox.IsValid) {
    return Data.BoundingBox.TransformBy(Data.Transform);
  }

  const FVector Loc = Data.Transform.GetLocation();
  const float Extent = StructuralPowerConstants::DefaultFoundationExtentCm;
  const FVector HalfExtents(Extent, Extent, Extent);
  return FBox(Loc - HalfExtents, Loc + HalfExtents);
}

int32 FStructuralConnectivityGraph::AllocSlot() {
  if (FreeSlots.Num() > 0) {
    const int32 Slot = FreeSlots.Pop(EAllowShrinking::No);
    if (Members.IsValidIndex(Slot)) {
      Members[Slot].Reset();
    }
    return Slot;
  }

  const int32 Slot = Nodes.AddDefaulted();
  Parent.Add(Slot);
  RankArr.Add(0);
  Members.AddDefaulted();
  return Slot;
}

int32 FStructuralConnectivityGraph::Find(int32 Index) {
  while (Parent[Index] != Index) {
    Parent[Index] = Parent[Parent[Index]];
    Index = Parent[Index];
  }
  return Index;
}

int32 FStructuralConnectivityGraph::FindRootIndexReadOnly(int32 Index) const {
  while (Parent[Index] != Index) {
    Index = Parent[Index];
  }
  return Index;
}

void FStructuralConnectivityGraph::Union(int32 A, int32 B) {
  A = Find(A);
  B = Find(B);
  if (A == B) {
    return;
  }

  InvalidateCanonicalCache();

  if (RankArr[A] < RankArr[B]) {
    Swap(A, B);
  }

  // O(|component B|) — keep member lists for CollectComponent without full scans.
  if (Members.IsValidIndex(A) && Members.IsValidIndex(B)) {
    Members[A].Append(Members[B]);
    Members[B].Reset();
  }

  Parent[B] = A;
  if (RankArr[A] == RankArr[B]) {
    ++RankArr[A];
  }
}

void FStructuralConnectivityGraph::IndexCells(int32 NodeIndex) {
  const FBox& Bounds = Nodes[NodeIndex].Bounds;
  if (!Bounds.IsValid) {
    return;
  }

  const FIntVector MinCell = ToCell(Bounds.Min);
  const FIntVector MaxCell = ToCell(Bounds.Max);
  for (int32 X = MinCell.X; X <= MaxCell.X; ++X) {
    for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y) {
      for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z) {
        Cells.FindOrAdd(FIntVector(X, Y, Z)).AddUnique(NodeIndex);
      }
    }
  }
}

void FStructuralConnectivityGraph::UnindexCells(int32 NodeIndex) {
  const FBox& Bounds = Nodes[NodeIndex].Bounds;
  if (!Bounds.IsValid) {
    return;
  }

  const FIntVector MinCell = ToCell(Bounds.Min);
  const FIntVector MaxCell = ToCell(Bounds.Max);
  for (int32 X = MinCell.X; X <= MaxCell.X; ++X) {
    for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y) {
      for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z) {
        const FIntVector Cell(X, Y, Z);
        if (TArray<int32>* Indices = Cells.Find(Cell)) {
          Indices->RemoveSingleSwap(NodeIndex, EAllowShrinking::No);
          if (Indices->Num() == 0) {
            Cells.Remove(Cell);
          }
        }
      }
    }
  }
}

void FStructuralConnectivityGraph::CollectNeighbors(int32 NodeIndex, TArray<int32>& Out) const {
  Out.Reset();
  const FNode& Self = Nodes[NodeIndex];
  if (!Self.bValid || !Self.Bounds.IsValid) {
    return;
  }

  const FBox Query = Self.Bounds.ExpandBy(NeighborQueryMarginCm);
  const FIntVector MinCell = ToCell(Query.Min);
  const FIntVector MaxCell = ToCell(Query.Max);
  TSet<int32> Seen;

  for (int32 X = MinCell.X; X <= MaxCell.X; ++X) {
    for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y) {
      for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z) {
        const TArray<int32>* Indices = Cells.Find(FIntVector(X, Y, Z));
        if (!Indices) {
          continue;
        }

        for (int32 Candidate : *Indices) {
          if (Candidate == NodeIndex || Seen.Contains(Candidate)) {
            continue;
          }
          Seen.Add(Candidate);

          const FNode& Other = Nodes[Candidate];
          if (!Other.bValid) {
            continue;
          }

          if (FStructuralLightweightIndex::AreBoundsStructurallyConnected(
                  Self.Bounds, Other.Bounds, Self.Class, Other.Class)) {
            Out.Add(Candidate);
          }
        }
      }
    }
  }
}

void FStructuralConnectivityGraph::CollectComponent(int32 StartIndex, TArray<int32>& Out) const {
  Out.Reset();
  if (!Nodes.IsValidIndex(StartIndex) || !Nodes[StartIndex].bValid) {
    return;
  }

  const int32 Root = FindRootIndexReadOnly(StartIndex);
  if (Members.IsValidIndex(Root) && Members[Root].Num() > 0) {
    Out.Reserve(Members[Root].Num());
    for (int32 Index : Members[Root]) {
      if (Nodes.IsValidIndex(Index) && Nodes[Index].bValid &&
          FindRootIndexReadOnly(Index) == Root) {
        Out.Add(Index);
      }
    }
    if (Out.Num() > 0) {
      return;
    }
  }

  // Fallback if member lists drifted — O(N) scan.
  Out.Reserve(NumValid);
  for (int32 Index = 0; Index < Nodes.Num(); ++Index) {
    if (!Nodes[Index].bValid) {
      continue;
    }
    if (FindRootIndexReadOnly(Index) == Root) {
      Out.Add(Index);
    }
  }
}

int32 FStructuralConnectivityGraph::AddNodeInternal(const FStructuralNodeId& Id, const FBox& Bounds,
                                                    TSubclassOf<AFGBuildable> Class,
                                                    TArray<int32>& OutMergedRoots) {
  OutMergedRoots.Reset();
  if (!Id.IsValid() || !Bounds.IsValid || IdToIndex.Contains(Id)) {
    return INDEX_NONE;
  }

  const int32 Slot = AllocSlot();
  FNode& Node = Nodes[Slot];
  Node.Id = Id;
  Node.Bounds = Bounds;
  Node.Class = Class;
  Node.bValid = true;
  Parent[Slot] = Slot;
  RankArr[Slot] = 0;
  if (Members.IsValidIndex(Slot)) {
    Members[Slot].Reset();
    Members[Slot].Add(Slot);
  }
  IdToIndex.Add(Id, Slot);
  IndexCells(Slot);
  ++NumValid;

  TArray<int32> Neighbors;
  CollectNeighbors(Slot, Neighbors);

  TSet<int32> PriorRoots;
  for (int32 Neighbor : Neighbors) {
    PriorRoots.Add(Find(Neighbor));
  }
  for (int32 Neighbor : Neighbors) {
    Union(Slot, Neighbor);
  }

  OutMergedRoots.Reserve(PriorRoots.Num());
  for (int32 Root : PriorRoots) {
    OutMergedRoots.Add(Root);
  }
  return Slot;
}

bool FStructuralConnectivityGraph::AddActorNode(AFGBuildable* Buildable,
                                                TArray<int32>& OutMergedRoots) {
  if (!IsValid(Buildable) || !FStructuralEligibilityRules::IsBusMember(Buildable)) {
    OutMergedRoots.Reset();
    return false;
  }

  const FBox Bounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Buildable);
  return AddNodeInternal(MakeActorNodeId(Buildable), Bounds, Buildable->GetClass(),
                         OutMergedRoots) != INDEX_NONE;
}

bool FStructuralConnectivityGraph::AddLightweightNode(UWorld* World,
                                                      const FStructuralLightweightKey& Key,
                                                      TArray<int32>& OutMergedRoots) {
  if (!Key.IsValid()) {
    OutMergedRoots.Reset();
    return false;
  }

  TSubclassOf<AFGBuildable> Class = nullptr;
  const FBox Bounds = ComputeLightweightBounds(World, Key, Class);
  return AddNodeInternal(FStructuralLightweightIndex::MakeNodeId(Key), Bounds, Class,
                         OutMergedRoots) != INDEX_NONE;
}

void FStructuralConnectivityGraph::InvalidateSlot(int32 Index) {
  if (!Nodes.IsValidIndex(Index) || !Nodes[Index].bValid) {
    return;
  }

  const FStructuralNodeId Id = Nodes[Index].Id;
  UnindexCells(Index);
  Nodes[Index].bValid = false;
  Nodes[Index].Id = FStructuralNodeId();
  Nodes[Index].Class = nullptr;
  Nodes[Index].Bounds = FBox(ForceInit);
  Parent[Index] = Index;
  RankArr[Index] = 0;
  if (Members.IsValidIndex(Index)) {
    Members[Index].Reset();
  }
  IdToIndex.Remove(Id);
  FreeSlots.Add(Index);
  --NumValid;
}

void FStructuralConnectivityGraph::ReuniteSurvivors(const TArray<int32>& Survivors,
                                                    const int32 OldRootHint,
                                                    TArray<int32>& OutAffectedRoots) {
  for (int32 NodeIndex : Survivors) {
    if (!Nodes.IsValidIndex(NodeIndex) || !Nodes[NodeIndex].bValid) {
      continue;
    }
    Parent[NodeIndex] = NodeIndex;
    RankArr[NodeIndex] = 0;
    if (Members.IsValidIndex(NodeIndex)) {
      Members[NodeIndex].Reset();
      Members[NodeIndex].Add(NodeIndex);
    }
  }

  TArray<int32> Neighbors;
  for (int32 NodeIndex : Survivors) {
    if (!Nodes.IsValidIndex(NodeIndex) || !Nodes[NodeIndex].bValid) {
      continue;
    }
    CollectNeighbors(NodeIndex, Neighbors);
    for (int32 Neighbor : Neighbors) {
      if (Nodes[Neighbor].bValid) {
        Union(NodeIndex, Neighbor);
      }
    }
  }

  TSet<int32> Roots;
  if (OldRootHint != INDEX_NONE) {
    Roots.Add(OldRootHint);
  }
  for (int32 NodeIndex : Survivors) {
    if (Nodes.IsValidIndex(NodeIndex) && Nodes[NodeIndex].bValid) {
      Roots.Add(Find(NodeIndex));
    }
  }
  OutAffectedRoots.Reserve(OutAffectedRoots.Num() + Roots.Num());
  for (int32 Root : Roots) {
    OutAffectedRoots.AddUnique(Root);
  }
}

void FStructuralConnectivityGraph::RemoveNode(const FStructuralNodeId& Id,
                                              TArray<int32>& OutAffectedRoots) {
  TArray<FStructuralNodeId> Ids;
  Ids.Add(Id);
  RemoveNodesBatched(Ids, OutAffectedRoots);
}

void FStructuralConnectivityGraph::RemoveNodesBatched(const TArray<FStructuralNodeId>& Ids,
                                                      TArray<int32>& OutAffectedRoots) {
  OutAffectedRoots.Reset();
  if (Ids.Num() == 0) {
    return;
  }

  TSet<int32> RemoveSlots;
  TSet<int32> SeedRoots;
  for (const FStructuralNodeId& Id : Ids) {
    const int32* IndexPtr = IdToIndex.Find(Id);
    if (!IndexPtr) {
      continue;
    }
    const int32 Index = *IndexPtr;
    if (!Nodes.IsValidIndex(Index) || !Nodes[Index].bValid) {
      continue;
    }
    RemoveSlots.Add(Index);
    SeedRoots.Add(Find(Index));
  }

  if (RemoveSlots.Num() == 0) {
    return;
  }

  // One component gather per touched root — not per deleted pad.
  TSet<int32> ComponentSlots;
  for (int32 Root : SeedRoots) {
    TArray<int32> Part;
    CollectComponent(Root, Part);
    for (int32 Slot : Part) {
      ComponentSlots.Add(Slot);
    }
  }

  InvalidateCanonicalCache();

  for (int32 Index : RemoveSlots) {
    InvalidateSlot(Index);
  }

  for (int32 NodeIndex : ComponentSlots) {
    if (Members.IsValidIndex(NodeIndex)) {
      Members[NodeIndex].Reset();
    }
  }

  TArray<int32> Survivors;
  Survivors.Reserve(ComponentSlots.Num());
  for (int32 NodeIndex : ComponentSlots) {
    if (!RemoveSlots.Contains(NodeIndex) && Nodes.IsValidIndex(NodeIndex) &&
        Nodes[NodeIndex].bValid) {
      Survivors.Add(NodeIndex);
    }
  }

  const int32 OldRootHint = SeedRoots.Num() > 0 ? *SeedRoots.CreateConstIterator() : INDEX_NONE;
  ReuniteSurvivors(Survivors, OldRootHint, OutAffectedRoots);
  for (int32 Root : SeedRoots) {
    OutAffectedRoots.AddUnique(Root);
  }
}

int32 FStructuralConnectivityGraph::FindRoot(const FStructuralNodeId& Id) const {
  const int32* IndexPtr = IdToIndex.Find(Id);
  if (!IndexPtr) {
    return INDEX_NONE;
  }
  return FindRootIndexReadOnly(*IndexPtr);
}

int32 FStructuralConnectivityGraph::FindRootForBounds(const FBox& Bounds,
                                                      TSubclassOf<AFGBuildable> Class,
                                                      FStructuralNodeId* OutBestNodeId) const {
  if (OutBestNodeId) {
    *OutBestNodeId = FStructuralNodeId();
  }

  if (!Bounds.IsValid) {
    return INDEX_NONE;
  }

  const FBox Query = Bounds.ExpandBy(NeighborQueryMarginCm);
  const FIntVector MinCell = ToCell(Query.Min);
  const FIntVector MaxCell = ToCell(Query.Max);
  TSet<int32> Seen;

  int32 BestIndex = INDEX_NONE;
  double BestScore = -TNumericLimits<double>::Max();

  for (int32 X = MinCell.X; X <= MaxCell.X; ++X) {
    for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y) {
      for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z) {
        const TArray<int32>* Indices = Cells.Find(FIntVector(X, Y, Z));
        if (!Indices) {
          continue;
        }

        for (int32 Candidate : *Indices) {
          if (Seen.Contains(Candidate)) {
            continue;
          }
          Seen.Add(Candidate);

          const FNode& Other = Nodes[Candidate];
          if (!Other.bValid) {
            continue;
          }

          if (!FStructuralLightweightIndex::AreBoundsStructurallyConnected(Bounds, Other.Bounds,
                                                                           Class, Other.Class)) {
            continue;
          }

          const FBox Overlap = Bounds.Overlap(Other.Bounds);
          const double Score = Overlap.IsValid
                                   ? static_cast<double>(Overlap.GetVolume())
                                   : -FMath::Sqrt(static_cast<double>(
                                         Bounds.ComputeSquaredDistanceToBox(Other.Bounds)));
          if (Score > BestScore) {
            BestScore = Score;
            BestIndex = Candidate;
          }
        }
      }
    }
  }

  if (BestIndex == INDEX_NONE) {
    return INDEX_NONE;
  }

  if (OutBestNodeId) {
    *OutBestNodeId = Nodes[BestIndex].Id;
  }
  return FindRootIndexReadOnly(BestIndex);
}

FStructuralNodeId FStructuralConnectivityGraph::MakeCanonicalNodeIdForComponent(int32 Root) const {
  if (Root == INDEX_NONE) {
    return FStructuralNodeId();
  }

  if (const FStructuralNodeId* Cached = CanonicalByRoot.Find(Root)) {
    return *Cached;
  }

  FStructuralNodeId Best;
  for (int32 Index = 0; Index < Nodes.Num(); ++Index) {
    if (!Nodes[Index].bValid) {
      continue;
    }

    if (FindRootIndexReadOnly(Index) != Root) {
      continue;
    }

    const FStructuralNodeId& Candidate = Nodes[Index].Id;
    if (!Best.IsValid() || CompareNodeId(Candidate, Best) < 0) {
      Best = Candidate;
    }
  }

  if (Best.IsValid()) {
    CanonicalByRoot.Add(Root, Best);
  }

  return Best;
}

bool FStructuralConnectivityGraph::FindNearestStructureAnchor(const FVector& QueryLoc,
                                                              float MaxHorizontal,
                                                              float MaxVertical, FVector& OutAnchor,
                                                              int32& OutComponentRoot) const {
  if (MaxHorizontal <= 0.0f || MaxVertical <= 0.0f || NumValid <= 0) {
    return false;
  }

  float BestScoreSq = FLT_MAX;
  bool bFound = false;
  OutComponentRoot = INDEX_NONE;

  for (const TPair<FStructuralNodeId, int32>& Pair : IdToIndex) {
    const int32 Index = Pair.Value;
    if (!Nodes.IsValidIndex(Index) || !Nodes[Index].bValid) {
      continue;
    }

    const FBox& Bounds = Nodes[Index].Bounds;
    if (!Bounds.IsValid) {
      continue;
    }

    FVector Closest;
    Closest.X = FMath::Clamp(QueryLoc.X, Bounds.Min.X, Bounds.Max.X);
    Closest.Y = FMath::Clamp(QueryLoc.Y, Bounds.Min.Y, Bounds.Max.Y);
    Closest.Z = FMath::Clamp(QueryLoc.Z, Bounds.Min.Z, Bounds.Max.Z);

    const float Horizontal =
        FVector2D::Distance(FVector2D(QueryLoc.X, QueryLoc.Y), FVector2D(Closest.X, Closest.Y));
    const float Vertical = FMath::Abs(QueryLoc.Z - Closest.Z);
    if (Horizontal > MaxHorizontal || Vertical > MaxVertical) {
      continue;
    }

    const float HorizontalSq = FMath::Square(Horizontal);
    const float VerticalSq = FMath::Square(Vertical);
    const float ScoreSq = FMath::Max(HorizontalSq, VerticalSq);
    if (ScoreSq >= BestScoreSq) {
      continue;
    }

    const int32 Root = FindRoot(Pair.Key);
    if (Root == INDEX_NONE) {
      continue;
    }

    BestScoreSq = ScoreSq;
    OutAnchor = Closest;
    OutComponentRoot = Root;
    bFound = true;
  }

  return bFound;
}

void FStructuralConnectivityGraph::GetComponentStats(int32& OutComponents, int32& OutLargest) {
  OutComponents = 0;
  OutLargest = 0;

  TMap<int32, int32> Counts;
  for (int32 Index = 0; Index < Nodes.Num(); ++Index) {
    if (!Nodes[Index].bValid) {
      continue;
    }
    const int32 Root = Find(Index);
    const int32 Count = Counts.FindOrAdd(Root) + 1;
    Counts[Root] = Count;
    OutLargest = FMath::Max(OutLargest, Count);
  }
  OutComponents = Counts.Num();
}

void FStructuralConnectivityGraph::Rebuild(UWorld* World) {
  Reset();
  if (!IsValid(World)) {
    return;
  }

  TArray<int32> Ignored;

  if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World)) {
    for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef()) {
      if (IsValid(Buildable) && FStructuralEligibilityRules::IsBusMember(Buildable)) {
        AddActorNode(Buildable, Ignored);
      }
    }
  }

  if (AFGLightweightBuildableSubsystem* Lw = AFGLightweightBuildableSubsystem::Get(World)) {
    for (const TPair<TSubclassOf<AFGBuildable>, TArray<FRuntimeBuildableInstanceData>>& Pair :
         Lw->GetAllLightweightBuildableInstances()) {
      TSubclassOf<AFGBuildable> Class = Pair.Key;
      if (!Class) {
        continue;
      }

      const AFGBuildable* CDO = Class->GetDefaultObject<AFGBuildable>();
      if (!FStructuralEligibilityRules::IsBusMember(CDO)) {
        continue;
      }

      const TArray<FRuntimeBuildableInstanceData>& Instances = Pair.Value;
      for (int32 Index = 0; Index < Instances.Num(); ++Index) {
        if (!Instances[Index].IsValidOnLoad()) {
          continue;
        }
        AddLightweightNode(World, FStructuralLightweightKey{Class, Index}, Ignored);
      }
    }
  }
}
