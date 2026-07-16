// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Session/UPCWorldSubsystem.h"

#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "Appearance/FPCMetallicColorCorrection.h"
#include "Appearance/FPCMetallicFlag.h"
#include "Application/FCustomizationApplicator.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Config/FPCPipelineColorModConfig.h"
#include "Content/FPipeFluidKeyResolver.h"
#include "Content/FPipeSupportTouch.h"
#include "Core/FPCWorldGate.h"
#include "EngineUtils.h"
#include "FGBuildablePipelineFlowIndicator.h"
#include "FGBuildableSubsystem.h"
#include "FGFactoryColoringTypes.h"
#include "PipelineColorLog.h"
#include "Store/APCSwatchStoreSubsystem.h"
#include "Swatches/FPCSwatchPublisher.h"
#include "Swatches/UPCFinishDescs.h"
#include "Target/FBuildableColorTarget.h"

UPCWorldSubsystem* UPCWorldSubsystem::Get(UWorld* World) {
  return IsValid(World) ? World->GetSubsystem<UPCWorldSubsystem>() : nullptr;
}

void UPCWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
  Super::Initialize(Collection);

  UWorld* World = GetWorld();
  bAuthority = false;
  bWorldReadyDone = false;
  if (FPCWorldGate::IsGameplayWorld(World) && World->GetNetMode() != NM_Client) {
    bAuthority = true;
    FPCFluidAppearanceCatalog::Get().EnsureLoaded();
  }
}

void UPCWorldSubsystem::Deinitialize() {
  if (APCSwatchStoreSubsystem* Store = BoundStore.Get()) {
    Store->OnEntryChanged().Remove(StoreChangedHandle);
  }
  StoreChangedHandle.Reset();
  BoundStore.Reset();

  if (ConfigChangedHandle.IsValid()) {
    FPCPipelineColorModConfig::OnConfigChanged().Remove(ConfigChangedHandle);
    ConfigChangedHandle.Reset();
  }

  Dirty.Reset();
  WatchList.Reset();
  WatchMembership.Reset();
  LastApplied.Reset();
  bWorldReadyDone = false;
  Super::Deinitialize();
}

void UPCWorldSubsystem::DirtyAllWatched() {
  if (!bAuthority) {
    return;
  }

  for (const TWeakObjectPtr<AFGBuildable>& Weak : WatchList) {
    if (AFGBuildable* Buildable = Weak.Get()) {
      Dirty.Add(Buildable);
    }
  }
}

void UPCWorldSubsystem::BindSwatchStore(UWorld* World) {
  APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
  if (Store && BoundStore.Get() != Store) {
    if (APCSwatchStoreSubsystem* Prev = BoundStore.Get()) {
      Prev->OnEntryChanged().Remove(StoreChangedHandle);
    }

    BoundStore = Store;
    StoreChangedHandle =
        Store->OnEntryChanged().AddUObject(this, &UPCWorldSubsystem::OnSwatchEntryChanged);
  }

  if (!ConfigChangedHandle.IsValid()) {
    ConfigChangedHandle = FPCPipelineColorModConfig::OnConfigChanged().AddUObject(
        this, &UPCWorldSubsystem::OnMetallicConfigChanged);
  }
}

void UPCWorldSubsystem::OnSwatchEntryChanged(FName /*Key*/) {
  DirtyAllWatched();
}

void UPCWorldSubsystem::OnMetallicConfigChanged() {
  DirtyAllWatched();
}

void UPCWorldSubsystem::ApplyStoreOverlay(FPCAppearanceSpec& Spec) const {
  APCSwatchStoreSubsystem* Store = BoundStore.Get();
  if (!Store && GetWorld()) {
    Store = APCSwatchStoreSubsystem::Find(GetWorld());
  }
  if (!Store || Spec.CatalogKey.IsNone()) {
    return;
  }

  FPCSwatchEntry Entry;
  if (!Store->TryGet(Spec.CatalogKey, Entry)) {
    return;
  }

  Spec.PrimaryColor = Entry.Primary;
  Spec.SecondaryColor = Entry.Secondary;
  if (Entry.PaintFinish.IsValid()) {
    if (TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> Loaded =
            Entry.PaintFinish.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>()) {
      Spec.PaintFinish = Loaded;
    }
  }
}

namespace {
// DummyHeaders GetFlowIndicator() stubs to null — read SaveGame UPROPERTY.
AFGBuildablePipelineFlowIndicator* ResolveFlowIndicator(AFGBuildablePipeline* Pipe) {
  if (!IsValid(Pipe)) {
    return nullptr;
  }

  if (AFGBuildablePipelineFlowIndicator* Ind = Pipe->GetFlowIndicator()) {
    return Ind;
  }

  static FObjectProperty* Prop = FindFProperty<FObjectProperty>(
      AFGBuildablePipeline::StaticClass(), TEXT("mFlowIndicator"));
  if (!Prop) {
    return nullptr;
  }
  return Cast<AFGBuildablePipelineFlowIndicator>(
      Prop->GetObjectPropertyValue_InContainer(Pipe));
}

void FinalizePaintFinishSpec(FPCAppearanceSpec& Spec) {
  const bool bMetallic = FPCPipelineColorModConfig::IsMetallicForKey(Spec.CatalogKey);

  if (!bMetallic) {
    if (Spec.CatalogKey == FName(TEXT("Neutral"))) {
      Spec.PaintFinish = FPCFluidAppearanceCatalog::Get().GetFinishClass(EPCPaintFinishKind::Matte);
      Spec.RoughnessValue = FPCMetallicColorCorrection::NeutralMatteRoughness;
      Spec.bOverrideRoughness = true;
      return;
    }

    Spec.bOverrideRoughness = false;
    if (!Spec.PaintFinish || Spec.PaintFinish == UPCFinish_MetallicColor::StaticClass()) {
      Spec.PaintFinish =
          FPCFluidAppearanceCatalog::Get().GetFinishClass(EPCPaintFinishKind::Default);
    }
    return;
  }

  Spec.PaintFinish = UPCFinish_MetallicColor::StaticClass();
  FPCMetallicColorCorrection::Apply(Spec);
}

void PaintSupportsMatchingPipe(AFGBuildablePipeline* Pipe, const FPCAppearanceSpec& Spec,
                               TMap<TWeakObjectPtr<AFGBuildable>, FPCAppearanceSpec>& LastApplied) {
  if (!IsValid(Pipe)) {
    return;
  }

  TArray<AFGBuildable*> Supports;
  FPipeSupportTouch::CollectSupportsTouchingPipe(Pipe, Supports);
  for (AFGBuildable* Support : Supports) {
    if (!IsValid(Support) || !Support->HasAuthority()) {
      continue;
    }

    FPipeSupportTouch::RememberLink(Pipe, Support);

    if (const FPCAppearanceSpec* Prev = LastApplied.Find(Support)) {
      if (Prev->EqualsApplied(Spec)) {
        continue;
      }
    }

    FBuildableColorTarget Target(Support);
    if (Target.Apply(Spec)) {
      LastApplied.FindOrAdd(Support) = Spec;
    }
  }
}
}  // namespace

bool UPCWorldSubsystem::IsTickable() const {
  return bAuthority && FPCWorldGate::IsGameplayWorld(GetWorld());
}

TStatId UPCWorldSubsystem::GetStatId() const {
  RETURN_QUICK_DECLARE_CYCLE_STAT(UPCWorldSubsystem, STATGROUP_Tickables);
}

void UPCWorldSubsystem::OnWorldReady(UWorld* World) {
  if (!FPCWorldGate::IsGameplayWorld(World) || World->GetNetMode() == NM_Client) {
    return;
  }

  if (bWorldReadyDone) {
    return;
  }

  bWorldReadyDone = true;
  bAuthority = true;

  FPCFluidAppearanceCatalog::Get().EnsureLoaded();
  BindSwatchStore(World);
  FPCSwatchPublisher::PublishForWorld(World);
  ScanWorld();

  UE_LOG(LogPipelineColor, Log, TEXT("%s OnWorldReady"), PIPELINECOLOR_LOG_PREFIX);
}

void UPCWorldSubsystem::Enqueue(AFGBuildable* Buildable) {
  if (!bAuthority || !FPipeFluidKeyResolver::IsPipeColorTarget(Buildable)) {
    return;
  }

  Dirty.Add(Buildable);

  const TWeakObjectPtr<AFGBuildable> WeakBuildable(Buildable);
  if (!WatchMembership.Contains(WeakBuildable)) {
    WatchMembership.Add(WeakBuildable);
    WatchList.Add(WeakBuildable);
  }
}

void UPCWorldSubsystem::InvalidateApplied(AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return;
  }
  LastApplied.Remove(Buildable);
}

void UPCWorldSubsystem::ProcessNow(AFGBuildable* Buildable) {
  if (!bAuthority || !IsValid(Buildable) || !Buildable->HasAuthority()) {
    return;
  }

  if (!FPipeFluidKeyResolver::IsPipeColorTarget(Buildable)) {
    return;
  }

  static FPipeFluidKeyResolver Resolver;
  TSubclassOf<UFGItemDescriptor> Fluid;
  bool bEmpty = true;
  Resolver.Resolve(Buildable, Fluid, bEmpty);

  FPCAppearanceSpec Spec;
  FPCFluidAppearanceCatalog::Get().EnsureLoaded();
  FPCFluidAppearanceCatalog::Get().Resolve(Fluid, bEmpty, Spec);
  ApplyStoreOverlay(Spec);
  FinalizePaintFinishSpec(Spec);

  const bool bSpecUnchanged = [&]() {
    if (const FPCAppearanceSpec* Prev = LastApplied.Find(Buildable)) {
      return Prev->EqualsApplied(Spec);
    }
    return false;
  }();

  if (!bSpecUnchanged) {
    FBuildableColorTarget Target(Buildable);
    if (Target.Apply(Spec)) {
      LastApplied.FindOrAdd(Buildable) = Spec;
    }
  }

  if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable)) {
    // Child meter may spawn after parent LastApplied — sync even when parent skip.
    if (AFGBuildablePipelineFlowIndicator* Ind = ResolveFlowIndicator(Pipe)) {
      FCustomizationApplicator::ApplyIfChanged(Ind, Spec);
    }
    PaintSupportsMatchingPipe(Pipe, Spec, LastApplied);
  }
}

void UPCWorldSubsystem::ScanWorld() {
  UWorld* World = GetWorld();
  if (!bAuthority || !FPCWorldGate::IsGameplayWorld(World)) {
    return;
  }

  int32 Found = 0;

  if (AFGBuildableSubsystem* BuildableSub = AFGBuildableSubsystem::Get(World)) {
    for (AFGBuildable* Buildable : BuildableSub->GetAllBuildablesRef()) {
      if (FPipeFluidKeyResolver::IsPipeColorTarget(Buildable)) {
        Enqueue(Buildable);
        ++Found;
      }
    }
  } else {
    for (TActorIterator<AFGBuildable> It(World); It; ++It) {
      if (FPipeFluidKeyResolver::IsPipeColorTarget(*It)) {
        Enqueue(*It);
        ++Found;
      }
    }
  }

  UE_LOG(LogPipelineColor, Log, TEXT("%s WorldReady scan enqueued %d"), PIPELINECOLOR_LOG_PREFIX,
         Found);
}

void UPCWorldSubsystem::DrainDirty(int32 MaxCount) {
  int32 Done = 0;
  TArray<TWeakObjectPtr<AFGBuildable>> Snapshot;
  Snapshot.Reserve(Dirty.Num());
  for (const TWeakObjectPtr<AFGBuildable>& Entry : Dirty) {
    Snapshot.Add(Entry);
  }
  Dirty.Reset();

  for (const TWeakObjectPtr<AFGBuildable>& Weak : Snapshot) {
    if (Done >= MaxCount) {
      Dirty.Add(Weak);
      continue;
    }

    if (AFGBuildable* Buildable = Weak.Get()) {
      ProcessNow(Buildable);
      ++Done;
    }
  }
}

void UPCWorldSubsystem::BudgetedEmptyPass(int32 MaxCount) {
  if (WatchList.Num() == 0) {
    return;
  }

  int32 Done = 0;
  const int32 Num = WatchList.Num();
  while (Done < MaxCount && Done < Num) {
    if (WatchCursor >= WatchList.Num()) {
      WatchCursor = 0;
    }

    TWeakObjectPtr<AFGBuildable> Weak = WatchList[WatchCursor];
    ++Done;

    AFGBuildable* Buildable = Weak.Get();
    if (!IsValid(Buildable)) {
      WatchMembership.Remove(Weak);
      WatchList.RemoveAt(WatchCursor);
      if (WatchCursor >= WatchList.Num()) {
        WatchCursor = 0;
      }
      continue;
    }

    ++WatchCursor;
    Enqueue(Buildable);
  }
}

void UPCWorldSubsystem::Tick(float DeltaTime) {
  if (!bAuthority || !FPCWorldGate::IsGameplayWorld(GetWorld())) {
    return;
  }

  DrainDirty(DirtyBatch);

  EmptyScanAccum += DeltaTime;
  if (EmptyScanAccum >= EmptyScanInterval) {
    EmptyScanAccum = 0.f;
    BudgetedEmptyPass(EmptyScanBatch);
  }
}
