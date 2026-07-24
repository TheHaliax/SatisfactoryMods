// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Store/APCSwatchStoreSubsystem.h"

#include "Appearance/FPCAppearanceSpec.h"
#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "Appearance/FPCFluidRoster.h"
#include "Core/FPCWorldGate.h"
#include "FGFactoryColoringTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PipelineColorLog.h"
#include "Resources/FGItemDescriptor.h"
#include "Swatches/UPCSwatchDescs.h"
#include "UObject/SoftObjectPath.h"

namespace {
constexpr int32 GStoreSchemaPaintFinishPath = 2;

void FillEntryFromSpec(FPCSwatchEntry& Entry, FName Key, const FPCAppearanceSpec& Spec) {
  Entry.Key = Key;
  Entry.Primary = Spec.PrimaryColor;
  Entry.Secondary = Spec.SecondaryColor;
  // Path string only — Spec.PaintFinish may be a dangling cached UClass*.
  Entry.PaintFinishPath = FPCFluidAppearanceCatalog::GetFinishPath(
      FPCFluidAppearanceCatalog::Get().FinishKindForKey(Key));
}

void FillEmptyPaintFinishPaths(APCSwatchStoreSubsystem& Store, FPCFluidAppearanceCatalog& Catalog) {
  bool bMigrated = false;
  for (FPCSwatchEntry& Entry : Store.Entries) {
    if (!Entry.PaintFinishPath.IsEmpty()) {
      continue;
    }
    Entry.PaintFinishPath =
        FPCFluidAppearanceCatalog::GetFinishPath(Catalog.FinishKindForKey(Entry.Key));
    bMigrated = true;
  }

  if (bMigrated) {
    UE_LOG(LogPipelineColor, Log, TEXT("%s store migrated to PaintFinishPath"),
           PIPELINECOLOR_LOG_PREFIX);
  }
}
} // namespace

APCSwatchStoreSubsystem::APCSwatchStoreSubsystem() {
  PrimaryActorTick.bCanEverTick = false;
  bReplicates = true;
  bAlwaysRelevant = true;
}

void APCSwatchStoreSubsystem::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(APCSwatchStoreSubsystem, Entries);
}

void APCSwatchStoreSubsystem::PreSaveGame_Implementation(int32 /*SaveVersion*/,
                                                         int32 /*GameVersion*/) {
  StoreSchema = GStoreSchemaPaintFinishPath;
}

void APCSwatchStoreSubsystem::PostLoadGame_Implementation(int32 /*SaveVersion*/,
                                                          int32 /*GameVersion*/) {
  RebuildMaps();

  FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
  Catalog.EnsureLoaded();
  FillEmptyPaintFinishPaths(*this, Catalog);

  if (HasAuthority()) {
    SeedMissingFromCatalog();
  }

  StoreSchema = GStoreSchemaPaintFinishPath;
}

void APCSwatchStoreSubsystem::OnRep_Entries() {
  RebuildMaps();
}

APCSwatchStoreSubsystem* APCSwatchStoreSubsystem::Find(UWorld* World) {
  if (!IsValid(World)) {
    return nullptr;
  }
  return Cast<APCSwatchStoreSubsystem>(UGameplayStatics::GetActorOfClass(World, StaticClass()));
}

APCSwatchStoreSubsystem* APCSwatchStoreSubsystem::GetOrCreate(UWorld* World) {
  if (!FPCWorldGate::IsGameplayWorld(World) || World->GetNetMode() == NM_Client) {
    return Find(World);
  }

  if (APCSwatchStoreSubsystem* Existing = Find(World)) {
    return Existing;
  }

  FActorSpawnParameters Params;
  Params.Name = TEXT("PipelineColorSwatchStore");
  Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  return World->SpawnActor<APCSwatchStoreSubsystem>(StaticClass(), FTransform::Identity, Params);
}

bool APCSwatchStoreSubsystem::IsPCCustomization(
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  return Swatch && Swatch->IsChildOf(UPCSwatchDescBase::StaticClass());
}

void APCSwatchStoreSubsystem::RebuildMaps() {
  KeyToIndex.Reset();
  for (int32 i = 0; i < Entries.Num(); ++i) {
    if (!Entries[i].Key.IsNone()) {
      KeyToIndex.Add(Entries[i].Key, i);
    }
  }
}

int32 APCSwatchStoreSubsystem::FindIndex(FName Key) const {
  if (const int32* Idx = KeyToIndex.Find(Key)) {
    return *Idx;
  }
  return INDEX_NONE;
}

FName APCSwatchStoreSubsystem::KeyFromSwatchStatic(
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  if (!Swatch) {
    return NAME_None;
  }
  return UPCSwatchDescBase::GetCatalogKey(Swatch);
}

FName APCSwatchStoreSubsystem::KeyFromSwatch(
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) const {
  return KeyFromSwatchStatic(Swatch);
}

bool APCSwatchStoreSubsystem::TryGet(FName Key, FPCSwatchEntry& Out) const {
  const int32 Idx = FindIndex(Key);
  if (Idx == INDEX_NONE) {
    return false;
  }
  Out = Entries[Idx];
  return true;
}

bool APCSwatchStoreSubsystem::TryGetBySwatch(
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch, FPCSwatchEntry& Out) const {
  return TryGet(KeyFromSwatch(Swatch), Out);
}

void APCSwatchStoreSubsystem::Set(FName Key, const FPCSwatchEntry& Entry) {
  if (Key.IsNone() || !HasAuthority()) {
    return;
  }

  FPCSwatchEntry Copy = Entry;
  Copy.Key = Key;

  const int32 Idx = FindIndex(Key);
  if (Idx == INDEX_NONE) {
    KeyToIndex.Add(Key, Entries.Num());
    Entries.Add(Copy);
  } else {
    Entries[Idx] = Copy;
  }

  EntryChanged.Broadcast(Key);
  ForceNetUpdate();
}

void APCSwatchStoreSubsystem::SetFromSlot(FName Key, const FFactoryCustomizationColorSlot& Slot) {
  FPCSwatchEntry Entry;
  Entry.FromSlot(Key, Slot);
  Set(Key, Entry);
}

void APCSwatchStoreSubsystem::SeedMissingFromCatalog() {
  if (!HasAuthority()) {
    return;
  }

  RebuildMaps();
  FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
  Catalog.EnsureLoaded();

  auto EnsureKey = [this](FName Key, const FPCAppearanceSpec& Spec) {
    if (FindIndex(Key) != INDEX_NONE) {
      return;
    }
    FPCSwatchEntry Entry;
    FillEntryFromSpec(Entry, Key, Spec);
    KeyToIndex.Add(Key, Entries.Num());
    Entries.Add(Entry);
  };

  {
    FPCAppearanceSpec Neutral;
    if (Catalog.ResolveByKey(FName(TEXT("Neutral")), Neutral)) {
      EnsureKey(FName(TEXT("Neutral")), Neutral);
    }
  }

  for (const FPCFluidRosterRow& Row : FPCFluidRoster::FluidRows()) {
    if (FindIndex(Row.Stem) != INDEX_NONE) {
      continue;
    }
    if (Row.Section != EPCFluidRosterSection::Default && !FPCFluidRoster::SoftDescPresent(Row)) {
      continue;
    }
    FPCAppearanceSpec Spec;
    if (!Catalog.ResolveByKey(Row.Stem, Spec)) {
      UE_LOG(LogPipelineColor, Warning, TEXT("%s seed skip unresolved key %s"),
             PIPELINECOLOR_LOG_PREFIX, *Row.Stem.ToString());
      continue;
    }
    EnsureKey(Row.Stem, Spec);
  }

  {
    FPCAppearanceSpec Fallback;
    if (Catalog.ResolveByKey(FName(TEXT("Fallback")), Fallback)) {
      EnsureKey(FName(TEXT("Fallback")), Fallback);
    }
  }

  RebuildMaps();
  UE_LOG(LogPipelineColor, Log, TEXT("%s store seeded (%d entries)"), PIPELINECOLOR_LOG_PREFIX,
         Entries.Num());
}

void APCSwatchStoreSubsystem::ForceReseedNeutralMatte() {
  if (!HasAuthority()) {
    return;
  }

  RebuildMaps();
  FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
  Catalog.EnsureLoaded();

  FPCAppearanceSpec Spec;
  Catalog.ResolveByKey(FName(TEXT("Neutral")), Spec);

  FPCSwatchEntry Entry;
  FillEntryFromSpec(Entry, FName(TEXT("Neutral")), Spec);

  const int32 Idx = FindIndex(Entry.Key);
  if (Idx == INDEX_NONE) {
    KeyToIndex.Add(Entry.Key, Entries.Num());
    Entries.Add(Entry);
  } else {
    Entries[Idx] = Entry;
  }
  EntryChanged.Broadcast(Entry.Key);
  RebuildMaps();
  UE_LOG(LogPipelineColor, Log, TEXT("%s Neutral reseeded Matte"), PIPELINECOLOR_LOG_PREFIX);
}

void APCSwatchStoreSubsystem::ReseedAllFromCatalog() {
  if (!HasAuthority()) {
    return;
  }

  FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
  Catalog.EnsureLoaded();

  Entries.Reset();
  KeyToIndex.Reset();

  auto WriteKey = [this](FName Key, const FPCAppearanceSpec& Spec) {
    FPCSwatchEntry Entry;
    FillEntryFromSpec(Entry, Key, Spec);
    KeyToIndex.Add(Key, Entries.Num());
    Entries.Add(Entry);
  };

  {
    FPCAppearanceSpec Neutral;
    if (Catalog.ResolveByKey(FName(TEXT("Neutral")), Neutral)) {
      WriteKey(FName(TEXT("Neutral")), Neutral);
    }
  }

  for (const FPCFluidRosterRow& Row : FPCFluidRoster::FluidRows()) {
    if (Row.Section != EPCFluidRosterSection::Default && !FPCFluidRoster::SoftDescPresent(Row)) {
      continue;
    }
    FPCAppearanceSpec Spec;
    if (!Catalog.ResolveByKey(Row.Stem, Spec)) {
      UE_LOG(LogPipelineColor, Warning, TEXT("%s reseed skip unresolved key %s"),
             PIPELINECOLOR_LOG_PREFIX, *Row.Stem.ToString());
      continue;
    }
    WriteKey(Row.Stem, Spec);
  }

  {
    FPCAppearanceSpec Fallback;
    if (Catalog.ResolveByKey(FName(TEXT("Fallback")), Fallback)) {
      WriteKey(FName(TEXT("Fallback")), Fallback);
    }
  }

  RebuildMaps();
  EntryChanged.Broadcast(NAME_None);
  ForceNetUpdate();
  UE_LOG(LogPipelineColor, Log, TEXT("%s store reseeded all (%d entries)"),
         PIPELINECOLOR_LOG_PREFIX, Entries.Num());
}
