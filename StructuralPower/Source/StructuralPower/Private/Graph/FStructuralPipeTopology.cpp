// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralPipeTopology.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePipeReservoir.h"
#include "Core/FStructuralGraphSession.h"
#include "EngineUtils.h"
#include "FGPipeConnectionComponent.h"
#include "Graph/FStructuralBridgeRootIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerLog.h"

void FStructuralPipeTopology::Reset() {
  Parent.Reset();
  Rank.Reset();
  Actors.Reset();
  InjectStructureRootByPipeRoot.Reset();
}

void FStructuralPipeTopology::RemoveBuildable(AFGBuildable* Buildable,
                                              FStructuralGraphSession& Session) {
  if (!IsValid(Buildable)) {
    return;
  }

  const FStructuralNodeId Id = AStructuralPowerGraphSubsystem::MakeNodeId(Buildable);
  if (!Parent.Contains(Id)) {
    return;
  }

  const FStructuralNodeId OldRoot = FindRootMutable(Id);
  TArray<FStructuralNodeId> Survivors;
  CollectComponentMembers(OldRoot, Survivors);
  Survivors.Remove(Id);

  InjectStructureRootByPipeRoot.Remove(OldRoot);
  Parent.Remove(Id);
  Rank.Remove(Id);
  Actors.Remove(Id);

  for (const FStructuralNodeId& SurvivorId : Survivors) {
    if (!Parent.Contains(SurvivorId)) {
      continue;
    }
    Parent.Add(SurvivorId, SurvivorId);
    Rank.Add(SurvivorId, 0);
  }

  TArray<AFGBuildable*> Neighbors;
  for (const FStructuralNodeId& SurvivorId : Survivors) {
    const TWeakObjectPtr<AFGBuildable>* Weak = Actors.Find(SurvivorId);
    AFGBuildable* Conductor = Weak ? Weak->Get() : nullptr;
    if (!IsValid(Conductor)) {
      continue;
    }

    CollectConnectedConductors(Conductor, Neighbors);
    for (AFGBuildable* Neighbor : Neighbors) {
      if (!IsValid(Neighbor)) {
        continue;
      }
      const FStructuralNodeId NeighborId = AStructuralPowerGraphSubsystem::MakeNodeId(Neighbor);
      if (!Parent.Contains(NeighborId)) {
        continue;
      }
      Union(SurvivorId, NeighborId);
    }
  }

  TSet<FStructuralNodeId> NewRoots;
  for (const FStructuralNodeId& SurvivorId : Survivors) {
    if (!Parent.Contains(SurvivorId)) {
      continue;
    }
    const FStructuralNodeId Root = FindRootMutable(SurvivorId);
    if (Root.IsValid()) {
      NewRoots.Add(Root);
    }
  }

  for (const FStructuralNodeId& Root : NewRoots) {
    const int32 InjectRoot = DiscoverInjectOnComponent(Root, Session);
    if (InjectRoot != INDEX_NONE) {
      InjectStructureRootByPipeRoot.Add(Root, InjectRoot);
    }
  }
}

FStructuralNodeId FStructuralPipeTopology::FindRoot(const FStructuralNodeId& Id) const {
  if (!Id.IsValid() || !Parent.Contains(Id)) {
    return FStructuralNodeId();
  }

  FStructuralNodeId Cur = Id;
  for (int32 Hop = 0; Hop < MaxConductorHops; ++Hop) {
    const FStructuralNodeId* P = Parent.Find(Cur);
    if (!P || *P == Cur) {
      return Cur;
    }
    Cur = *P;
  }
  return Cur;
}

FStructuralNodeId FStructuralPipeTopology::FindRootMutable(const FStructuralNodeId& Id) {
  if (!Id.IsValid() || !Parent.Contains(Id)) {
    return FStructuralNodeId();
  }

  FStructuralNodeId Cur = Id;
  TArray<FStructuralNodeId, TInlineAllocator<64>> Path;
  for (int32 Hop = 0; Hop < MaxConductorHops; ++Hop) {
    const FStructuralNodeId* P = Parent.Find(Cur);
    if (!P || *P == Cur) {
      for (const FStructuralNodeId& N : Path) {
        Parent.Add(N, Cur);
      }
      return Cur;
    }
    Path.Add(Cur);
    Cur = *P;
  }
  return Cur;
}

void FStructuralPipeTopology::Union(const FStructuralNodeId& A, const FStructuralNodeId& B) {
  FStructuralNodeId Ra = FindRootMutable(A);
  FStructuralNodeId Rb = FindRootMutable(B);
  if (!Ra.IsValid() || !Rb.IsValid() || Ra == Rb) {
    return;
  }

  const int32 RankA = Rank.FindRef(Ra);
  const int32 RankB = Rank.FindRef(Rb);
  FStructuralNodeId NewRoot = Ra;
  FStructuralNodeId OldRoot = Rb;
  if (RankA < RankB) {
    NewRoot = Rb;
    OldRoot = Ra;
  } else if (RankA == RankB) {
    Rank.Add(Ra, RankA + 1);
  }

  Parent.Add(OldRoot, NewRoot);

  if (const int32* InjOld = InjectStructureRootByPipeRoot.Find(OldRoot)) {
    if (!InjectStructureRootByPipeRoot.Contains(NewRoot)) {
      InjectStructureRootByPipeRoot.Add(NewRoot, *InjOld);
    }
    InjectStructureRootByPipeRoot.Remove(OldRoot);
  }
}

void FStructuralPipeTopology::CollectConnectedConductors(AFGBuildable* Self,
                                                         TArray<AFGBuildable*>& Out) const {
  Out.Reset();
  if (!IsValid(Self)) {
    return;
  }

  TInlineComponentArray<UFGPipeConnectionComponent*> Conns(Self);
  if (AFGBuildablePipelineAttachment* Attachment = Cast<AFGBuildablePipelineAttachment>(Self)) {
    const TArray<UFGPipeConnectionComponent*> AttachmentConns = Attachment->GetPipeConnections();
    for (UFGPipeConnectionComponent* Conn : AttachmentConns) {
      if (IsValid(Conn)) {
        Conns.AddUnique(Conn);
      }
    }
  } else if (AFGBuildablePipeReservoir* Reservoir = Cast<AFGBuildablePipeReservoir>(Self)) {
    const TArray<UFGPipeConnectionComponent*> ReservoirConns = Reservoir->GetPipeConnections();
    for (UFGPipeConnectionComponent* Conn : ReservoirConns) {
      if (IsValid(Conn)) {
        Conns.AddUnique(Conn);
      }
    }
  } else if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(Self)) {
    if (UFGPipeConnectionComponent* C0 = Pipeline->GetPipeConnection0()) {
      Conns.AddUnique(C0);
    }
    if (UFGPipeConnectionComponent* C1 = Pipeline->GetPipeConnection1()) {
      Conns.AddUnique(C1);
    }
  }

  const int32 Limit = FMath::Min(Conns.Num(), MaxNeighborPorts);
  for (int32 i = 0; i < Limit; ++i) {
    UFGPipeConnectionComponent* Conn = Conns[i];
    if (!IsValid(Conn)) {
      continue;
    }

    UFGPipeConnectionComponent* Other = Conn->GetPipeConnection();
    if (!IsValid(Other)) {
      UFGPipeConnectionComponentBase* BaseOther = Conn->GetConnection();
      Other = Cast<UFGPipeConnectionComponent>(BaseOther);
    }
    if (!IsValid(Other)) {
      continue;
    }

    AFGBuildable* Owner = Cast<AFGBuildable>(Other->GetOwner());
    if (FStructuralEligibilityRules::IsFluidPipeConductor(Owner)) {
      Out.AddUnique(Owner);
    }
  }
}

AFGBuildable* FStructuralPipeTopology::FindTouchedPipe(AFGBuildable* Support) const {
  if (!FStructuralEligibilityRules::IsFluidPipeSupport(Support)) {
    return nullptr;
  }

  TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Support);
  for (UFGPipeConnectionComponentBase* Conn : Conns) {
    if (!IsValid(Conn)) {
      continue;
    }
    UFGPipeConnectionComponentBase* Other = Conn->GetConnection();
    if (!IsValid(Other)) {
      continue;
    }
    if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Other->GetOwner())) {
      return Pipe;
    }
  }

  UWorld* World = Support->GetWorld();
  if (!IsValid(World)) {
    return nullptr;
  }

  FVector SnapLoc = Support->GetActorLocation();
  if (Conns.Num() > 0 && IsValid(Conns[0])) {
    SnapLoc = Conns[0]->GetConnectorLocation(/*withClearance=*/false);
  }

  AFGBuildablePipeline* Best = nullptr;
  float BestDistSq = SupportTouchRadiusUu * SupportTouchRadiusUu;
  for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It) {
    AFGBuildablePipeline* Pipe = *It;
    if (!IsValid(Pipe) || !FStructuralEligibilityRules::IsFluidPipeConductor(Pipe)) {
      continue;
    }
    const float Offset = Pipe->FindOffsetClosestToLocation(SnapLoc);
    FVector OnSpline = FVector::ZeroVector;
    FVector Dir = FVector::ZeroVector;
    Pipe->GetLocationAndDirectionAtOffset(Offset, OnSpline, Dir);
    const float DistSq = FVector::DistSquared(SnapLoc, OnSpline);
    if (DistSq <= BestDistSq) {
      BestDistSq = DistSq;
      Best = Pipe;
    }
  }
  return Best;
}

void FStructuralPipeTopology::CollectComponentMembers(const FStructuralNodeId& PipeRoot,
                                                      TArray<FStructuralNodeId>& Out) const {
  Out.Reset();
  if (!PipeRoot.IsValid()) {
    return;
  }

  for (const TPair<FStructuralNodeId, FStructuralNodeId>& Pair : Parent) {
    if (FindRoot(Pair.Key) == PipeRoot) {
      Out.Add(Pair.Key);
      if (Out.Num() >= MaxComponentMembers) {
        break;
      }
    }
  }
}

int32 FStructuralPipeTopology::DiscoverInjectOnComponent(
    const FStructuralNodeId& PipeRoot, FStructuralGraphSession& Session) const {
  if (const int32* Cached = InjectStructureRootByPipeRoot.Find(PipeRoot)) {
    if (*Cached != INDEX_NONE) {
      return *Cached;
    }
  }

  TArray<FStructuralNodeId> Members;
  CollectComponentMembers(PipeRoot, Members);

  for (const FStructuralNodeId& MemberId : Members) {
    const TWeakObjectPtr<AFGBuildable>* Weak = Actors.Find(MemberId);
    AFGBuildable* Conductor = Weak ? Weak->Get() : nullptr;
    if (!IsValid(Conductor)) {
      continue;
    }

    TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Conductor);
    const int32 Limit = FMath::Min(Conns.Num(), MaxNeighborPorts);
    for (int32 i = 0; i < Limit; ++i) {
      UFGPipeConnectionComponentBase* Conn = Conns[i];
      if (!IsValid(Conn)) {
        continue;
      }
      UFGPipeConnectionComponentBase* Other = Conn->GetConnection();
      if (!IsValid(Other)) {
        continue;
      }
      AFGBuildable* Owner = Cast<AFGBuildable>(Other->GetOwner());
      if (!FStructuralEligibilityRules::IsFluidPipeSupport(Owner)) {
        continue;
      }

      const FStructuralWallAnchor Anchor = Session.BridgeRootIndex().ResolveOutletAnchor(Owner);
      FStructuralNodeId ParentId;
      const int32 StructureRoot =
          Session.BridgeRootIndex().ResolveEndpointComponentRoot(Owner, Anchor, ParentId);
      if (StructureRoot != INDEX_NONE) {
        return StructureRoot;
      }
    }
  }

  return INDEX_NONE;
}

bool FStructuralPipeTopology::IntegrateConductor(AFGBuildable* Buildable,
                                                 FStructuralGraphSession& Session) {
  if (!FStructuralEligibilityRules::IsFluidPipeConductor(Buildable)) {
    return false;
  }

  const FStructuralNodeId SelfId = Session.MakeNodeId(Buildable);
  if (!SelfId.IsValid()) {
    return false;
  }

  if (!Parent.Contains(SelfId)) {
    Parent.Add(SelfId, SelfId);
    Rank.Add(SelfId, 0);
  }
  Actors.Add(SelfId, Buildable);

  TArray<AFGBuildable*, TInlineAllocator<64>> Queue;
  TSet<FStructuralNodeId> Visited;
  Queue.Add(Buildable);
  Visited.Add(SelfId);

  for (int32 Qi = 0; Qi < Queue.Num() && Qi < MaxConductorHops; ++Qi) {
    AFGBuildable* Cur = Queue[Qi];
    if (!IsValid(Cur)) {
      continue;
    }

    const FStructuralNodeId CurId = Session.MakeNodeId(Cur);
    if (!Parent.Contains(CurId)) {
      Parent.Add(CurId, CurId);
      Rank.Add(CurId, 0);
    }
    Actors.Add(CurId, Cur);

    TArray<AFGBuildable*> Neighbors;
    CollectConnectedConductors(Cur, Neighbors);
    for (AFGBuildable* N : Neighbors) {
      if (!IsValid(N)) {
        continue;
      }
      const FStructuralNodeId NId = Session.MakeNodeId(N);
      if (!NId.IsValid() || Visited.Contains(NId)) {
        continue;
      }
      Visited.Add(NId);
      if (!Parent.Contains(NId)) {
        Parent.Add(NId, NId);
        Rank.Add(NId, 0);
      }
      Actors.Add(NId, N);
      Union(CurId, NId);
      Queue.Add(N);
    }
  }

  return true;
}

void FStructuralPipeTopology::RegisterInject(const FStructuralNodeId& ConductorId,
                                             int32 StructureRoot) {
  if (!ConductorId.IsValid() || StructureRoot == INDEX_NONE) {
    return;
  }

  const FStructuralNodeId PipeRoot = FindRootMutable(ConductorId);
  if (!PipeRoot.IsValid()) {
    return;
  }

  if (!InjectStructureRootByPipeRoot.Contains(PipeRoot)) {
    InjectStructureRootByPipeRoot.Add(PipeRoot, StructureRoot);
  }
}

void FStructuralPipeTopology::RegisterMachineInject(AFGBuildable* Machine, int32 StructureRoot,
                                                    FStructuralGraphSession& Session) {
  if (!IsValid(Machine) || StructureRoot == INDEX_NONE) {
    return;
  }

  TInlineComponentArray<UFGPipeConnectionComponent*> Conns(Machine);
  const int32 Limit = FMath::Min(Conns.Num(), MaxNeighborPorts);
  TSet<FStructuralNodeId> TouchedRoots;

  for (int32 i = 0; i < Limit; ++i) {
    UFGPipeConnectionComponent* Conn = Conns[i];
    if (!IsValid(Conn)) {
      continue;
    }

    UFGPipeConnectionComponent* Other = Conn->GetPipeConnection();
    if (!IsValid(Other)) {
      continue;
    }

    AFGBuildable* Owner = Cast<AFGBuildable>(Other->GetOwner());
    if (!FStructuralEligibilityRules::IsFluidPipeConductor(Owner)) {
      continue;
    }

    IntegrateConductor(Owner, Session);
    const FStructuralNodeId OwnerId = Session.MakeNodeId(Owner);
    RegisterInject(OwnerId, StructureRoot);
    const FStructuralNodeId PipeRoot = FindRootMutable(OwnerId);
    if (PipeRoot.IsValid()) {
      TouchedRoots.Add(PipeRoot);
    }
  }

  for (const FStructuralNodeId& PipeRoot : TouchedRoots) {
    EnqueuePumpsOnRoot(Session, PipeRoot);
  }
}

void FStructuralPipeTopology::ProcessSupport(AFGBuildable* Support,
                                             FStructuralGraphSession& Session) {
  if (!FStructuralEligibilityRules::IsFluidPipeSupport(Support)) {
    return;
  }

  const FStructuralWallAnchor Anchor = Session.BridgeRootIndex().ResolveOutletAnchor(Support);
  FStructuralNodeId ParentId;
  const int32 StructureRoot =
      Session.BridgeRootIndex().ResolveEndpointComponentRoot(Support, Anchor, ParentId);
  if (StructureRoot == INDEX_NONE) {
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] pipe support no structure site %s"),
           *Support->GetName());
    return;
  }

  AFGBuildable* Pipe = FindTouchedPipe(Support);
  if (!IsValid(Pipe)) {
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] pipe support no touched pipe %s"),
           *Support->GetName());
    return;
  }

  IntegrateConductor(Pipe, Session);
  const FStructuralNodeId PipeId = Session.MakeNodeId(Pipe);
  RegisterInject(PipeId, StructureRoot);
  const FStructuralNodeId PipeRoot = FindRootMutable(PipeId);
  EnqueuePumpsOnRoot(Session, PipeRoot);

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] pipe support inject %s → pipe=%s structureRoot=%d"), *Support->GetName(),
         *Pipe->GetName(), StructureRoot);
}

int32 FStructuralPipeTopology::ResolveInjectedStructureRoot(AFGBuildable* PipeActor,
                                                            FStructuralGraphSession& Session) {
  if (!IsValid(PipeActor)) {
    return INDEX_NONE;
  }

  IntegrateConductor(PipeActor, Session);
  const FStructuralNodeId Id = Session.MakeNodeId(PipeActor);
  const FStructuralNodeId PipeRoot = FindRootMutable(Id);
  if (!PipeRoot.IsValid()) {
    return INDEX_NONE;
  }

  if (const int32* Cached = InjectStructureRootByPipeRoot.Find(PipeRoot)) {
    if (*Cached != INDEX_NONE) {
      return *Cached;
    }
  }

  const int32 Discovered = DiscoverInjectOnComponent(PipeRoot, Session);
  if (Discovered != INDEX_NONE) {
    InjectStructureRootByPipeRoot.Add(PipeRoot, Discovered);
  }
  return Discovered;
}

void FStructuralPipeTopology::EnqueuePumpsOnRoot(FStructuralGraphSession& Session,
                                                 const FStructuralNodeId& PipeRoot) {
  if (!PipeRoot.IsValid()) {
    return;
  }

  TArray<FStructuralNodeId> Members;
  CollectComponentMembers(PipeRoot, Members);
  int32 Enqueued = 0;
  for (const FStructuralNodeId& MemberId : Members) {
    if (Enqueued >= MaxEnqueuePumps) {
      break;
    }
    const TWeakObjectPtr<AFGBuildable>* Weak = Actors.Find(MemberId);
    AFGBuildable* Actor = Weak ? Weak->Get() : nullptr;
    if (!FStructuralEligibilityRules::IsStructuralPipelinePump(Actor)) {
      continue;
    }
    if (Session.IsBuildablePlacementPending(Actor)) {
      continue;
    }
    Session.EnqueuePlacement(Actor, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
    ++Enqueued;
  }
}
