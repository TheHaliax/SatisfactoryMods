// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Content/FPipeSupportTouch.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Components/ActorComponent.h"
#include "EngineUtils.h"
#include "FGPipeConnectionComponent.h"
#include "PipelineColorLog.h"
#include "UObject/SoftObjectPath.h"

namespace FPipeSupportTouch {
namespace {
constexpr float TouchRadiusUu = 50.f;

TMap<TWeakObjectPtr<AFGBuildablePipeline>, TArray<TWeakObjectPtr<AFGBuildable>>> GPipeToSupports;
TMap<TWeakObjectPtr<AFGBuildable>, TWeakObjectPtr<AFGBuildablePipeline>> GSupportToPipe;

// Pipes scanned world-wide with zero supports found. Membership means "do not
// rescan"; entries drop when a support links (RememberLink) or the pipe dies.
TSet<TWeakObjectPtr<AFGBuildablePipeline>> GScannedNoSupports;

// Full-map prune is O(cache); amortize instead of running per mutation.
int32 GOpsSincePrune = 0;
constexpr int32 PruneOpInterval = 256;

// Path literals only as the parse source. Resolved classes held as weak ptrs:
// GC-safe (weak nulls on menu -> reload, never dangles) and kills the previous
// per-actor FSoftClassPath parse + FindObject storm.
const TCHAR* kFluidSupportParentPaths[] = {
    TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupport/"
         "Build_PipelineSupport.Build_PipelineSupport_C"),
    TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupport/"
         "Build_PipeSupportStackable.Build_PipeSupportStackable_C"),
    TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupportWall/"
         "Build_PipelineSupportWall.Build_PipelineSupportWall_C"),
    TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupportWallHole/"
         "Build_PipelineSupportWallHole.Build_PipelineSupportWallHole_C"),
};
constexpr int32 kSupportParentCount = UE_ARRAY_COUNT(kFluidSupportParentPaths);

TWeakObjectPtr<UClass> GSupportParentClasses[kSupportParentCount];

UClass* GetSupportParentClass(int32 Index) {
  if (UClass* Cached = GSupportParentClasses[Index].Get()) {
    return Cached;
  }

  // ResolveClass = FindObject, no load. Class absent from memory -> no such
  // support exists in world -> IsA would be false anyway.
  UClass* Resolved = FSoftClassPath(kFluidSupportParentPaths[Index]).ResolveClass();
  GSupportParentClasses[Index] = Resolved;
  return Resolved;
}

void AddUniqueSupport(TArray<AFGBuildable*>& Out, AFGBuildable* Candidate) {
  if (!IsValid(Candidate)) {
    return;
  }
  Out.AddUnique(Candidate);
}

void PruneCacheDead() {
  for (auto It = GPipeToSupports.CreateIterator(); It; ++It) {
    if (!It.Key().IsValid()) {
      It.RemoveCurrent();
      continue;
    }
    TArray<TWeakObjectPtr<AFGBuildable>>& List = It.Value();
    for (int32 i = List.Num() - 1; i >= 0; --i) {
      if (!List[i].IsValid()) {
        List.RemoveAtSwap(i);
      }
    }
  }
  for (auto It = GSupportToPipe.CreateIterator(); It; ++It) {
    if (!It.Key().IsValid() || !It.Value().IsValid()) {
      It.RemoveCurrent();
    }
  }
  for (auto It = GScannedNoSupports.CreateIterator(); It; ++It) {
    if (!It->IsValid()) {
      It.RemoveCurrent();
    }
  }
}

void MaybePruneCacheDead() {
  if (++GOpsSincePrune < PruneOpInterval) {
    return;
  }
  GOpsSincePrune = 0;
  PruneCacheDead();
}

UFGPipeConnectionComponentBase* FirstPipeSnapConn(AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return nullptr;
  }

  TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Buildable);
  for (UFGPipeConnectionComponentBase* Conn : Conns) {
    if (IsValid(Conn)) {
      return Conn;
    }
  }
  return nullptr;
}

FVector SupportSnapWorldLoc(AFGBuildable* Support) {
  if (UFGPipeConnectionComponentBase* Conn = FirstPipeSnapConn(Support)) {
    return Conn->GetConnectorLocation(/*withClearance=*/false);
  }
  return Support->GetActorLocation();
}

bool IsMidSpanTouching(AFGBuildablePipeline* Pipe, AFGBuildable* Support) {
  if (!IsValid(Pipe) || !IsValid(Support)) {
    return false;
  }

  const FVector SnapLoc = SupportSnapWorldLoc(Support);
  const float Offset = Pipe->FindOffsetClosestToLocation(SnapLoc);
  FVector OnSpline = FVector::ZeroVector;
  FVector Dir = FVector::ZeroVector;
  Pipe->GetLocationAndDirectionAtOffset(Offset, OnSpline, Dir);
  return FVector::DistSquared(SnapLoc, OnSpline) <= (TouchRadiusUu * TouchRadiusUu);
}

AFGBuildablePipeline* PipeFromConn(UFGPipeConnectionComponentBase* Conn) {
  if (!IsValid(Conn)) {
    return nullptr;
  }
  UFGPipeConnectionComponentBase* Other = Conn->GetConnection();
  if (!IsValid(Other)) {
    return nullptr;
  }
  return Cast<AFGBuildablePipeline>(Other->GetOwner());
}

void ScanSupportsTouchingPipe(AFGBuildablePipeline* Pipe, TArray<AFGBuildable*>& OutSupports) {
  OutSupports.Reset();
  if (!IsValid(Pipe)) {
    return;
  }

  auto TryAddFromPipeConn = [&](UFGPipeConnectionComponentBase* Conn) {
    if (!IsValid(Conn)) {
      return;
    }
    UFGPipeConnectionComponentBase* Other = Conn->GetConnection();
    if (!IsValid(Other)) {
      return;
    }
    if (AFGBuildable* Owner = Cast<AFGBuildable>(Other->GetOwner())) {
      if (IsPipeSupport(Owner)) {
        AddUniqueSupport(OutSupports, Owner);
      }
    }
  };

  TryAddFromPipeConn(Pipe->GetConnection0());
  TryAddFromPipeConn(Pipe->GetConnection1());

  UWorld* World = Pipe->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  auto ConsiderSupport = [&](AFGBuildable* Candidate) {
    if (!IsPipeSupport(Candidate)) {
      return;
    }

    TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Candidate);
    for (UFGPipeConnectionComponentBase* Conn : Conns) {
      if (PipeFromConn(Conn) == Pipe) {
        AddUniqueSupport(OutSupports, Candidate);
        return;
      }
    }

    if (IsMidSpanTouching(Pipe, Candidate)) {
      AddUniqueSupport(OutSupports, Candidate);
    }
  };

  for (TActorIterator<AFGBuildable> It(World); It; ++It) {
    ConsiderSupport(*It);
  }
}

TWeakObjectPtr<UWorld> GSeededWorld;
} // namespace

void SeedFromWorld(UWorld* World) {
  if (!IsValid(World) || GSeededWorld.Get() == World) {
    return;
  }
  GSeededWorld = World;

  // Single world pass replaces per-pipe TActorIterator fallbacks: resolve each
  // support's pipe through its snap connection (free), spline-project only
  // against the connection-adjacent pipe. Pipes left unlinked get the negative
  // flag lazily on their first Collect (which now cannot world-scan again).
  int32 Supports = 0;
  int32 Linked = 0;
  TArray<AFGBuildablePipeline*> Pipes;
  for (TActorIterator<AFGBuildable> It(World); It; ++It) {
    AFGBuildable* Candidate = *It;
    if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Candidate)) {
      Pipes.Add(Pipe);
      continue;
    }
    if (!IsPipeSupport(Candidate)) {
      continue;
    }

    ++Supports;
    bool bConnLinked = false;
    TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Candidate);
    for (UFGPipeConnectionComponentBase* Conn : Conns) {
      if (AFGBuildablePipeline* Pipe = PipeFromConn(Conn)) {
        RememberLink(Pipe, Candidate);
        bConnLinked = true;
        ++Linked;
        break;
      }
    }

    // Mid-span support (wall hole etc.): pipeline-only proximity scan, once,
    // at seed time — cheaper than letting its pipe world-scan later.
    if (!bConnLinked && FindTouchedPipe(Candidate)) {
      ++Linked;
    }
  }

  // Every support in the world is linked now, so any pipe without a cache
  // entry is provably supportless: negative-flag them all so no pipe ever
  // pays a first-Collect world scan. New builds clear flags via the hooks.
  int32 Flagged = 0;
  for (AFGBuildablePipeline* Pipe : Pipes) {
    if (!GPipeToSupports.Contains(Pipe)) {
      GScannedNoSupports.Add(Pipe);
      ++Flagged;
    }
  }

  UE_LOG(LogPipelineColor, Log,
         TEXT("%s support seed: %d support(s), %d linked, %d pipe(s) flagged supportless"),
         PIPELINECOLOR_LOG_PREFIX, Supports, Linked, Flagged);
}

void RememberLink(AFGBuildablePipeline* Pipe, AFGBuildable* Support) {
  if (!IsValid(Pipe) || !IsValid(Support)) {
    return;
  }
  MaybePruneCacheDead();
  GScannedNoSupports.Remove(Pipe);
  GSupportToPipe.FindOrAdd(Support) = Pipe;
  TArray<TWeakObjectPtr<AFGBuildable>>& List = GPipeToSupports.FindOrAdd(Pipe);
  List.AddUnique(Support);
}

void InvalidateBuildable(AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return;
  }
  MaybePruneCacheDead();

  if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable)) {
    if (TArray<TWeakObjectPtr<AFGBuildable>>* List = GPipeToSupports.Find(Pipe)) {
      for (const TWeakObjectPtr<AFGBuildable>& Weak : *List) {
        GSupportToPipe.Remove(Weak);
      }
    }
    GPipeToSupports.Remove(Pipe);
    GScannedNoSupports.Remove(Pipe);
    return;
  }

  if (TWeakObjectPtr<AFGBuildablePipeline>* PipeWeak = GSupportToPipe.Find(Buildable)) {
    if (AFGBuildablePipeline* Pipe = PipeWeak->Get()) {
      if (TArray<TWeakObjectPtr<AFGBuildable>>* List = GPipeToSupports.Find(Pipe)) {
        List->Remove(Buildable);
      }
      // Sibling supports may remain; next Collect rescans once and either
      // repopulates the list or negative-flags the pipe.
      GScannedNoSupports.Remove(Pipe);
    }
    GSupportToPipe.Remove(Buildable);
  }
}

bool IsPipeSupport(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable) || Buildable->IsA<AFGBuildablePipeline>() ||
      Buildable->IsA<AFGBuildablePipelineAttachment>()) {
    return false;
  }

  for (int32 Index = 0; Index < kSupportParentCount; ++Index) {
    UClass* Parent = GetSupportParentClass(Index);
    if (Parent && Buildable->IsA(Parent)) {
      return true;
    }
  }
  return false;
}

AFGBuildablePipeline* FindTouchedPipe(AFGBuildable* Support) {
  if (!IsPipeSupport(Support)) {
    return nullptr;
  }

  if (const TWeakObjectPtr<AFGBuildablePipeline>* Cached = GSupportToPipe.Find(Support)) {
    if (AFGBuildablePipeline* Pipe = Cached->Get()) {
      return Pipe;
    }
  }

  TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Support);
  for (UFGPipeConnectionComponentBase* Conn : Conns) {
    if (AFGBuildablePipeline* Pipe = PipeFromConn(Conn)) {
      RememberLink(Pipe, Support);
      return Pipe;
    }
  }

  UWorld* World = Support->GetWorld();
  if (!IsValid(World)) {
    return nullptr;
  }

  AFGBuildablePipeline* Best = nullptr;
  float BestDistSq = TouchRadiusUu * TouchRadiusUu;
  const FVector SnapLoc = SupportSnapWorldLoc(Support);

  auto Consider = [&](AFGBuildablePipeline* Pipe) {
    if (!IsValid(Pipe)) {
      return;
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
  };

  for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It) {
    Consider(*It);
  }

  if (Best) {
    RememberLink(Best, Support);
  }
  return Best;
}

void CollectSupportsTouchingPipe(AFGBuildablePipeline* Pipe, TArray<AFGBuildable*>& OutSupports) {
  OutSupports.Reset();
  if (!IsValid(Pipe)) {
    return;
  }

  MaybePruneCacheDead();
  if (TArray<TWeakObjectPtr<AFGBuildable>>* List = GPipeToSupports.Find(Pipe)) {
    for (const TWeakObjectPtr<AFGBuildable>& Weak : *List) {
      if (AFGBuildable* Support = Weak.Get()) {
        AddUniqueSupport(OutSupports, Support);
      }
    }
    if (OutSupports.Num() > 0) {
      return;
    }
  }

  // World scan is the expensive last resort. Negative cache keeps supportless
  // pipes from re-scanning the world on every revisit.
  if (GScannedNoSupports.Contains(Pipe)) {
    return;
  }

  ScanSupportsTouchingPipe(Pipe, OutSupports);
  if (OutSupports.Num() == 0) {
    GScannedNoSupports.Add(Pipe);
    return;
  }

  for (AFGBuildable* Support : OutSupports) {
    RememberLink(Pipe, Support);
  }
}
} // namespace FPipeSupportTouch
