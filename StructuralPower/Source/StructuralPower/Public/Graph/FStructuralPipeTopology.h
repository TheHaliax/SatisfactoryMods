// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"

class AFGBuildable;
class FStructuralGraphSession;

/**
 * Fluid-pipe connection membership + inject → structure site root.
 * Not a power mesh — pumps attach via existing site OutletBus / consumer path.
 */
class STRUCTURALPOWER_API FStructuralPipeTopology {
 public:
  static constexpr int32 MaxConductorHops = 4096;
  static constexpr int32 MaxComponentMembers = 16384;
  static constexpr int32 MaxNeighborPorts = 32;
  static constexpr int32 MaxEnqueuePumps = 256;
  static constexpr float SupportTouchRadiusUu = 50.f;

  void Reset();

  void RemoveBuildable(AFGBuildable* Buildable, FStructuralGraphSession& Session);

  /** Add conductor (pipeline / attachment) and union connection neighbors (bounded BFS). */
  bool IntegrateConductor(AFGBuildable* Buildable, FStructuralGraphSession& Session);

  /** Fluid support: touch pipe + structure site → inject map; enqueue pumps. */
  void ProcessSupport(AFGBuildable* Support, FStructuralGraphSession& Session);

  /** Machine with active SP transfer: inject structure root into connected pipe run. */
  void RegisterMachineInject(AFGBuildable* Machine, int32 StructureRoot,
                             FStructuralGraphSession& Session);

  void RegisterInject(const FStructuralNodeId& ConductorId, int32 StructureRoot);

  FStructuralNodeId FindRoot(const FStructuralNodeId& Id) const;

  /** Structure site root powering this pipe actor, or INDEX_NONE. */
  int32 ResolveInjectedStructureRoot(AFGBuildable* PipeActor, FStructuralGraphSession& Session);

  void EnqueuePumpsOnRoot(FStructuralGraphSession& Session, const FStructuralNodeId& PipeRoot);

  bool IsTracked(const FStructuralNodeId& Id) const {
    return Parent.Contains(Id);
  }

  int32 GetNodeCount() const {
    return Parent.Num();
  }

 private:
  FStructuralNodeId FindRootMutable(const FStructuralNodeId& Id);
  void Union(const FStructuralNodeId& A, const FStructuralNodeId& B);
  void CollectConnectedConductors(AFGBuildable* Self, TArray<AFGBuildable*>& Out) const;
  AFGBuildable* FindTouchedPipe(AFGBuildable* Support) const;
  int32 DiscoverInjectOnComponent(const FStructuralNodeId& PipeRoot,
                                  FStructuralGraphSession& Session) const;
  void CollectComponentMembers(const FStructuralNodeId& PipeRoot,
                               TArray<FStructuralNodeId>& Out) const;

  TMap<FStructuralNodeId, FStructuralNodeId> Parent;
  TMap<FStructuralNodeId, int32> Rank;
  TMap<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>> Actors;
  /** Pipe UF root NodeId → structure StructureGraph root index. */
  TMap<FStructuralNodeId, int32> InjectStructureRootByPipeRoot;
};
