// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Store/APCSwatchStoreSubsystem.h"

#include "Appearance/FPCAppearanceSpec.h"
#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "Core/FPCWorldGate.h"
#include "FGFactoryColoringTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PipelineColorLog.h"
#include "PipelineColorRootInstanceModule.h"
#include "Resources/FGItemDescriptor.h"
#include "Swatches/FPCDynamicSwatchRegistry.h"
#include "Swatches/UPCSwatchDescs.h"

namespace {
constexpr int32 GStoreSchemaOnlyCustom = 4;

void FillNeutralOrFallback(FPCSwatchEntry& Entry, FName Key, const FPCAppearanceSpec& Spec) {
  Entry.Key = Key;
  Entry.Primary = Spec.PrimaryColor;
  Entry.Secondary = Spec.SecondaryColor;
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

void FillFromDynamic(FPCSwatchEntry& Entry, const FPCDynamicSwatchEntry& Dyn) {
  Entry.Key = Dyn.CatalogKey;
  Entry.Primary = Dyn.Primary;
  Entry.Secondary = FLinearColor::FromSRGBColor(FColor(0x2A, 0x2A, 0x2A, 255));
  Entry.PaintFinishPath = FPCFluidAppearanceCatalog::GetFinishPath(Dyn.Finish);
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
  StoreSchema = GStoreSchemaOnlyCustom;
}

void APCSwatchStoreSubsystem::PostLoadGame_Implementation(int32 /*SaveVersion*/,
                                                          int32 /*GameVersion*/) {
  if (StoreSchema < GStoreSchemaOnlyCustom) {
    UE_LOG(LogPipelineColor, Log, TEXT("%s store schema %d->%d nuke (only-custom)"),
           PIPELINECOLOR_LOG_PREFIX, StoreSchema, GStoreSchemaOnlyCustom);
    Entries.Reset();
    KeyToIndex.Reset();
    StoreSchema = GStoreSchemaOnlyCustom;
    if (HasAuthority()) {
      EntryChanged.Broadcast(NAME_None);
      ForceNetUpdate();
    }
    return;
  }

  RebuildMaps();

  FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
  Catalog.EnsureLoaded();
  FillEmptyPaintFinishPaths(*this, Catalog);
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
  APCSwatchStoreSubsystem* Spawned =
      World->SpawnActor<APCSwatchStoreSubsystem>(StaticClass(), FTransform::Identity, Params);
  if (Spawned) {
    Spawned->StoreSchema = GStoreSchemaOnlyCustom;
  }
  return Spawned;
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

  StoreSchema = GStoreSchemaOnlyCustom;
  EntryChanged.Broadcast(Key);
  ForceNetUpdate();
}

void APCSwatchStoreSubsystem::SetFromSlot(FName Key, const FFactoryCustomizationColorSlot& Slot) {
  FPCSwatchEntry Entry;
  Entry.FromSlot(Key, Slot);
  Set(Key, Entry);
}

void APCSwatchStoreSubsystem::SeedMissingFromCatalog() {
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
  FillNeutralOrFallback(Entry, FName(TEXT("Neutral")), Spec);

  const int32 Idx = FindIndex(Entry.Key);
  if (Idx == INDEX_NONE) {
    return;
  }
  Entries[Idx] = Entry;
  EntryChanged.Broadcast(Entry.Key);
  RebuildMaps();
  ForceNetUpdate();
  UE_LOG(LogPipelineColor, Log, TEXT("%s Neutral reseeded Matte"), PIPELINECOLOR_LOG_PREFIX);
}

void APCSwatchStoreSubsystem::ReseedAllFromCatalog() {
  if (!HasAuthority()) {
    return;
  }
  Entries.Reset();
  KeyToIndex.Reset();
  StoreSchema = GStoreSchemaOnlyCustom;
  EntryChanged.Broadcast(NAME_None);
  ForceNetUpdate();
  UE_LOG(LogPipelineColor, Log, TEXT("%s store customs cleared (!pc default)"),
         PIPELINECOLOR_LOG_PREFIX);
}

bool APCSwatchStoreSubsystem::ReseedKeyFromCatalog(FName Key) {
  if (Key.IsNone() || !HasAuthority()) {
    return false;
  }
  if (FindIndex(Key) == INDEX_NONE) {
    return false;
  }

  UWorld* World = GetWorld();
  FPCDynamicSwatchRegistry::Ensure(World, UPipelineColorRootInstanceModule::Find(World));

  FPCDynamicSwatchEntry Dyn;
  if (FPCDynamicSwatchRegistry::TryGetByKey(Key, Dyn)) {
    FPCSwatchEntry Entry;
    FillFromDynamic(Entry, Dyn);
    Set(Key, Entry);
    return true;
  }

  FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
  Catalog.EnsureLoaded();
  FPCAppearanceSpec Spec;
  if (!Catalog.ResolveByKey(Key, Spec)) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s ReseedKey unresolved %s"), PIPELINECOLOR_LOG_PREFIX,
           *Key.ToString());
    return false;
  }

  FPCSwatchEntry Entry;
  FillNeutralOrFallback(Entry, Key, Spec);
  Set(Key, Entry);
  return true;
}

bool APCSwatchStoreSubsystem::ReseedKeyColorsFromCatalog(FName Key) {
  if (Key.IsNone() || !HasAuthority()) {
    return false;
  }

  UWorld* World = GetWorld();
  FPCDynamicSwatchRegistry::Ensure(World, UPipelineColorRootInstanceModule::Find(World));

  FPCDynamicSwatchEntry Dyn;
  if (FPCDynamicSwatchRegistry::TryGetByKey(Key, Dyn)) {
    FPCSwatchEntry Entry;
    if (!TryGet(Key, Entry)) {
      return false;
    }
    Entry.Primary = Dyn.Primary;
    Entry.Secondary = FLinearColor::FromSRGBColor(FColor(0x2A, 0x2A, 0x2A, 255));
    Set(Key, Entry);
    return true;
  }

  FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
  Catalog.EnsureLoaded();
  FPCAppearanceSpec Spec;
  if (!Catalog.ResolveByKey(Key, Spec)) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s ReseedKeyColors unresolved %s"),
           PIPELINECOLOR_LOG_PREFIX, *Key.ToString());
    return false;
  }

  FPCSwatchEntry Entry;
  if (!TryGet(Key, Entry)) {
    return false;
  }
  Entry.Primary = Spec.PrimaryColor;
  Entry.Secondary = Spec.SecondaryColor;
  Set(Key, Entry);
  return true;
}
